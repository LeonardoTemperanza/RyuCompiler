
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

// This is for C-code compatibility, the keyword restrict does
// not exist in C++
#define restrict

#ifdef Debug
#define Assert(expression) assert(expression)
#else
#define Assert(expression)
#endif

#if _WIN32 || _WIN64
#if _WIN64
#define Env64Bit
#else
#define Env32Bit
#endif
#endif

#if __GNUC__
#if __x86_64__ || __ppc64__
#define Env64Bit
#else
#define Env32Bit
#endif
#endif

// Common utility functions
#define KB(num) (num)*1024LLU
#define MB(num) KB(num)*1024LLU
#define GB(num) MB(num)*1024LLU

#define StArraySize(array) sizeof(array) / sizeof(array[0])

#define StrLit(string) String { (string), sizeof(string)-1 }

#define Swap(type, var1, var2) do { type tmp = var1; var1 = var2; var2 = tmp; } while(0)
#define SwapIntegral(type, var1, var2) do { var1 += var2; var2 = var1 - var2; var1 -= var2; } while(0)

#define RotateLeft(val, n)  (((val) << (n)) | ((val) >> (sizeof(size_t)*8 - (n))))
#define RotateRight(val, n) (((val) >> (n)) | ((val) << (sizeof(size_t)*8 - (n))))

#ifdef Debug
#define cforceinline 
#else
#define cforceinline __forceinline
#endif

// Switch statement that disables the
// "switch statements should cover all cases"
// warning for enums.
#define switch_nocheck(expression) \
switch(expression) \
_Pragma("warning(push)") \
_Pragma("warning(disable : 4062)") \
_Pragma("warning(disable : 4061)")

#define switch_nocheck_end _Pragma("warning(pop)")

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef uint32_t bool32;

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

#define ProfileFunc(guardName) ProfileFuncGuard guardName(__FUNCTION__, sizeof(__FUNCTION__));

struct ProfileFuncGuard
{
    ProfileFuncGuard() = delete;
    ProfileFuncGuard(char* funcName, uint64 stringLength);
    ~ProfileFuncGuard();
};

#else
#define ProfileFunc(guardName)
#endif  /* Profile */

// Generic data structures
template<typename t>
struct Slice
{
    t* ptr = 0;
    int64 length = 0;
    
    void Append(Arena* a, t element);
    void Resize(Arena* a, uint32 newSize);
    void ResizeAndInit(Arena* a, uint32 newSize);
    Slice<t> CopyToArena(Arena* to);
    cforceinline t last() { return this->ptr[this->length-1]; };
    
#ifdef BoundsChecking
    // For reading the value
    cforceinline t  operator [](int idx) const { Assert(idx < length); return ptr[idx]; };
    // For writing to the value (this returns a left-value)
    cforceinline t& operator [](int idx) { Assert(idx < length); return ptr[idx]; };
#else
    // For reading the value
    cforceinline t  operator [](int idx) const { return ptr[idx]; };
    // For writing to the value (this returns a left-value)
    cforceinline t& operator [](int idx) { return ptr[idx]; };
#endif
};

#define for_array(loopVar, array) for(int loopVar = 0; loopVar < (array).length; ++loopVar)

// Used for dynamic arrays with unknown/variable lifetimes
#define Array_MinCapacity 16
template<typename t>
struct Array : public Slice<t>
{
    int64 capacity = 0;
    
