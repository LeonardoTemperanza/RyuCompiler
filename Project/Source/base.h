
#pragma once

// This file provides common utilities used in all files

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <new>       // This is only used for the placement new in memory arenas
#include <climits>
#include <assert.h>
#include <stdarg.h>

// @temporary
#include <vector>

#ifdef Debug
#define Assert(expression) assert(expression)
#else
#define Assert(expression)
#endif

#define KB(num) (num)*1024LLU
#define MB(num) KB(num)*1024LLU
#define GB(num) MB(num)*1024LLU

#define StArraySize(array) sizeof(array) / sizeof(array[0])

// TODO: bad type conversion here???
#define StrLit(string) String { (string), (int64)strlen(string) }

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef unsigned char uchar;
typedef uintptr_t uintptr;

#include "os/os_agnostic.h"
#include "memory_management.h"

// Thread context
#define ThreadCtx_NumScratchArenas 4
struct ThreadContext
{
    Arena scratchPool[ThreadCtx_NumScratchArenas];
};

void ThreadCtx_Init(ThreadContext* threadCtx, size_t scratchReserveSize, size_t scratchCommitSize);
Arena* GetScratchArena(ThreadContext* threadCtx, Arena** conflictArray, int count);


#ifdef Profile
#include "spall/spall.h"

static SpallProfile spallCtx;
static SpallBuffer spallBuffer;

void InitSpall();
void QuitSpall();

#define ProfileFunc(guardName) ProfileFuncGuard guardName(__FUNCTION__);

struct ProfileFuncGuard
{
    ProfileFuncGuard() = delete;
    ProfileFuncGuard(char* funcName);
    ~ProfileFuncGuard();
};

#else
#define ProfileFunc(guardName)
#endif  /* Profile */

// Generic data structures
template<typename t>
struct Array
{
    t* ptr;
    int64 length;
    
    void AddElement(Arena* a, t element);
    Array<t> CopyToArena(Arena* to);
    
#ifdef BoundsChecking
    // For reading the value
    inline t  operator [](int idx) const { Assert(idx < length); return ptr[idx]; };
    // For writing to the value (this returns a left-value)
    inline t& operator [](int idx) { Assert(idx < length); return ptr[idx]; };
#else
    // For reading the value
    inline t  operator [](int idx) const { return ptr[idx]; };
    // For writing to the value (this returns a left-value)
    inline t& operator [](int idx) { return ptr[idx]; };
#endif
};

// Used for dynamic arrays allocated on the heap
// @performance @temporary Improve implementation
template<typename t>
struct DynArray
{
    std::vector<t> vec;
    int64 length = 0;
    
    void AddElement(t element);
    //Array<t> CopyToArena(Arena* to);
    
#ifdef BoundsChecking
    // For reading the value
    inline t  operator [](int idx) const { Assert(idx < length); return vec[idx]; };
    // For writing to the value (this returns a left-value)
    inline t& operator [](int idx) { Assert(idx < length); return vec[idx]; };
#else
    // For reading the value
    inline t  operator [](int idx) const { return vec[idx]; };
    // For writing to the value (this returns a left-value)
    inline t& operator [](int idx) { return vec[idx]; };
#endif
};

// Generic string. This could be null-terminated or not,
// but in the case of strings allocated in a custom allocator,
// it's better to keep the null terminator for C compatibility.
// It's the same thing either way, the length shouldn't take the
// null terminator into account, if present.
typedef Array<char> String;

// Math utilities
inline bool IsPowerOf2(uintptr a)
{
    return (a & (a-1)) == 0;
}

// I/O utilities
char* ReadEntireFileIntoMemoryAndNullTerminate(char* fileName);

// String utilities

// Very simple implementation of StringBuilder. Uses linear arenas,
// it's pretty much an Array<char> that can append strings instead of chars
// @performance Maybe it would be better to have an array of pointers to strings
// instead, if the strings are very long (>8 characters).
// Instead of copying each character twice, you end up only copying each
// character once (and instead copy the pointers)
struct StringBuilder
{
    String string = { 0, 0 };
    void Append(String str, Arena* arena);
    String ToString(Arena* dest);
};

bool operator ==(String s1, String s2);
bool operator ==(char* s1, String s2);
bool operator ==(String s1, char* s2);

bool String_FirstCharsMatchEntireString(char* stream, String str);

// Others
// Defer, used to execute anything at the end of the scope
// used in many languages such as D, Go, Odin.
// Credits to GingerBill.
template<typename T> struct Defer_RemoveReference      { typedef T Type; };
template<typename T> struct Defer_RemoveReference<T&>  { typedef T Type; };
template<typename T> struct Defer_RemoveReference<T&&> { typedef T Type; };

// C++ "Move semantics" are used
template<typename T>
inline T&& Defer_Forward(typename Defer_RemoveReference<T>::Type& t)
{
    return static_cast<T&&>(t);
}

template<typename T>
inline T&& Defer_Forward(typename Defer_RemoveReference<T>::Type&& t)
{
    return static_cast<T&&>(t);
}

template<typename T>
inline T&& Defer_Move(T&& t)
{
    return static_cast<typename Defer_RemoveReference<T>::Type &&>(t);
}

template<typename F>
struct Defer
{
    F f;
    Defer(F&& f) : f(Defer_Forward<F>(f)) {}
    ~Defer() { f(); }
};

template<typename F>
Defer<F> DeferFunc(F&& f)
{
    return Defer<F>(Defer_Forward<F>(f));
}

#define Internal_Defer1(x, y) x##y
#define Internal_Defer2(x, y) Internal_Defer1(x, y)
#define Internal_Defer3(x)    Internal_Defer2(x, __COUNTER__)
#define defer(code)           auto Internal_Defer3(_defer_) = DeferFunc([&]()->void{code;})

// Defer example usage:
//    int test()
//    {
//        int* arr = new int[5];
//        defer(delete[] arr);
//        
//        ...
//        
//    // delete will be executed here
//    }