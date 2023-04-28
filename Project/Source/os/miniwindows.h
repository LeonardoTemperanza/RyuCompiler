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
    
}