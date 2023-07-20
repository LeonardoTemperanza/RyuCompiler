
#include "base.h"
#include "os_agnostic.h"
#include <sys/mman.h>

void* ReserveMemory(size_t size)
{
    ProfileFunc(prof);
    
    void* result = mmap(0, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, (off_t)0);
    Assert(result != MAP_FAILED && "mmap failed!");
    if(result == MAP_FAILED)
        result = 0;
    return result;
}

void CommitMemory(void* mem, size_t size)
{
    int result = mprotect(mem, size, PROT_READ | PROT_WRITE);
    Assert(!result && "mprotect failed!");
}

void FreeMemory(void* mem, size_t size)
{
    int result = madvise(mem, size, MADV_DONTNEED);
    Assert(result != -1 && "madvise failed!");
}