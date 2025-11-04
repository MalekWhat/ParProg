#include <iostream>
#include <mpi.h>
#include <cstdlib>

using namespace std;

const int length = 1000000000;
const int num_processes = 5;

int main(int argc, char** argv) {

    MPI_Init(&argc, &argv);
    
    int rank;  //ID среди всех процессов: 0, 1, 2, 3 или 4
    int size;  //сколько всего процессов запущено
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (size != num_processes) {
        if (rank == 0) {
            cerr << "не запустились все процессы " << num_processes << "(запущено " << size << ")" << endl;
        }
        MPI_Finalize();
        return 1;
    }
    
    //каждый процесс выполняет ВЕСЬ код main()
    //но через if (rank == 0) мы делаем так, что только процесс 0 что-то делает
    unsigned char* mas = nullptr;
    
    if (rank == 0) {
        cout << "Процесс 0: создаю и наполняю массив" << endl;
        mas = new unsigned char[length];
        
        srand(2025);
        for (int i = 0; i < length; i++) {
            mas[i] = rand() & 0xff;
        }
        
    }
    
    int chunk_size = length / num_processes;
    int start = rank * chunk_size;  
    int end = (rank + 1) * chunk_size;
    
    int local_size = end - start;
    

    int buffer_size = local_size + 2;
    unsigned char* local_data = new unsigned char[buffer_size];
    
    if (rank == 0) {
        cout << "Процесс 0: раздаю данные" << endl;
        
        for (int i = 0; i < local_size; i++) {
            local_data[i+1] = mas[i];
        }
        local_data[0] = 0;  // Фиктивное значение
        
        //отправляем данные процессам 1, 2, 3, 4
        for (int proc = 1; proc < num_processes; proc++) {
            int proc_start = proc * chunk_size;
            
            // 1. &mas[proc_start] - ОТКУДА берём данные (адрес начала)
            // 2. chunk_size - СКОЛЬКО элементов отправляем
            // 3. MPI_UNSIGNED_CHAR - ТИП данных (беззнаковый char)
            // 4. proc - КОМУ отправляем (номер процесса-получателя)
            // 5. 0 - ТЕГ сообщения (метка, чтобы различать разные сообщения)
            // 6. MPI_COMM_WORLD - КОММУНИКАТОР (группа процессов)
            MPI_Send(&mas[proc_start], chunk_size, MPI_UNSIGNED_CHAR, 
                     proc, 0, MPI_COMM_WORLD);
        }
        
        delete[] mas;
        
    } else {        
        // 1. local_data - КУДА сохранить полученные данные
        // 2. local_size - СКОЛЬКО элементов ожидаем получить
        // 3. MPI_UNSIGNED_CHAR - ТИП данных
        // 4. 0 - ОТ КОГО ждём данные (от процесса 0)
        // 5. 0 - ТЕГ сообщения (должен совпадать с тегом в Send)
        // 6. MPI_COMM_WORLD - КОММУНИКАТОР
        // 7. MPI_STATUS_IGNORE - статус
        MPI_Recv(&local_data[1], local_size, MPI_UNSIGNED_CHAR, 
                 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    
    if (rank > 0) {
        //есть левый сосед(процесс rank-1)
        //отправляем ему мой ПЕРВЫЙ элемент(он станет его right_peace)
        //получаю от него его ПОСЛЕДНИЙ элемент(он станет моим left_peace)
        
        unsigned char my_first = local_data[1];  // Мой первый реальный элемент
        unsigned char left_peace;
        
        // MPI_Sendrecv - отправка и приём одновременно
        // Отправка: данные, размер, тип, кому, тег
        // Приём: буфер, размер, тип, от кого, тег
        // Коммуникатор, статус
        MPI_Sendrecv(&my_first, 1, MPI_UNSIGNED_CHAR, rank - 1, 0,
                     &left_peace, 1, MPI_UNSIGNED_CHAR, rank - 1, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        local_data[0] = left_peace;  // Сохраняем в left_peace позицию

    } else {
        //процесс 0: нет левого соседа, left_peace не нужен
        local_data[0] = 0;
    }
    
    // Обмен с ПРАВЫМ соседом
    if (rank < num_processes - 1) {
        //есть правый сосед (процесс rank+1)
        //отправляем ему мой ПОСЛЕДНИЙ элемент(он станет его left_peace)
        //получаю от него его ПЕРВЫЙ элемент(он станет моим right_peace)
        
        unsigned char my_last = local_data[local_size];  // Мой последний реальный элемент
        unsigned char right_peace;
        
        MPI_Sendrecv(&my_last, 1, MPI_UNSIGNED_CHAR, rank + 1, 0,
                     &right_peace, 1, MPI_UNSIGNED_CHAR, rank + 1, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        local_data[local_size + 1] = right_peace;  // Сохраняем в right_peace позицию
    } else {
        local_data[local_size + 1] = 0;
    }

    int local_count = 0;

    for (int i = 1; i <= local_size; i++) {
        // global_i - это индекс элемента в ИСХОДНОМ большом массиве
        int global_i = start + (i-1);
        
        // СЛУЧАЙ 1: Самый первый элемент массива (индекс 0)
        if (global_i == 0) {
            // У него нет левого соседа, проверяем только правого
            if (local_data[i] >= local_data[i + 1]) {
                local_count++;
            }
        }
        // СЛУЧАЙ 2: Самый последний элемент массива
        else if (global_i == length - 1) {
            // У него нет правого соседа, проверяем только левого
            if (local_data[i] >= local_data[i - 1]) {
                local_count++;
            }
        }
        else {
            // local_data[i-1] - левый сосед (может быть ghost)
            // local_data[i]   - текущий элемент
            // local_data[i+1] - правый сосед (может быть ghost)
            
            if (local_data[i] >= local_data[i - 1] && 
                local_data[i] >= local_data[i + 1]) {
                local_count++;
            }
        }
    }
    
    if (rank == 0) {

        int total_count = local_count;
        cout << local_count << " ";

        //получаем результаты от процессов 1, 2, 3, 4
        for (int source = 1; source < num_processes; source++) {
            int recv_count;
            
            //получаем одно целое число от процесса source
            MPI_Recv(&recv_count, 1, MPI_INT, source, 0, 
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            cout << recv_count << " ";
            
            total_count += recv_count;
        }
        cout << total_count << endl;
        
    } else {
        //отправляем своё число процессу 0
        MPI_Send(&local_count, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }
    
    delete[] local_data;
    
    MPI_Finalize();
    
    return 0;
}