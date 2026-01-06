#!pip install pycuda
#запускаю в гугл коллабе если что

import pycuda.autoinit
import pycuda.driver as cuda
from pycuda.compiler import SourceModule
import numpy as np
import ctypes

length = 1000000000
num_blocks = 5
threads_per_block = 256

cuda_kernel = """
#include <stdio.h>

__global__ void countLocalMaxima(unsigned char *mas, int *block_counts, int length, int num_blocks) {
    int chunk_size = length / num_blocks;
    int block_start = blockIdx.x * chunk_size;
    int block_end = (blockIdx.x == num_blocks - 1) ? length : (blockIdx.x + 1) * chunk_size;
    
    //высисляю глобальный индекс потока внутри блока
    int local_idx = threadIdx.x;
    int stride = blockDim.x;  //сколько потоков в блоке

    int local_count = 0;

    for (int idx = block_start + local_idx; idx < block_end; idx += stride) {
        int is_maximum = 0;
        if (idx == 0) {
            if (mas[0] >= mas[1]) {
                is_maximum = 1;
            }
        }
        else if (idx == length - 1) {
            if (mas[idx] >= mas[idx - 1]) {
                is_maximum = 1;
            }
        }
        else if (idx == block_start && idx > 0) {
            //заглядываем в предыдущий блок
            if (mas[idx] >= mas[idx - 1] && mas[idx] >= mas[idx + 1]) {
                is_maximum = 1;
            }
        }
        else if (idx == block_end - 1 && idx < length - 1) {
            //заглядываем в следующий блок
            if (mas[idx] >= mas[idx - 1] && mas[idx] >= mas[idx + 1]) {
                is_maximum = 1;
            }
        }
        else {
            if (mas[idx] >= mas[idx - 1] && mas[idx] >= mas[idx + 1]) {
                is_maximum = 1;
            }
        }
        
        local_count += is_maximum;
    }
    
    //используем shared memory для суммирования внутри блока
    __shared__ int shared_counts[256];
    shared_counts[threadIdx.x] = local_count;
    __syncthreads();  //ждём пока все потоки запишут свои значения
    
    // делает только первый поток
    if (threadIdx.x == 0) {
        int block_total = 0;
        for (int i = 0; i < blockDim.x; i++) {
            block_total += shared_counts[i];
        }
        block_counts[blockIdx.x] = block_total;
    }
}
"""
libc = ctypes.CDLL("libc.so.6")
libc.srand(2025)

mas_cpu = np.empty(length, dtype=np.uint8)

chunk_size = 10000000  # Обрабатываем по 10 млн за раз для прогресса
for i in range(0, length, chunk_size):
    end = min(i + chunk_size, length)
    for j in range(i, end):
        mas_cpu[j] = libc.rand() & 0xFF
    if (i // chunk_size) % 10 == 0:
        print(f"Заполнено {i}/{length} ({100*i//length}%)")

print(f"Заполнено {length}/{length} (100%)")

mod = SourceModule(cuda_kernel)
count_func = mod.get_function("countLocalMaxima")

mas_gpu = cuda.mem_alloc(mas_cpu.nbytes)
block_counts_gpu = cuda.mem_alloc(num_blocks * np.int32().itemsize)
cuda.memcpy_htod(mas_gpu, mas_cpu)

#запускаем kernel
#grid=(num_blocks, 1) - количество блоков
#block=(threads_per_block, 1, 1) - потоков внутри каждого блока
count_func(
    mas_gpu,
    block_counts_gpu,
    np.int32(length),
    np.int32(num_blocks),
    block=(threads_per_block, 1, 1),
    grid=(num_blocks, 1)
)
cuda.Context.synchronize()
block_counts_cpu = np.empty(num_blocks, dtype=np.int32)
cuda.memcpy_dtoh(block_counts_cpu, block_counts_gpu)

for i in range(num_blocks):
    print(f"Блок {i}: {block_counts_cpu[i]}")

total_count = np.sum(block_counts_cpu)
print(f"Всего: {total_count}")

mas_gpu.free()
block_counts_gpu.free()