    // All of the below functions assume that the array is initialized
    void Init(int64 initCapacity = DynArray_MinCapacity) { ptr = (t*)malloc(sizeof(t)*initCapacity); capacity = initCapacity; };
    void Append(t element);
    void InsertAtIdx(Slice<t> elements, int idx);
    void InsertAtIdx(t element, int idx);
    t* Reserve();
    void Resize(uint32 numElements);
    void ResizeAndInit(uint32 numElements);
    cforceinline t last() { return this->ptr[this->length-1]; };
    //Array<t> CopyToArena(Arena* to);
    void FreeAll();
    
#ifdef BoundsChecking
    // For reading the value
    cforceinline t  operator [](int idx) const { Assert(idx < length); return vec[idx]; };
    // For writing to the value (this returns a left-value)
    cforceinline t& operator [](int idx) { Assert(idx < length); return vec[idx]; };
#else
    // For reading the value
    cforceinline t  operator [](int idx) const { return ptr[idx]; };
    // For writing to the value (this returns a left-value)
    cforceinline t& operator [](int idx) { return ptr[idx]; };
#endif
};

// Pointer map data structure

template<typename k, typename v>
struct PtrMapEntry
{
    static_assert(sizeof(k) == sizeof(void*), "Key must be pointer");
    
    k key;
    v val;
    uint32 next;
};

template<typename k, typename v>
struct PtrMap
{
    Slice<uint32> hashes;
    PtrMapEntry<k, v>* entries;
    uint32 count;
    uint32 capacity;
};

uint32 PtrHashFunction(uintptr ptr);

// Generic string. This could be null-terminated or not,
// but in the case of strings allocated in a custom allocator,
// it's better to keep the null terminator for C compatibility.
// It's the same thing either way, the length shouldn't take the
// null terminator into account, if present. This is not just a typedef
// of Array<char> because Strings are considered immutable (can't write to it),
// so there is no [] operator for writing to it.
struct String
{
    char* ptr;
    int64 length;
    
    void Append(Arena* a, char element);
    String CopyToArena(Arena* to);
    
#ifdef BoundsChecking
    // For reading the value
    inline char operator [](int idx) const { Assert(idx < length); return ptr[idx]; };
#else
    // For reading the value
    inline char operator [](int idx) const { return ptr[idx]; };
#endif
};

// Hash function from the stb library
uint64 HashString(String str, uint64 seed);
uint64 HashString(char* str, uint64 seed);

// Math utilities
inline bool IsPowerOf2(uintptr a)
{
    return (a & (a-1)) == 0;
}

template<typename t>
cforceinline t max(t i, t j) { return i < j? j : i; }

template<typename t>
cforceinline t min(t i, t j) { return i > j? j : i; }

int numDigits(int n);

// I/O utilities
size_t GetFileSize(FILE* file);
char* ReadEntireFileIntoMemoryAndNullTerminate(char* fileName);

// String utilities

// Very simple implementation of StringBuilder. Uses linear arenas,
// it's pretty much an Array<char> that can append strings instead of chars
// @performance Maybe it would be better to have an array of pointers to strings
// instead, if the strings are very long (>8 characters).
// Instead of copying each character twice, you end up only copying each
// character once (and instead copy the pointers)
// TODO: I think this would just be better if the arena was in the struct itself
struct StringBuilder
{
    String string = { 0, 0 };
    Arena* arena;
    
