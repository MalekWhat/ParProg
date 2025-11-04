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
    // Процессы 1, 2, 3, 4 НЕ заходят в этот if и просто ждут
    
    int chunk_size = length / num_processes;
    int start = rank * chunk_size;  
    int end = (rank + 1) * chunk_size;
    
    int local_size = end - start;
    
    cout << "Процесс " << rank << ": мой диапазон [" << start << ", " << end << ")" << endl;
    
    // ШАГ 5: КАЖДЫЙ ПРОЦЕСС СОЗДАЁТ СВОЙ ЛОКАЛЬНЫЙ БУФЕР
    // Процесс 1 не может просто взять и прочитать mas[0], потому что mas есть только у процесса 0
    // Поэтому КАЖДЫЙ процесс создаёт свой маленький массив для своей части данных

    //int buffer_size = local_size + 2;
    
    unsigned char* local_data = new unsigned char[local_size];
    
    if (rank == 0) {
        cout << "Процесс 0: раздаю данные" << endl;
        
        for (int i = 0; i < local_size; i++) {
            local_data[i] = mas[i];
        }
        cout << "Процесс 0: скопировал свою часть" << endl;
        
        // Теперь отправляем данные процессам 1, 2, 3, 4
        for (int dest = 1; dest < num_processes; dest++) {
            int dest_start = dest * chunk_size;  // Откуда начинается кусок для процесса dest
            
            // 1. &mas[dest_start] - ОТКУДА берём данные (адрес начала)
            // 2. chunk_size - СКОЛЬКО элементов отправляем
            // 3. MPI_UNSIGNED_CHAR - ТИП данных (беззнаковый char)
            // 4. dest - КОМУ отправляем (номер процесса-получателя)
            // 5. 0 - ТЕГ сообщения (метка, чтобы различать разные сообщения)
            // 6. MPI_COMM_WORLD - КОММУНИКАТОР (группа процессов)
            MPI_Send(&mas[dest_start], chunk_size, MPI_UNSIGNED_CHAR, 
                     dest, 0, MPI_COMM_WORLD);
        }
        
        delete[] mas;
        
    } else {        
        // 1. local_data - КУДА сохранить полученные данные
        // 2. local_size - СКОЛЬКО элементов ожидаем получить
        // 3. MPI_UNSIGNED_CHAR - ТИП данных
        // 4. 0 - ОТ КОГО ждём данные (от процесса 0)
        // 5. 0 - ТЕГ сообщения (должен совпадать с тегом в Send)
        // 6. MPI_COMM_WORLD - КОММУНИКАТОР
        // 7. MPI_STATUS_IGNORE - статус (нам не нужен, поэтому игнорируем)
        MPI_Recv(local_data, local_size, MPI_UNSIGNED_CHAR, 
                 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
    

    int local_count = 0;

    for (int i = 0; i < local_size; i++) {
        // global_i - это индекс элемента в ИСХОДНОМ большом массиве
        int global_i = start + i;
        
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
            // Проблема на границах кусков:
            // Например, процесс 1 обрабатывает элементы 200000000 - 399999999
            // Для элемента 200000000 левый сосед (199999999) находится у процесса 0!
            // Мы его НЕ видим в своём local_data
            
            bool left_ok, right_ok;
            
            // Проверка ЛЕВОГО соседа
            if (i == 0 && rank > 0) {
                // Мы на границе куска (первый элемент) И мы не процесс 0
                // Значит левый сосед в другом процессе
                // УПРОЩЕНИЕ: считаем что условие выполнено
                // (в реальности нужно запросить этот элемент у соседнего процесса)
                left_ok = true;
            } else if (i > 0) {
                // Левый сосед в нашем куске - просто сравниваем
                left_ok = (local_data[i] >= local_data[i - 1]);
            } else {
                left_ok = true;
            }
            
            // Проверка ПРАВОГО соседа
            if (i == local_size - 1 && rank < num_processes - 1) {
                // Мы на границе куска (последний элемент) И мы не последний процесс
                // Значит правый сосед в другом процессе
                // УПРОЩЕНИЕ: считаем что условие выполнено
                right_ok = true;
            } else if (i < local_size - 1) {
                // Правый сосед в нашем куске - просто сравниваем
                right_ok = (local_data[i] >= local_data[i + 1]);
            } else {
                right_ok = true;
            }
            
            // Если элемент больше или равен ОБОИМ соседям - это локальный максимум
            if (left_ok && right_ok) {
                local_count++;
            }
        }
    }
    
    cout << "Процесс " << rank << ": нашёл " << local_count << " локальных максимумов" << endl;
    
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