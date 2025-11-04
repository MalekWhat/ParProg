#include <iostream>
#include <windows.h>
#include <cstdlib>

using namespace std;

const int length = 1000000000;
const int num_threads = 5;

unsigned char* mas = nullptr;

struct ThreadData {
    int threadID;
    int start;
    int end;
    int countThread;
};

DWORD WINAPI CountLocalMaxima(LPVOID param) {
    ThreadData* data = (ThreadData*)param;
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
    
    data->countThread = count;
    return 0;
}

int main() {
    mas = new unsigned char[length];
    
    srand(2025);
    for (int i = 0; i < length; i++) {
        mas[i] = rand() & 0xff;
    }
    
    ThreadData threadData[num_threads];
    HANDLE threads[num_threads];
    
    for (int i = 0; i < num_threads; i++) {
        threadData[i].threadID = i;
        threadData[i].start = i * length / num_threads;
        threadData[i].end = (i + 1) * length / num_threads;
        threadData[i].countThread = 0;
        
        threads[i] = CreateThread(NULL, 0, CountLocalMaxima, &threadData[i], 0, NULL);
    }
    
    WaitForMultipleObjects(num_threads, threads, TRUE, INFINITE);
    
    for (int i = 0; i < num_threads; i++) {
        CloseHandle(threads[i]);
    }
    
    int counter = 0;
    for (int i = 0; i < num_threads; i++) {
        cout << "Поток " << i << ": " << threadData[i].countThread << endl;
        counter += threadData[i].countThread;
    }
    cout << "Всего: " << counter << endl;
    
    delete[] mas;
    return 0;
}
