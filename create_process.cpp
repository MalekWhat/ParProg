#include <iostream>
#include <windows.h>
#include <cstdlib>

using namespace std;

const int length = 1000000000;
const int num_processes = 5;

//
// CreateProcess с ДОПОМ Relocs Disable  5l
//
//
int main(int argc, char* argv[]) {
    //дочка
    if (argc == 4) {
        int processID = atoi(argv[1]);
        int start = atoi(argv[2]);
        int end = atoi(argv[3]);
        
        cout << "Процесс " << processID << ": запущен, PID=" << GetCurrentProcessId() << endl;
        cout << "Процесс " << processID << ": адрес main = " << hex << (void*)main << dec << endl;
        
        int chunk = end - start;
        int buffer_size = chunk + 2;
        unsigned char* local_data = new unsigned char[buffer_size];

        srand(2025);
        for (int i = 0; i < start - 1; i++) {
            rand();
        }
        if (processID > 0) {
            local_data[0] = rand() & 0xff;
        } else {
            local_data[0] = 0;
        }
        for (int i = 0; i < chunk; i++) {
            local_data[i + 1] = rand() & 0xff;
        }
        if (processID < num_processes - 1) {
            local_data[buffer_size - 1] = rand() & 0xff;
        } else {
            local_data[buffer_size - 1] = 0;
        }
        int local_count = 0;
   
        for (int i = 1; i <= chunk; i++) {
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
        delete[] local_data;
        return local_count;
    }
    //родитель
    cout << "PID родителя: " << GetCurrentProcessId() << endl;
    cout << "Адрес main в родителе: " << hex << (void*)main << dec << endl;
    cout << endl;
    
    //путь к программе
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    
    //для процессов
    PROCESS_INFORMATION processInfo[num_processes];
    STARTUPINFOA startupInfo[num_processes];
    
    int chunk_size = length / num_processes;
    
    cout << "Создаю " << num_processes << " дочерних процессов" << endl;
    cout << endl;
    //создание дочек
    for (int i = 0; i < num_processes; i++) {
        int start = i * chunk_size;
        int end = (i + 1) * chunk_size;
        
        char cmdLine[512];
        sprintf(cmdLine, "\"%s\" %d %d %d", exePath, i, start, end);
        
        ZeroMemory(&startupInfo[i], sizeof(STARTUPINFOA));
        startupInfo[i].cb = sizeof(STARTUPINFOA);
        ZeroMemory(&processInfo[i], sizeof(PROCESS_INFORMATION));
        
        BOOL success = CreateProcessA(
            NULL, cmdLine, NULL, NULL, FALSE, 0, 
            NULL, NULL, &startupInfo[i], &processInfo[i]
        );
        
        if (!success) {
            cerr << "ОШИБКА при создании процесса " << i << ": " << GetLastError() << endl;
            return 1;
        }
        
        cout << "Процесс " << i << " PID: " << processInfo[i].dwProcessId 
             << ", диапазон: [" << start << ", " << end << ")" << endl;
    }

    HANDLE handles[num_processes];
    for (int i = 0; i < num_processes; i++) {
        handles[i] = processInfo[i].hProcess;
    }
    WaitForMultipleObjects(num_processes, handles, TRUE, INFINITE);

    int total_count = 0;
    for (int i = 0; i < num_processes; i++) {
        DWORD exitCode;
        GetExitCodeProcess(processInfo[i].hProcess, &exitCode);
        
        cout << "Процесс " << i << ": " << exitCode << " максимумов" << endl;
        total_count += exitCode;
        
        CloseHandle(processInfo[i].hProcess);
        CloseHandle(processInfo[i].hThread);
    }
    cout << "Итого максимумов: " << total_count << endl;

    cout << "Программа скомпилирована с флагами:" << endl;
    cout << "-Wl,--image-base=0x400000" << endl;
    cout << "-Wl,--disable-dynamicbase" << endl;
    return 0;
}