
#include "base.h"
#include "os_agnostic.h"

//#define WIN32_LEAN_AND_MEAN
//#include <windows.h>
//#undef WIN32_LEAN_AND_MEAN

#include "miniwindows.h"

static DWORD win32ThreadContextIdx = 0;
static HANDLE win32ConsoleHandle = 0;
static CONSOLE_SCREEN_BUFFER_INFO win32SavedScreenBufferInfo;

void OS_Init()
{
    win32ThreadContextIdx = TlsAlloc();
    win32ConsoleHandle = GetStdHandle(STD_ERROR_HANDLE);
    
    GetConsoleScreenBufferInfo(win32ConsoleHandle, &win32SavedScreenBufferInfo);
}

// Memory utilities
void* ReserveMemory(size_t size)
{
    void* result = VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
    Assert(result && "VirtualAlloc failed!");
    return result;
}

void CommitMemory(void* mem, size_t size)
{
    void* result = VirtualAlloc(mem, size, MEM_COMMIT, PAGE_READWRITE);
    Assert(result && "VirtualAlloc failed!");
}

void FreeMemory(void* mem, size_t size)
{
    bool result = VirtualFree(mem, 0, MEM_RELEASE);
    Assert(result && "VirtualFree failed!");
}

// Thread context
void SetThreadContext(void* ptr)
{
    TlsSetValue(win32ThreadContextIdx, ptr);
}

void* GetThreadContext()
{
    return (void*)TlsGetValue(win32ThreadContextIdx);
}

// Timing and profiling utilities
uint64 GetRdtscFreq()
{
    uint32 tscFreq = 3000000000;
    bool fastPath = false;
    HMODULE ntdll = LoadLibrary("ntdll.dll");
    if(ntdll)
    {
        int (*NtQuerySystemInformation)(int, void*, uint32, uint32*) =
        (int (*)(int, void*, uint32, uint32*))GetProcAddress(ntdll, "NtQuerySystemInformation");
        if(NtQuerySystemInformation)
        {
            volatile uint64* HSUV = 0;
            uint32 size = 0;
            int result = NtQuerySystemInformation(0xc5, (void**)&HSUV, sizeof(HSUV), &size);
            if(size == sizeof(HSUV) && result >= 0)
            {
                tscFreq = (10000000ull << 32) / (HSUV[1] >> 32);
                fastPath = true;
            }
        }
        
        FreeLibrary(ntdll);
    }
    
    if(!fastPath)
    {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        
        LARGE_INTEGER qpcBegin;
        QueryPerformanceCounter(&qpcBegin);
        uint64 tscBegin = __rdtsc();
        Sleep(2);
        
        LARGE_INTEGER qpcEnd;
        QueryPerformanceCounter(&qpcEnd);
        uint64 tscEnd = __rdtsc();
        tscFreq = (tscEnd - tscBegin) * frequency.QuadPart / (qpcEnd.QuadPart - qpcBegin.QuadPart);
    }
    
    return tscFreq;
}


void SetErrorColor()
{
    GetConsoleScreenBufferInfo(win32ConsoleHandle, &win32SavedScreenBufferInfo);
    SetConsoleTextAttribute(win32ConsoleHandle, 4);  // Dark red color
}

void ResetColor()
{
    SetConsoleTextAttribute(win32ConsoleHandle, win32SavedScreenBufferInfo.wAttributes);
}