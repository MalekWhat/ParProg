#include <iostream>
#include <mpi.h>
#include <cstdlib>

using namespace std;

const int length = 1000000000;
const int num_processes = 5;

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (size != num_processes) {
        if (rank == 0) {
            cerr << "нужно было запустить " << num_processes 
                 << " процессов! , было запущено " << size << ")" << endl;
        }
        MPI_Finalize();
        return 1;
    }
    
    unsigned char* mas = nullptr;
    
    if (rank == 0) {
        mas = new unsigned char[length];
        srand(2025);
        for (int i = 0; i < length; i++) {
            mas[i] = rand() & 0xff;
        }
    }
    
    int chunk_size = length / num_processes;
    int local_size = chunk_size;
    
    
    // Структура: [left_peace | мои_данные | right_peace]
    int buffer_size = local_size + 2;
    unsigned char* local_data = new unsigned char[buffer_size];
    
    
    
    //MPI_Scatter - автоматически распределяет данные от процесса 0 всем остальным
    //Все процессы вызывают одну и ту же функцию MPI_Scatter
    
    // 1. mas - данные для распределения (важны только для процесса 0, остальные могут передать NULL)
    // 2. chunk_size - сколько элементов получит КАЖДЫЙ процесс
    // 3. MPI_UNSIGNED_CHAR - тип отправляемых данных
    // 4. &local_data[1] - куда сохранить полученные данные (начинаем с индекса 1 из-за left_peace)
    // 5. chunk_size - сколько элементов я ожидаю получить
    // 6. MPI_UNSIGNED_CHAR - тип получаемых данных
    // 7. 0 - какой процесс является источником данных (процесс 0)
    // 8. MPI_COMM_WORLD - коммуникатор
    
    MPI_Scatter(mas, chunk_size, MPI_UNSIGNED_CHAR,
                &local_data[1], chunk_size, MPI_UNSIGNED_CHAR,
                0, MPI_COMM_WORLD);
    
    
    if (rank == 0) {
        delete[] mas;
    }
    
    int start = rank * chunk_size;
    
    //обмен с ЛЕВЫМ соседом
    if (rank > 0) {
        unsigned char my_first = local_data[1];
        unsigned char left_peace;
        
        //только два процесса участвуют: я и мой левый сосед
        MPI_Sendrecv(&my_first, 1, MPI_UNSIGNED_CHAR, rank - 1, 0,
                     &left_peace, 1, MPI_UNSIGNED_CHAR, rank - 1, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        local_data[0] = left_peace;
    } else {
        local_data[0] = 0;  //у процесса 0 нет левого соседа
    }
    
    //обмен с ПРАВЫМ соседом
    if (rank < num_processes - 1) {
        unsigned char my_last = local_data[local_size];
        unsigned char right_peace;
        
        MPI_Sendrecv(&my_last, 1, MPI_UNSIGNED_CHAR, rank + 1, 0,
                     &right_peace, 1, MPI_UNSIGNED_CHAR, rank + 1, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        local_data[local_size + 1] = right_peace;
    } else {
        local_data[local_size + 1] = 0;  //у последнего процесса нет правого соседа
    }
    
    int local_count = 0;
    
    for (int i = 1; i <= local_size; i++) {
        int global_i = start + (i - 1);
        
        if (global_i == 0) {
            if (local_data[i] >= local_data[i + 1]) {
                local_count++;
            }
        }
        else if (global_i == length - 1) {
            if (local_data[i] >= local_data[i - 1]) {
                local_count++;
            }
        }
        else {
            if (local_data[i] >= local_data[i - 1] && 
                local_data[i] >= local_data[i + 1]) {
                local_count++;
            }
        }
    }
    
    cout << local_count << " ";
    
    int total_count = 0;
    
    
    // Параметры MPI_Reduce:
    // 1. &local_count - данные, которые Я отправляю (мой локальный результат)
    // 2. &total_count - куда сохранить результат (важно только для процесса 0)
    // 3. 1 - сколько чисел отправляем (одно целое число)
    // 4. MPI_INT - тип данных
    // 5. MPI_SUM - операция (сложение всех чисел) Есть и другие операции
    // 6. 0 - какой процесс получит результат (процесс 0)
    // 7. MPI_COMM_WORLD - коммуникатор
    
    MPI_Reduce(&local_count, &total_count, 1, MPI_INT, 
               MPI_SUM, 0, MPI_COMM_WORLD);
    
    
    if (rank == 0) {
        cout << total_count << " ";
    }
    
    delete[] local_data;
    MPI_Finalize();
    return 0;
}