#pragma once

// This file provides common utilities used in all files

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <new>       // This is only used for the placement new in memory arenas
#include <climits>
#include <assert.h>


#ifdef Debug
#define Assert(expression) assert(expression)
#else
#define Assert(expression)
#endif

#ifdef Profile
#include "spall/spall.h"

SpallProfile spallCtx;
SpallBuffer spallBuffer;

void InitSpall();
void QuitSpall();

#define ProfileFunc() \
spall_buffer_begin(&spallCtx, &spallBuffer, __FUNCTION__, sizeof(__FUNCTION__), __rdtsc()); \
defer(spall_buffer_end(&spallCtx, &spallBuffer, __rdtsc()))
#else
#define ProfileFunc()
#endif

#define KB(num) (num)*1024LLU
#define MB(num) KB(num)*1024LLU
#define GB(num) MB(num)*1024LLU

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

// Generic data structures
template<typename t>
struct Array
{
    t* ptr;
    int64 length;
    
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

// Array utilities
template<typename t>
void Array_AddElement(Array<t>* arr, Arena* a, t element);
template<typename t>
Array<t> Array_CopyToArena(Array<t> arr, Arena* to);

// Generic string. This could be null-terminated or not,
// as it could point to a stream (thus non-null-terminated),
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
bool operator ==(String s1, String s2);
bool operator ==(char* s1, String s2);
bool operator ==(String s1, char* s2);

// Others
// NOTE(Leo): Defer, used to execute anything at the end of the scope
// used in many languages such as D, Go, Odin.
// Credits to GingerBill for this. 
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