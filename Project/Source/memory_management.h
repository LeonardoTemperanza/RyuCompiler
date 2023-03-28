
#pragma once

#include "base.h"

// This file provides allocation strategies that are
// utilized throughout this project
#define Default_Alignment (2 * sizeof(void*))

// NOTE(Leo): The first helper macros are for smallest alignment
// (good for cache locality, but bad for SSE?),
// while the second helper macros are for 18-byte alignment
// (good for SSE, bad for cache locality?)
// The second one seems to be better because when good
// cache locality is needed things are stored in an array
// anyway, usually.
#if 0

#define Arena_FromStack(arenaPtr, stackVar) \
(decltype(stackVar)*)Arena_AllocAndCopy((arenaPtr), &(stackVar), sizeof(stackVar), \
alignof(decltype(stackVar)))
#define Arena_AllocArray(arenaPtr, size, type) \
(type*)Arena_Alloc((arenaPtr), (size)*sizeof(type), alignof(type))
#define Arena_AllocVar(arenaPtr, type) \
(type*)Arena_Alloc((arenaPtr), sizeof(type), alignof(type))

// Calls the constructor / initializer list. A "placement new" is used because
// by default C++ can't call the constructor on custom allocated variables.
#define Arena_AllocAndInit(arenaPtr, type) \
new (Arena_Alloc((arenaPtr), sizeof(type), alignof(type))) type

#else

#define Arena_FromStack(arenaPtr, stackVar) \
(decltype(stackVar)*)Arena_AllocAndCopy((arenaPtr), &(stackVar), sizeof(stackVar))
#define Arena_AllocArray(arenaPtr, size, type) \
(type*)Arena_Alloc((arenaPtr), (size)*sizeof(type))
#define Arena_AllocVar(arenaPtr, type) \
(type*)Arena_Alloc((arenaPtr), sizeof(type))

// Calls the constructor / initializer list. A "placement new" is used because
// by default C++ can't call the constructor on custom allocated variables.
#define Arena_AllocAndInit(arenaPtr, type) \
new (Arena_Alloc((arenaPtr), sizeof(type))) type

#endif

struct Arena
{
    uchar* buffer;
    size_t length;
    size_t offset;
    size_t prevOffset;
    
    // NOTE(Leo): Commit memory in
    // blocks of size commitSize,
    // if 0 then it never commits
    // (useful for stack-allocated arenas)
    size_t commitSize;
};

struct TempArenaMemory
{
    Arena* arena;
    size_t offset;
    size_t prevOffset;
};

// Initialize the arena with a pre-allocated buffer
void Arena_Init(Arena* arena,
                void* backingBuffer,
                size_t backingBufferLength,
                size_t commitSize);
void* Arena_Alloc(Arena* arena,
                  size_t size,
                  size_t align = Default_Alignment);
void* Arena_ResizeLastAlloc(Arena* arena,
                            void* oldMemory,
                            size_t oldSize,
                            size_t newSize,
                            size_t align = Default_Alignment);
void* Arena_AllocAndCopy(Arena* arena,
                         void* toCopy,
                         size_t size,
                         size_t align = Default_Alignment);
// The provided string can also not be null-terminated.
// The resulting string will be null-terminated
// automatically.
char* Arena_PushString(Arena* arena,
                       void* toCopy,
                       size_t size);

// Used to free all the memory within the allocator
// by setting the buffer offsets to zero
inline void Arena_FreeAll(Arena* arena)
{
    arena->offset     = 0;
    arena->prevOffset = 0;
}

inline TempArenaMemory Arena_TempBegin(Arena* arena)
{
    TempArenaMemory tmp;
    tmp.arena      = arena;
    tmp.offset     = arena->offset;
    tmp.prevOffset = arena->prevOffset;
    return tmp;
}

inline void Arena_TempEnd(TempArenaMemory tmp)
{
    Assert(tmp.offset >= 0);
    Assert(tmp.prevOffset >= 0);
    tmp.arena->offset     = tmp.offset;
    tmp.arena->prevOffset = tmp.prevOffset;
}