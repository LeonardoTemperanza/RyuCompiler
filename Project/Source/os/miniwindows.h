#pragma once

// NOTE(Leo): this file is a minimal version of windows.h,
// containing only the functions that are needed in this
// project (VirtualAlloc and VirtualFree and the associated
// types). This speeds up compilation quite a lot, and
// saves from including a bunch of unnecessary stuff.

extern "C"
{
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)
    
#define WINAPI  __stdcall
#define WINBASEAPI
#define CONST const
    typedef unsigned short WORD;
    typedef unsigned long ULONG;
    typedef long LONG;
    typedef short SHORT;
    typedef long long LONGLONG;
    typedef unsigned long DWORD;
    typedef unsigned __int64 DWORD64;
    typedef void VOID;
    typedef void* LPVOID;
    typedef LONG* LPLONG;
    typedef CONST void *LPCVOID;
    typedef char CHAR;
    typedef CHAR *LPSTR;
    typedef wchar_t WCHAR;
    typedef WCHAR *LPWSTR;
    typedef CONST WCHAR *LPCWSTR;
    typedef __nullterminated CONST CHAR *LPCSTR;
    typedef bool BOOL;
#if defined(_WIN64)
    typedef unsigned __int64 ULONG_PTR;
#else
    typedef unsigned long ULONG_PTR;
#endif
    typedef ULONG_PTR SIZE_T;
	typedef unsigned long DWORD;
    typedef void* HANDLE;
    typedef HANDLE* LPHANDLE;
    typedef HANDLE HINSTANCE;
    typedef HINSTANCE HMODULE;
    typedef DWORD COLORREF;
    typedef DWORD* LPCOLORREF;
    // What even is this...?
#define far
#define FAR far
#define near
#define NEAR near
#define DUMMYSTRUCTNAME
    typedef int (FAR WINAPI *FARPROC)();
    typedef unsigned char BYTE;
    typedef BYTE far *LPBYTE;
    
    typedef union _LARGE_INTEGER {
        struct {
            DWORD LowPart;
            LONG  HighPart;
        } DUMMYSTRUCTNAME;
        struct {
            DWORD LowPart;
            LONG  HighPart;
        } u;
        LONGLONG QuadPart;
    } LARGE_INTEGER;
    
    typedef struct _SMALL_RECT {
        SHORT Left;
        SHORT Top;
        SHORT Right;
        SHORT Bottom;
    } SMALL_RECT;
    
    typedef struct _COORD {
        SHORT X;
        SHORT Y;
    } COORD, *PCOORD;
    
    typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
        COORD      dwSize;
        COORD      dwCursorPosition;
        WORD       wAttributes;
        SMALL_RECT srWindow;
        COORD      dwMaximumWindowSize;
    } CONSOLE_SCREEN_BUFFER_INFO, *PCONSOLE_SCREEN_BUFFER_INFO;
    
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
    
#define MEM_COMMIT     0x00001000
#define MEM_RESERVE    0x00002000
#define MEM_RELEASE    0x00008000
#define PAGE_READWRITE 0x04
    
    WINBASEAPI
        _Ret_maybenull_
        _Post_writable_byte_size_(dwSize)
        LPVOID
        WINAPI
        VirtualAlloc(
                     _In_opt_ LPVOID lpAddress,
                     _In_ SIZE_T dwSize,
                     _In_ DWORD flAllocationType,
                     _In_ DWORD flProtect
                     );
    
    WINBASEAPI
        BOOL
        WINAPI
        VirtualFree(
                    _Pre_notnull_ _When_(dwFreeType == MEM_DECOMMIT,_Post_invalid_) _When_(dwFreeType == MEM_RELEASE,_Post_ptr_invalid_) LPVOID lpAddress,
                    _In_ SIZE_T dwSize,
                    _In_ DWORD dwFreeType
                    );
    
    void ASSERT(BOOL cond);
    
#ifndef _M_CEE_PURE
    WINBASEAPI
        _Check_return_
        DWORD
        WINAPI
        GetLastError(
                     VOID
                     );
