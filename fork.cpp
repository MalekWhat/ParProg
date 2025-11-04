#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

const int length = 1000000000;
const int num_processes = 5;

unsigned char* mas = nullptr;

int countLocalMaxima(int start, int end) {
    int count = 0;
    
    for (int i = start; i < end; i++) {
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
    
    return count;
}

int main() {
    mas = new unsigned char[length];
    srand(2025);
    for (int i = 0; i < length; i++) {
        mas[i] = rand() & 0xff;
    }
 
    //массивы для хранения pipe'ов и PID'ов
    int pipes[num_processes][2];  // [0] - для чтения, [1] - для записи
    pid_t pids[num_processes];
    
    //создаём дочерние процессы
    for (int i = 0; i < num_processes; i++) {

        if (pipe(pipes[i]) == -1) {
            cerr << "Ошибка создания pipe для процесса " << i << endl;
            return 1;
        }
        int start = i * length / num_processes;
        int end = (i + 1) * length / num_processes;
        
        //дочерний процесс
        pid_t pid = fork();
        
        if (pid < 0) {
            cerr << "Ошибка fork() для процесса " << i << endl;
            return 1;  
        }
        else if (pid == 0) {
            //КОД ДОЧЕРНЕГО ПРОЦЕССА
            //закрываем  конец pipe (чтение)
            close(pipes[i][0]);

            int count = countLocalMaxima(start, end);
            
            //отправляем результат родителю через pipe
            write(pipes[i][1], &count, sizeof(count));
            
            close(pipes[i][1]);
            delete[] mas;
            exit(0);
        }
        else {
            //КОД РОДИТЕЛЬСКОГО ПРОЦЕССА            
            //сохраняем PID дочернего процесса
            pids[i] = pid;
            //cout << "Процесс " << i << " запущен (PID: " << pid << ")" << endl;
            
            close(pipes[i][1]);
        }
    }
    
    //родитель
    for (int i = 0; i < num_processes; i++) {
        int status;
        waitpid(pids[i], &status, 0);
    }
        
    int counter = 0;
    
    for (int i = 0; i < num_processes; i++) {
        int result;

        ssize_t bytes = read(pipes[i][0], &result, sizeof(result));
        
        if (bytes == sizeof(result)) {
            cout << result << " ";
            counter += result;
        } else {
            cerr << "Ошибка чтения результата процесса " << i << endl;
        }
        
        close(pipes[i][0]);
    }
    
    cout << counter << endl;
    
    delete[] mas;
    return 0;
}