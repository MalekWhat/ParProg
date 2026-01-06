#include <iostream>
#include <mpi.h>
#include <cstdlib>

using namespace std;

const int length = 1000000000;
const int num_processes = 5;  // ЖЁСТКО ЗАДАННОЕ количество процессов

int main(int argc, char** argv) {
    // ШАГ 1: ЗАПУСК MPI
    MPI_Init(&argc, &argv);
    
    // ШАГ 2: УЗНАЁМ "КТО Я?"
    int rank;  // Мой номер (0, 1, 2, 3 или 4)
    int size;  // Сколько всего процессов
    
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    // Проверка: запущено ли ровно 5 процессов?
    if (size != num_processes) {
        if (rank == 0) {
            cerr << "ОШИБКА! Нужно запустить ровно " << num_processes 
                 << " процессов! (запущено " << size << ")" << endl;
        }
        MPI_Finalize();
        return 1;
    }
    
    // ШАГ 3: ПРОЦЕСС 0 СОЗДАЁТ МАССИВ
    unsigned char* mas = nullptr;
    
    if (rank == 0) {
        cout << "=== Процесс 0: СОЗДАЮ массив ===" << endl;
        mas = new unsigned char[length];
        srand(2025);
        for (int i = 0; i < length; i++) {
            mas[i] = rand() & 0xff;
        }
        cout << "Процесс 0: массив создан!" << endl;
    }
    
    // ШАГ 4: ДЕЛИМ РАБОТУ НА ЧАСТИ
    int chunk_size = length / num_processes;  // 200 000 000 элементов на процесс
    int start = rank * chunk_size;
    int end = (rank + 1) * chunk_size;
    int local_size = end - start;
    
    cout << "Процесс " << rank << ": мой диапазон [" << start << ", " << end << ")" << endl;
    
    // ШАГ 5: СОЗДАЁМ ЛОКАЛЬНЫЙ БУФЕР С GHOST CELLS
    // Важно! Мы берём НЕ просто local_size элементов, а +2 (по одному с каждой стороны)
    // Структура: [left_ghost | мои_данные | right_ghost]
    // Индексы:        0      |  1 ... local_size | local_size+1
    
    int buffer_size = local_size + 2;  // +2 для ghost cells
    unsigned char* local_data = new unsigned char[buffer_size];
    
    // ШАГ 6: ПРОЦЕСС 0 РАЗДАЁТ ДАННЫЕ
    if (rank == 0) {
        cout << "\n=== Процесс 0: РАЗДАЮ данные ===" << endl;
        
        // Процесс 0 копирует свою часть (индексы 1 до local_size включительно)
        for (int i = 0; i < local_size; i++) {
            local_data[i + 1] = mas[i];  // +1 потому что 0-й индекс для left_ghost
        }
        
        // Процесс 0 - первый, у него НЕТ левого соседа
        // Поэтому left_ghost (индекс 0) не используется, можем поставить что угодно
        local_data[0] = 0;  // Фиктивное значение
        
        cout << "Процесс 0: скопировал свою часть" << endl;
        
        // Отправляем данные процессам 1, 2, 3, 4
        for (int dest = 1; dest < num_processes; dest++) {
            int dest_start = dest * chunk_size;
            
            cout << "Процесс 0: отправляю данные процессу " << dest << endl;
            
            // Отправляем основную часть данных
            MPI_Send(&mas[dest_start], chunk_size, MPI_UNSIGNED_CHAR, 
                     dest, 0, MPI_COMM_WORLD);
        }
        
        cout << "Процесс 0: все данные разосланы!" << endl;
        delete[] mas;
        
    } else {
        cout << "Процесс " << rank << ": жду данные от процесса 0..." << endl;
        
        // Получаем данные в середину буфера (индексы 1 до local_size)
        MPI_Recv(&local_data[1], local_size, MPI_UNSIGNED_CHAR, 
                 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        cout << "Процесс " << rank << ": данные получены!" << endl;
    }
    
    // ШАГ 7: ОБМЕН ГРАНИЧНЫМИ ЭЛЕМЕНТАМИ (GHOST CELLS)
    // Это КЛЮЧЕВОЙ момент! Здесь мы ПРАВИЛЬНО заполняем ghost cells
    
    cout << "\n=== Процесс " << rank << ": ОБМЕНИВАЮСЬ граничными элементами ===" << endl;
    
    // Обмен с ЛЕВЫМ соседом (если он есть)
    if (rank > 0) {
        // У меня ЕСТЬ левый сосед (процесс rank-1)
        // Я отправляю ему мой ПЕРВЫЙ элемент (он станет его right_ghost)
        // Я получаю от него его ПОСЛЕДНИЙ элемент (он станет моим left_ghost)
        
        unsigned char my_first = local_data[1];  // Мой первый реальный элемент
        unsigned char left_ghost;
        
        // MPI_Sendrecv - отправка И приём одновременно (избегает deadlock)
        // Параметры:
        // Отправка: данные, размер, тип, кому, тег
        // Приём: буфер, размер, тип, от кого, тег
        // Коммуникатор, статус
        MPI_Sendrecv(&my_first, 1, MPI_UNSIGNED_CHAR, rank - 1, 0,
                     &left_ghost, 1, MPI_UNSIGNED_CHAR, rank - 1, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        local_data[0] = left_ghost;  // Сохраняем в left_ghost позицию
        
        cout << "Процесс " << rank << ": получил left_ghost = " 
             << (int)left_ghost << " от процесса " << (rank - 1) << endl;
    } else {
        // Процесс 0: нет левого соседа, left_ghost не нужен
        local_data[0] = 0;  // Фиктивное значение (не будет использоваться)
    }
    
    // Обмен с ПРАВЫМ соседом (если он есть)
    if (rank < num_processes - 1) {
        // У меня ЕСТЬ правый сосед (процесс rank+1)
        // Я отправляю ему мой ПОСЛЕДНИЙ элемент (он станет его left_ghost)
        // Я получаю от него его ПЕРВЫЙ элемент (он станет моим right_ghost)
        
        unsigned char my_last = local_data[local_size];  // Мой последний реальный элемент
        unsigned char right_ghost;
        
        MPI_Sendrecv(&my_last, 1, MPI_UNSIGNED_CHAR, rank + 1, 0,
                     &right_ghost, 1, MPI_UNSIGNED_CHAR, rank + 1, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        local_data[local_size + 1] = right_ghost;  // Сохраняем в right_ghost позицию
        
        cout << "Процесс " << rank << ": получил right_ghost = " 
             << (int)right_ghost << " от процесса " << (rank + 1) << endl;
    } else {
        // Последний процесс: нет правого соседа, right_ghost не нужен
        local_data[local_size + 1] = 0;  // Фиктивное значение
    }
    
    // ШАГ 8: ПОДСЧЁТ ЛОКАЛЬНЫХ МАКСИМУМОВ
    cout << "\n=== Процесс " << rank << ": НАЧИНАЮ подсчёт ===" << endl;
    
    int local_count = 0;
    
    // Проходим по РЕАЛЬНЫМ элементам (индексы 1 до local_size)
    for (int i = 1; i <= local_size; i++) {
        int global_i = start + (i - 1);  // Глобальный индекс в исходном массиве
        
        // СЛУЧАЙ 1: Самый первый элемент массива (global_i == 0)
        if (global_i == 0) {
            // У него нет левого соседа в принципе
            // Проверяем только правого
            if (local_data[i] >= local_data[i + 1]) {
                local_count++;
            }
        }
        // СЛУЧАЙ 2: Самый последний элемент массива
        else if (global_i == length - 1) {
            // У него нет правого соседа в принципе
            // Проверяем только левого
            if (local_data[i] >= local_data[i - 1]) {
                local_count++;
            }
        }
        // СЛУЧАЙ 3: Все остальные элементы
        else {
            // Теперь у нас ЕСТЬ все соседи благодаря ghost cells!
            // local_data[i-1] - левый сосед (может быть ghost)
            // local_data[i]   - текущий элемент
            // local_data[i+1] - правый сосед (может быть ghost)
            
            if (local_data[i] >= local_data[i - 1] && 
                local_data[i] >= local_data[i + 1]) {
                local_count++;
            }
        }
    }
    
    cout << "Процесс " << rank << ": нашёл " << local_count << " локальных максимумов" << endl;
    
    // ШАГ 9: СОБИРАЕМ РЕЗУЛЬТАТЫ НА ПРОЦЕССЕ 0
    if (rank == 0) {
        cout << "\n=== Процесс 0: СОБИРАЮ результаты ===" << endl;
        
        int total_count = local_count;
        
        for (int source = 1; source < num_processes; source++) {
            int recv_count;
            MPI_Recv(&recv_count, 1, MPI_INT, source, 0, 
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            cout << "Процесс 0: получил от процесса " << source 
                 << " результат = " << recv_count << endl;
            
            total_count += recv_count;
        }
        
        cout << "\n========================================" << endl;
        cout << "ИТОГО локальных максимумов: " << total_count << endl;
        cout << "========================================" << endl;
        
    } else {
        cout << "Процесс " << rank << ": отправляю результат процессу 0" << endl;
        MPI_Send(&local_count, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }
    
    delete[] local_data;
    MPI_Finalize();
    return 0;
}