#endif
#if !defined(MIDL_PASS)
    WINBASEAPI
        _Success_(return != 0)
        DWORD
        WINAPI
        FormatMessageA(
                       _In_     DWORD dwFlags,
                       _In_opt_ LPCVOID lpSource,
                       _In_     DWORD dwMessageId,
                       _In_     DWORD dwLanguageId,
                       _When_((dwFlags & FORMAT_MESSAGE_ALLOCATE_BUFFER) != 0, _At_((LPSTR*)lpBuffer, _Outptr_result_z_))
                       _When_((dwFlags & FORMAT_MESSAGE_ALLOCATE_BUFFER) == 0, _Out_writes_z_(nSize))
                       LPSTR lpBuffer,
                       _In_     DWORD nSize,
                       _In_opt_ va_list *Arguments
                       );
    WINBASEAPI
        _Success_(return != 0)
        DWORD
        WINAPI
        FormatMessageW(
                       _In_     DWORD dwFlags,
                       _In_opt_ LPCVOID lpSource,
                       _In_     DWORD dwMessageId,
                       _In_     DWORD dwLanguageId,
                       _When_((dwFlags & FORMAT_MESSAGE_ALLOCATE_BUFFER) != 0, _At_((LPWSTR*)lpBuffer, _Outptr_result_z_))
                       _When_((dwFlags & FORMAT_MESSAGE_ALLOCATE_BUFFER) == 0, _Out_writes_z_(nSize))
                       LPWSTR lpBuffer,
                       _In_     DWORD nSize,
                       _In_opt_ va_list *Arguments
                       );
#ifdef UNICODE
#define FormatMessage  FormatMessageW
#else
#define FormatMessage  FormatMessageA
#endif // !UNICODE
    
