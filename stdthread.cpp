#include <iostream>
#include <thread>
#include <vector>
#include <cstdlib>

using namespace std;

const int length = 1000000000;
const int num_thread = 5;

unsigned char* mas;
int results[num_thread];

void countLocalMaximums(int threadId, int start, int end) {
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
    
    results[threadId] = count;
}

int main() {
    mas = new unsigned char[length];
    
    srand(2025);
    for (int i = 0; i < length; i++) {
        mas[i] = rand() & 0xff;
    }
    
    vector<thread> threads;
    
    for (int i = 0; i < num_thread; i++) {
        int start = i * length / num_thread;
        int end = (i + 1) * length / num_thread;
        threads.push_back(thread(countLocalMaximums, i, start, end));
    }
    
    for (int i = 0; i < num_thread; i++) {
        threads[i].join();
    }
    
    int total = 0;
    for (int i = 0; i < num_thread; i++) {
        cout << "Поток " << i << ": " << results[i] << endl;
        total += results[i];
    }
    cout << "Всего: " << total << endl;
    
    delete[] mas;
    return 0;
}
