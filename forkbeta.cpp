#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>

using namespace std;

const int length = 1000000000;
const int num_processes = 5;

// Глобальный массив - будет в КАЖДОМ процессе своей копией
unsigned char* mas = nullptr;

// Структура для данных процесса
struct ProcessData {
    int processID;
    int start;
    int end;
    int countProcess;
};

// Функция для подсчёта локальных максимумов
void countLocalMaximums(ProcessData* data) {
    int count = 0;
    
    for (int i = data->start; i < data->end; i++) {
        if (i == 0) {
            if (mas[0] >= mas[1]) count++;
        }
        else if (i == length - 1) {
            if (mas[i] >= mas[i - 1]) count++;
        }
        else {
            if (mas[i] >= mas[i - 1] && mas[i] >= mas[i + 1]) count++;
        }
    }
    
    data->countProcess = count;
}

int main() {
    mas = new unsigned char[length];
    
    // Заполняем массив случайными числами
    srand(2025);
    for (int i = 0; i < length; i++) {
        mas[i] = rand() & 0xff;
    }
    
    ProcessData processData[num_processes];
    int pipefd[num_processes][2];  // Пары файловых дескрипторов для pipe
    
    // Создаём pipe для каждого процесса
    for (int i = 0; i < num_processes; i++) {
        if (pipe(pipefd[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }
    
    // Создаём процессы
    for (int i = 0; i < num_processes; i++) {
        processData[i].processID = i;
        processData[i].start = i * length / num_processes;
        processData[i].end = (i + 1) * length / num_processes;
        processData[i].countProcess = 0;
        
        pid_t pid = fork();
        
        if (pid == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        
        if (pid == 0) {
            // ДОЧЕРНИЙ ПРОЦЕСС
            close(pipefd[i][0]);  // Закрываем чтение в дочернем процессе
            
            // Подсчитываем локальные максимумы
            countLocalMaximums(&processData[i]);
            
            // Отправляем результат через pipe
            write(pipefd[i][1], &processData[i].countProcess, sizeof(int));
            close(pipefd[i][1]);
            
            exit(EXIT_SUCCESS);  // Завершаем дочерний процесс
        }
        // РОДИТЕЛЬСКИЙ ПРОЦЕСС продолжает цикл
    }
    
    // РОДИТЕЛЬСКИЙ ПРОЦЕСС - собираем результаты
    int total = 0;
    
    for (int i = 0; i < num_processes; i++) {
        close(pipefd[i][1]);  // Закрываем запись в родительском процессе
        
        int countFromChild;
        read(pipefd[i][0], &countFromChild, sizeof(int));
        close(pipefd[i][0]);
        
        // Ждём завершения дочернего процесса
        wait(NULL);
        
        cout << "Процесс " << i << ": " << countFromChild << endl;
        total += countFromChild;
    }
    
    cout << "Всего: " << total << endl;
    
    delete[] mas;
    return 0;
}