#if defined(_M_CEE)
#undef FormatMessage
    __inline
        DWORD
        FormatMessage(
                      DWORD dwFlags,
                      LPCVOID lpSource,
                      DWORD dwMessageId,
                      DWORD dwLanguageId,
                      LPTSTR lpBuffer,
                      DWORD nSize,
                      va_list *Arguments
                      )
    {
#ifdef UNICODE
        return FormatMessageW(
#else
                              return FormatMessageA(
#endif
                                                    dwFlags,
                                                    lpSource,
                                                    dwMessageId,
                                                    dwLanguageId,
                                                    lpBuffer,
                                                    nSize,
                                                    Arguments
                                                    );
    }
#endif  /* _M_CEE */
#endif  /* MIDL_PASS */
    
    WINBASEAPI
        _Ret_maybenull_
        HMODULE
        WINAPI
        LoadLibraryExA(
                       _In_ LPCSTR lpLibFileName,
                       _Reserved_ HANDLE hFile,
                       _In_ DWORD dwFlags
                       );
    
    WINBASEAPI
        _Ret_maybenull_
        HMODULE
        WINAPI
        LoadLibraryExW(
                       _In_ LPCWSTR lpLibFileName,
                       _Reserved_ HANDLE hFile,
                       _In_ DWORD dwFlags
                       );
#ifdef UNICODE
#define LoadLibraryEx  LoadLibraryExW
#else
#define LoadLibraryEx  LoadLibraryExA
#endif // !UNICODE
    
    WINBASEAPI
        _Ret_maybenull_
        HMODULE
        WINAPI
        LoadLibraryA(
                     _In_ LPCSTR lpLibFileName
                     );
    
    WINBASEAPI
        _Ret_maybenull_
        HMODULE
        WINAPI
        LoadLibraryW(
                     _In_ LPCWSTR lpLibFileName
                     );
#ifdef UNICODE
#define LoadLibrary  LoadLibraryW
#else
#define LoadLibrary  LoadLibraryA
#endif // !UNICODE
    
    WINBASEAPI
        FARPROC
        WINAPI
        GetProcAddress(
                       _In_ HMODULE hModule,
                       _In_ LPCSTR lpProcName
                       );
    
    WINBASEAPI
        BOOL
        WINAPI
        FreeLibrary(
                    _In_ HMODULE hLibModule
                    );
    
    WINBASEAPI
        VOID
        WINAPI
        Sleep(
              _In_ DWORD dwMilliseconds
              );
    
    //
    // Performance counter API's
    //
    
    WINBASEAPI
        BOOL
        WINAPI
        QueryPerformanceCounter(
                                _Out_ LARGE_INTEGER* lpPerformanceCount
                                );
    
    WINBASEAPI
        BOOL
        WINAPI
        QueryPerformanceFrequency(
                                  _Out_ LARGE_INTEGER* lpFrequency
                                  );
    
#define ReadTimeStampCounter() __rdtsc()
    
    DWORD64
        __rdtsc (
                 VOID
                 );
    
    WINBASEAPI
        BOOL
        WINAPI
        TlsSetValue(
                    _In_ DWORD dwTlsIndex,
                    _In_opt_ LPVOID lpTlsValue
                    );
    
    _Must_inspect_result_
        WINBASEAPI
        DWORD
        WINAPI
        TlsAlloc(
                 VOID
                 );
    
    WINBASEAPI
        LPVOID
        WINAPI
        TlsGetValue(
                    _In_ DWORD dwTlsIndex
                    );
    
    
    WINBASEAPI
        HANDLE
        WINAPI
        GetStdHandle(
                     _In_ DWORD nStdHandle
                     );
    
    WINBASEAPI
        BOOL
        WINAPI
        SetStdHandle(
                     _In_ DWORD nStdHandle,
                     _In_ HANDLE hHandle
                     );
    
    WINBASEAPI
        BOOL
        WINAPI
        SetConsoleTextAttribute(
                                _In_ HANDLE hConsoleOutput,
                                _In_ WORD wAttributes
                                );
    
    
    /*typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
        COORD dwSize;
        COORD dwCursorPosition;
        WORD  wAttributes;
        SMALL_RECT srWindow;
        COORD dwMaximumWindowSize;
    } CONSOLE_SCREEN_BUFFER_INFO, *PCONSOLE_SCREEN_BUFFER_INFO;
    */
    
    WINBASEAPI
        BOOL
        WINAPI
        GetConsoleScreenBufferInfo(
                                   _In_ HANDLE hConsoleOutput,
                                   _Out_ PCONSOLE_SCREEN_BUFFER_INFO lpConsoleScreenBufferInfo
                                   );
    
    typedef struct _CONSOLE_SCREEN_BUFFER_INFOEX {
        ULONG cbSize;
        COORD dwSize;
        COORD dwCursorPosition;
        WORD wAttributes;
        SMALL_RECT srWindow;
        COORD dwMaximumWindowSize;
        WORD wPopupAttributes;
        BOOL bFullscreenSupported;
        COLORREF ColorTable[16];
    } CONSOLE_SCREEN_BUFFER_INFOEX, *PCONSOLE_SCREEN_BUFFER_INFOEX;
    
    WINBASEAPI
        BOOL
        WINAPI
        GetConsoleScreenBufferInfoEx(
                                     _In_ HANDLE hConsoleOutput,
                                     _Inout_ PCONSOLE_SCREEN_BUFFER_INFOEX lpConsoleScreenBufferInfoEx
                                     );
    
    WINBASEAPI
        BOOL
        WINAPI
        SetConsoleScreenBufferInfoEx(
                                     _In_ HANDLE hConsoleOutput,
                                     _In_ PCONSOLE_SCREEN_BUFFER_INFOEX lpConsoleScreenBufferInfoEx
                                     );
    
    // Threads and processes
    
    typedef struct _PROCESS_INFORMATION {
        HANDLE hProcess;
        HANDLE hThread;
        DWORD  dwProcessId;
        DWORD  dwThreadId;
    } PROCESS_INFORMATION, *PPROCESS_INFORMATION, *LPPROCESS_INFORMATION;
    
    typedef struct _PROC_THREAD_ATTRIBUTE_LIST *PPROC_THREAD_ATTRIBUTE_LIST, *LPPROC_THREAD_ATTRIBUTE_LIST;
    
    typedef struct _SECURITY_ATTRIBUTES {
        DWORD  nLength;
        LPVOID lpSecurityDescriptor;
        BOOL   bInheritHandle;
    } SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;
    
    typedef struct _STARTUPINFOA {
        DWORD  cb;
        LPSTR  lpReserved;
        LPSTR  lpDesktop;
        LPSTR  lpTitle;
        DWORD  dwX;
        DWORD  dwY;
        DWORD  dwXSize;
        DWORD  dwYSize;
        DWORD  dwXCountChars;
        DWORD  dwYCountChars;
        DWORD  dwFillAttribute;
        DWORD  dwFlags;
        WORD   wShowWindow;
        WORD   cbReserved2;
        LPBYTE lpReserved2;
        HANDLE hStdInput;
        HANDLE hStdOutput;
        HANDLE hStdError;
    } STARTUPINFOA, *LPSTARTUPINFOA;
    
    typedef struct _STARTUPINFOW {
        DWORD  cb;
        LPWSTR lpReserved;
        LPWSTR lpDesktop;
        LPWSTR lpTitle;
        DWORD  dwX;
        DWORD  dwY;
        DWORD  dwXSize;
        DWORD  dwYSize;
        DWORD  dwXCountChars;
        DWORD  dwYCountChars;
        DWORD  dwFillAttribute;
        DWORD  dwFlags;
        WORD   wShowWindow;
        WORD   cbReserved2;
        LPBYTE lpReserved2;
        HANDLE hStdInput;
        HANDLE hStdOutput;
        HANDLE hStdError;
    } STARTUPINFOW, *LPSTARTUPINFOW;
    
#ifdef UNICODE
    typedef STARTUPINFOW STARTUPINFO;
    typedef LPSTARTUPINFOW LPSTARTUPINFO;
#else
    typedef STARTUPINFOA STARTUPINFO;
    typedef LPSTARTUPINFOA LPSTARTUPINFO;
#endif // UNICODE
    
    typedef struct _STARTUPINFOEXA {
        STARTUPINFOA                 StartupInfo;
        LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList;
    } STARTUPINFOEXA, *LPSTARTUPINFOEXA;
    
    typedef struct _PROC_THREAD_ATTRIBUTE_LIST *PPROC_THREAD_ATTRIBUTE_LIST, *LPPROC_THREAD_ATTRIBUTE_LIST;
    
    WINBASEAPI
        BOOL
        WINAPI
        CreateProcessA(
                       _In_opt_ LPCSTR lpApplicationName,
                       _Inout_opt_ LPSTR lpCommandLine,
                       _In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
                       _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
                       _In_ BOOL bInheritHandles,
                       _In_ DWORD dwCreationFlags,
                       _In_opt_ LPVOID lpEnvironment,
                       _In_opt_ LPCSTR lpCurrentDirectory,
                       _In_ LPSTARTUPINFOA lpStartupInfo,
                       _Out_ LPPROCESS_INFORMATION lpProcessInformation
                       );
    
    WINBASEAPI
        BOOL
        WINAPI
        CreateProcessW(
                       _In_opt_ LPCWSTR lpApplicationName,
                       _Inout_opt_ LPWSTR lpCommandLine,
                       _In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
                       _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
                       _In_ BOOL bInheritHandles,
                       _In_ DWORD dwCreationFlags,
                       _In_opt_ LPVOID lpEnvironment,
                       _In_opt_ LPCWSTR lpCurrentDirectory,
                       _In_ LPSTARTUPINFOW lpStartupInfo,
                       _Out_ LPPROCESS_INFORMATION lpProcessInformation
                       );
#ifdef UNICODE
#define CreateProcess  CreateProcessW
#else
#define CreateProcess  CreateProcessA
#endif // !UNICODE
    
    // Synchronization
    
#define INFINITE 0xFFFFFFFF
    
    WINBASEAPI
        BOOL
        WINAPI
        SetEvent(
                 _In_ HANDLE hEvent
                 );
    
    WINBASEAPI
        BOOL
        WINAPI
        ResetEvent(
                   _In_ HANDLE hEvent
                   );
    
    WINBASEAPI
        BOOL
        WINAPI
        ReleaseSemaphore(
                         _In_ HANDLE hSemaphore,
                         _In_ LONG lReleaseCount,
                         _Out_opt_ LPLONG lpPreviousCount
                         );
    
    WINBASEAPI
        BOOL
        WINAPI
        ReleaseMutex(
                     _In_ HANDLE hMutex
                     );
    
    WINBASEAPI
        DWORD
        WINAPI
        WaitForSingleObject(
                            _In_ HANDLE hHandle,
                            _In_ DWORD dwMilliseconds
                            );
    
    WINBASEAPI
        DWORD
        WINAPI
        SleepEx(
                _In_ DWORD dwMilliseconds,
                _In_ BOOL bAlertable
                );
    
    WINBASEAPI
        DWORD
        WINAPI
        WaitForSingleObjectEx(
                              _In_ HANDLE hHandle,
                              _In_ DWORD dwMilliseconds,
                              _In_ BOOL bAlertable
                              );
    
    WINBASEAPI
        DWORD
        WINAPI
        WaitForMultipleObjectsEx(
                                 _In_ DWORD nCount,
                                 _In_reads_(nCount) CONST HANDLE* lpHandles,
                                 _In_ BOOL bWaitAll,
                                 _In_ DWORD dwMilliseconds,
                                 _In_ BOOL bAlertable
                                 );
    
    // Handles
    
    WINBASEAPI
        BOOL
        WINAPI
        CloseHandle(
                    _In_ _Post_ptr_invalid_ HANDLE hObject
                    );
    
    WINBASEAPI
        BOOL
        WINAPI
        DuplicateHandle(
                        _In_ HANDLE hSourceProcessHandle,
                        _In_ HANDLE hSourceHandle,
                        _In_ HANDLE hTargetProcessHandle,
                        _Outptr_ LPHANDLE lpTargetHandle,
                        _In_ DWORD dwDesiredAccess,
                        _In_ BOOL bInheritHandle,
                        _In_ DWORD dwOptions
                        );
    
}