    StringBuilder() = delete;
    StringBuilder(Arena* arena) { this->arena = arena; }
    void Reset() { this->string = { 0, 0 }; }
    void Append(String str);
    void Append(char* str);
    void Append(char c);
    void Append(int numStr, ...);
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


// Using this quicksort implementation so that comparisons can be inlined

////////////////////////////////////////////////////////////
///////////  qsort as C Macro implementation ///////////////
////////////////////////////////////////////////////////////

/*
 * Copyright (c) 2013, 2017 Alexey Tourbin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * This is a traditional Quicksort implementation which mostly follows
 * [Sedgewick 1978].  Sorting is performed entirely on array indices,
 * while actual access to the array elements is abstracted out with the
 * user-defined `LESS` and `SWAP` primitives.
 *
 * Synopsis:
 *	QSORT(N, LESS, SWAP);
 * where
 *	N - the number of elements in A[];
 *	LESS(i, j) - compares A[i] to A[j];
 *	SWAP(i, j) - exchanges A[i] with A[j].
 */

#ifndef QSORT_H
#define QSORT_H

/* Sort 3 elements. */
#define Q_SORT3(q_a1, q_a2, q_a3, Q_LESS, Q_SWAP) \
do {                                          \
if (Q_LESS(q_a2, q_a1)) {                 \
if (Q_LESS(q_a3, q_a2))               \
Q_SWAP(q_a1, q_a3);               \
else {                                \
Q_SWAP(q_a1, q_a2);               \
if (Q_LESS(q_a3, q_a2))           \
Q_SWAP(q_a2, q_a3);           \
}                                     \
}                                         \
else if (Q_LESS(q_a3, q_a2)) {            \
Q_SWAP(q_a2, q_a3);                   \
if (Q_LESS(q_a2, q_a1))               \
Q_SWAP(q_a1, q_a2);               \
}                                         \
} while (0)

#if 0
{ // For 4coder indentation
#endif
    
    /* Partition [q_l,q_r] around a pivot.  After partitioning,
     * [q_l,q_j] are the elements that are less than or equal to the pivot,
     * while [q_i,q_r] are the elements greater than or equal to the pivot. */
#define Q_PARTITION(q_l, q_r, q_i, q_j, Q_UINT, Q_LESS, Q_SWAP)          \
do {                                                                 \
/* The middle element, not to be confused with the median. */    \
Q_UINT q_m = q_l + ((q_r - q_l) >> 1);                           \
    /* Reorder the second, the middle, and the last items.               \
     * As [Edelkamp Weiss 2016] explain, using the second element         \
     * instead of the first one helps avoid bad behaviour for             \
     * decreasingly sorted arrays.  This method is used in recent         \
     * versions of gcc's std::sort, see gcc bug 58437#c13, although       \
     * the details are somewhat different (cf. #c14). */                  \
    Q_SORT3(q_l + 1, q_m, q_r, Q_LESS, Q_SWAP);                          \
    /* Place the median at the beginning. */                             \
    Q_SWAP(q_l, q_m);                                                    \
    /* Partition [q_l+2, q_r-1] around the median which is in q_l.       \
     * q_i and q_j are initially off by one, they get decremented         \
     * in the do-while loops. */                                          \
    q_i = q_l + 1; q_j = q_r;                                            \
    while (1) {                                                          \
        do q_i++; while (Q_LESS(q_i, q_l));                              \
        do q_j--; while (Q_LESS(q_l, q_j));                              \
        if (q_i >= q_j) break; /* Sedgewick says "until j < i" */        \
        Q_SWAP(q_i, q_j);                                                \
    }                                                                    \
    /* Compensate for the i==j case. */                                  \
    q_i = q_j + 1;                                                       \
    /* Put the median to its final place. */                             \
    Q_SWAP(q_l, q_j);                                                    \
    /* The median is not part of the left subfile. */                    \
    q_j--;                                                               \
} while (0)

/* Insertion sort is applied to small subfiles - this is contrary to
 * Sedgewick's suggestion to run a separate insertion sort pass after
 * the partitioning is done.  The reason I don't like a separate pass
 * is that it triggers extra comparisons, because it can't see that the
 * medians are already in their final positions and need not be rechecked.
 * Since I do not assume that comparisons are cheap, I also do not try
 * to eliminate the (q_j > q_l) boundary check. */
#define Q_INSERTION_SORT(q_l, q_r, Q_UINT, Q_LESS, Q_SWAP)		          \
do {									                                \
Q_UINT q_i, q_j;							                        \
/* For each item starting with the second... */			         \
for (q_i = q_l + 1; q_i <= q_r; q_i++)				              \
/* move it down the array so that the first part is sorted. */  \
for (q_j = q_i; q_j > q_l && (Q_LESS(q_j, q_j - 1)); q_j--)     \
Q_SWAP(q_j, q_j - 1);                                           \
} while (0)

/* When the size of [q_l,q_r], i.e. q_r-q_l+1, is greater than or equal to
 * Q_THRESH, the algorithm performs recursive partitioning.  When the size
 * drops below Q_THRESH, the algorithm switches to insertion sort.
 * The minimum valid value is probably 5 (with 5 items, the second and
 * the middle items, the middle itself being rounded down, are distinct). */
#define Q_THRESH 16

/* The main loop. */
#define Q_LOOP(Q_UINT, Q_N, Q_LESS, Q_SWAP)				                 \
do {									                                \
Q_UINT q_l = 0;							                         \
Q_UINT q_r = (Q_N) - 1;						                     \
Q_UINT q_sp = 0; /* the number of frames pushed to the stack */	 \
struct { Q_UINT q_l, q_r; }						                 \
/* On 32-bit platforms, to sort a "char[3GB+]" array,		           \
 * it may take full 32 stack frames.  On 64-bit CPUs,		                \
 * though, the address space is limited to 48 bits.		                  \
 * The usage is further reduced if Q_N has a 32-bit type. */	             \
q_st[sizeof(Q_UINT) > 4 && sizeof(Q_N) > 4 ? 48 : 32];		              \
while (1) {								                                 \
	if (q_r - q_l + 1 >= Q_THRESH) {				                        \
	    Q_UINT q_i, q_j;						                            \
	    Q_PARTITION(q_l, q_r, q_i, q_j, Q_UINT, Q_LESS, Q_SWAP);	        \
	    /* Now have two subfiles: [q_l,q_j] and [q_i,q_r].		          \
	     * Dealing with them depends on which one is bigger. */	          \
	    if (q_j - q_l >= q_r - q_i)					                     \
            Q_SUBFILES(q_l, q_j, q_i, q_r);				                 \
	    else							                                    \
            Q_SUBFILES(q_i, q_r, q_l, q_j);				                 \
	}								                                       \
	else {								                                  \
	    Q_INSERTION_SORT(q_l, q_r, Q_UINT, Q_LESS, Q_SWAP);		         \
	    /* Pop subfiles from the stack, until it gets empty. */	         \
	    if (q_sp == 0) break;					                           \
	    q_sp--;							                                 \
	    q_l = q_st[q_sp].q_l;					                           \
	    q_r = q_st[q_sp].q_r;					                           \
	}								                                       \
}									                                       \
} while (0)

/* The missing part: dealing with subfiles.
 * Assumes that the first subfile is not smaller than the second. */
#define Q_SUBFILES(q_l1, q_r1, q_l2, q_r2)				                  \
do {									                                \
/* If the second subfile is only a single element, it needs		     \
 * no further processing.  The first subfile will be processed	           \
 * on the next iteration (both subfiles cannot be only a single	          \
 * element, due to Q_THRESH). */					                         \
if (q_l2 == q_r2) {							                             \
	q_l = q_l1;							                                 \
	q_r = q_r1;							                                 \
}									                                       \
else {								                                      \
	/* Otherwise, both subfiles need processing.                            \
	 * Push the larger subfile onto the stack. */                            \
	q_st[q_sp].q_l = q_l1;                                                  \
	q_st[q_sp].q_r = q_r1;						                          \
	q_sp++;								                                 \
	/* Process the smaller subfile on the next iteration. */	            \
	q_l = q_l2;							                                 \
	q_r = q_r2;							                                 \
}									                                       \
} while (0)

/* And now, ladies and gentlemen, may I proudly present to you... */
#define QSORT(Q_N, Q_LESS, Q_SWAP)					                      \
do {									                                \
if ((Q_N) > 1)							                          \
/* We could check sizeof(Q_N) and use "unsigned", but at least	      \
 * on x86_64, this has the performance penalty of up to 5%. */	           \
Q_LOOP(unsigned long, Q_N, Q_LESS, Q_SWAP);			                     \
} while (0)

#endif

/* ex:set ts=8 sts=4 sw=4 noet: */