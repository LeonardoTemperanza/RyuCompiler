
#include "base.h"
#include "os_generic.h"
#include "windows.h"
//#include "miniwindows.h"
#include "microsoft_craziness.h"

static DWORD win32ThreadContextIdx = 0;
static bool win32TlsInitted = false;
static DWORD defaultConsoleAttributes;

void OS_Init()
{
    win32ThreadContextIdx = TlsAlloc();
    
    
    
    win32TlsInitted = true;
}

// Thread context
void SetThreadContext(void* ptr)
{
    assert(win32TlsInitted);
    TlsSetValue(win32ThreadContextIdx, ptr);
}

void* GetThreadContext()
{
    assert(win32TlsInitted);
    return (void*)TlsGetValue(win32ThreadContextIdx);
}

// Timing and profiling utilities
uint64 GetRdtscFreq()
{
    static bool calculated = false;
    static uint64 value = 0;
    
    if(calculated) return value;
    
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
    
    calculated = true;
    value = tscFreq;
    return tscFreq;
}

void SetErrorColor()
{
    HANDLE consoleHandle = GetStdHandle(STD_ERROR_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info = {0};
    GetConsoleScreenBufferInfo(consoleHandle, &info);
    
    defaultConsoleAttributes = info.wAttributes;
    
    SetConsoleTextAttribute(consoleHandle, 12);  // Bright red color
}

void ResetColor()
{
    HANDLE consoleHandle = GetStdHandle(STD_ERROR_HANDLE);
    SetConsoleTextAttribute(consoleHandle, defaultConsoleAttributes);
}

int CurrentTerminalWidth()
{
    HANDLE consoleHandle = GetStdHandle(STD_ERROR_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info = {0};
    GetConsoleScreenBufferInfo(consoleHandle, &info);
    return info.dwSize.X;
}

char* GetPlatformLinkerName()
{
    return "msvc link.exe";
}

int RunPlatformLinker(char* outputPath, char** objFiles, int objFileCount)
{
    Find_Result findRes = find_visual_studio_and_windows_sdk();
    defer(free_resources(&findRes););
    
    if(findRes.windows_sdk_version == 0)
    {
        fprintf(stderr, "Linking failed: could not find Windows SDK\n");
        return 1;
    }
    
    // Launch subprocess executing link.exe
    
    if(!findRes.vs_exe_path)
    {
        fprintf(stderr, "Linking failed: could not find VisualStudio executable path\n");
        return 1;
    }
    
    if(!findRes.vs_library_path)
    {
        fprintf(stderr, "Linking failed: could not find VisualStudio library path\n");
        return 1;
    }
    
    constexpr int cmdMaxLength = 2048;
    wchar_t cmdStr[cmdMaxLength];
    cmdStr[cmdMaxLength-1] = 0;
    const wchar_t* linkerExeName = L"link.exe";
    
    int numChars = _snwprintf(cmdStr, cmdMaxLength,
                              L"\"%ls\\%ls\" /nologo /machine:amd64 /subsystem:console /debug:none /out:%hs /incremental:no /libpath:\"%ls\" /libpath:\"%ls\" kernel32.lib ucrt.lib msvcrt.lib vcruntime.lib ",
                              findRes.vs_exe_path, linkerExeName, outputPath, findRes.windows_sdk_um_library_path, findRes.windows_sdk_ucrt_library_path);
    
    // Add object file paths to the command
    for(int i = 0; i < objFileCount; ++i)
        numChars += _snwprintf(&cmdStr[numChars], cmdMaxLength - numChars, L"%hs ", objFiles[i]);
    
    // Launch linker process
    STARTUPINFOW startupInfo;
    memset(&startupInfo, 0, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    
    PROCESS_INFORMATION processInfo;
    if(!CreateProcessW(0, cmdStr, 0, 0, false, 0, 0, 0, &startupInfo, &processInfo))
    {
        fprintf(stderr, "Error: Could not create process for MSVC linker\n");
        return 1;
    }
    
    // Wait for the end of the process
    WaitForSingleObject(processInfo.hProcess, INFINITE);
    
    // TODO: Get return value of the process and return it to the caller
    
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    return 0;
}
