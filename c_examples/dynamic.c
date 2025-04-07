#include <cstdlib>
#include <cstdio>

void* MemAlloc(size_t size) {
    void* mem = malloc(size);

    ((int*)mem)[0] = 10;
    ((int*)mem)[1] = 20;

    return mem;
}

int main() {
    void* mem = MemAlloc(100);

    ((int*)mem)[4] = 40;

    int value = 5;
    int* fake_pointer = &value;

    *fake_pointer = 10;

    void* new_mem = realloc(mem, 150);
    
    free(new_mem);
}