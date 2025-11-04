#include <stdlib.h>
#include <iostream>

using namespace std;

int main(){
    size_t length = 1000000000;
    srand(2025);
    unsigned char *nums = new unsigned char[length];
    for (size_t i = 0; i < length; ++i) {
        nums[i] = rand() & 0xFF;
    }
    long counter = 0;
    for (size_t i=1; i < length-1; ++i){
        if (nums[i] >= nums[i-1] && nums[i] >= nums[i+1]){
            counter++;
        }
    }
    if (nums[0] >= nums[1]) 
        counter++;

    if (nums[length-1] >= nums[length-2]) 
        counter++;

    cout << counter << endl;
    delete[] nums;
}