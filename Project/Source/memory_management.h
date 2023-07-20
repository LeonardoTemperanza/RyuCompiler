
#pragma once

#include "base.h"

// This provides allocation strategies that are
// utilized throughout the project

#define Default_Alignment (2 * sizeof(void*))

// NOTE(Leo): The first helper macros are for smallest alignment
// (good for cache locality, but bad for SSE?),
// while the second helper macros (pack) are for 16-byte alignment
// (good for SSE, bad for cache locality?)
// The second one seems to be better because when good
// cache locality is needed, things are usually stored in an array anyway.


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


#define Arena_FromStackPack(arenaPtr, stackVar) \
(decltype(stackVar)*)Arena_AllocAndCopy((arenaPtr), &(stackVar), sizeof(stackVar), alignof(decltype(stackVar)))
#define Arena_AllocArrayPack(arenaPtr, size, type) \
(type*)Arena_Alloc((arenaPtr), (size)*sizeof(type), alignof(type))
#define Arena_AllocVarPack(arenaPtr, type) \
(type*)Arena_Alloc((arenaPtr), sizeof(type), alignof(type))
#define Arena_AllocAndInitPack(arenaPtr, type) \
new (Arena_Alloc((arenaPtr), sizeof(type), alignof(type))) type


// Variants that allow the use of different alignment
#define Arena_FromStackAlign(arenaPtr, stackVar, alignment) \
(decltype(stackVar)*)Arena_AllocAndCopy((arenaPtr), &(stackVar), sizeof(stackVar), (alignment))
#define Arena_AllocArrayAlign(arenaPtr, size, type, alignment) \
(type*)Arena_Alloc((arenaPtr), (size)*sizeof(type), (alignment))
#define Arena_AllocVarAlign(arenaPtr, type, alignment) \
(type*)Arena_Alloc((arenaPtr), sizeof(type), (alignment))
#define Arena_AllocAndInitAlign(arenaPtr, type, alignment) \
new (Arena_Alloc((arenaPtr), sizeof(type), (alignment))) type


struct Arena
{
    uchar* buffer;
    size_t length;
    size_t offset;
    size_t prevOffset;
    
    // Commit memory in blocks of size
    // commitSize, if == 0, then it never
    // commits (useful for stack-allocated arenas)
    size_t commitSize;
};

// Can be used like:
// TempArenaMemory tempGaurd = Arena_TempBegin(arena);
// defer(Arena_TempEnd(tempGuard);
struct TempArenaMemory
{
    Arena* arena;
    size_t offset;
    size_t prevOffset;
};

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

// Serves as a helper for obtaining a scratch
// arena. It also resets the arena's state upon
// destruction.
struct ScratchArena
{
    TempArenaMemory tempGuard;
    
    // NOTE(Leo): In case a ScratchArena is passed to the
    // constructor with the intent to do an implicit cast to
    // Arena*, the actual result is the copy constructor is called
    // which is not expected. This makes an error appear instead of
    // silently failing.
    ScratchArena(ScratchArena& scratch) = delete;
    
    // This could be templatized, but...
    // we're not going to need more than
    // 4 scratch arenas anyway, and you could
    // always add more constructors
    cforceinline ScratchArena();
    cforceinline ScratchArena(Arena* a1);
    cforceinline ScratchArena(Arena* a1, Arena* a2);
    cforceinline ScratchArena(Arena* a1, Arena* a2, Arena* a3);
    
    // This constructor is used to get a certain scratch arena,
    // without handling potential conflicts. It can be used when
    // using multiple scratch arenas in a single function which
    // doesn't allocate anything to be used by the caller (so,
    // it doesn't accept an Arena* as parameter)
    cforceinline ScratchArena(int idx);
    
    cforceinline ~ScratchArena() { Arena_TempEnd(tempGuard); };
    cforceinline void Reset() { Arena_TempEnd(tempGuard); };
    cforceinline Arena* arena() { return tempGuard.arena; };
    cforceinline operator Arena*() { return tempGuard.arena; };
};

uintptr AlignForward(uintptr ptr, size_t align);

// Initialize the arena with a pre-allocated buffer
void Arena_Init(Arena* arena, void* backingBuffer,
                size_t backingBufferLength, size_t commitSize);
void* Arena_Alloc(Arena* arena,
                  size_t size, size_t align = Default_Alignment);
void* Arena_ResizeLastAlloc(Arena* arena, void* oldMemory,
                            size_t oldSize, size_t newSize,
                            size_t align = Default_Alignment);
void* Arena_AllocAndCopy(Arena* arena, void* toCopy,
                         size_t size, size_t align = Default_Alignment);
// The provided string can also not be null-terminated.
// The resulting string will be null-terminated
// automatically.
char* Arena_PushString(Arena* arena, void* toCopy, size_t size);

// Used to free all the memory within the allocator
// by setting the buffer offsets to zero
inline void Arena_FreeAll(Arena* arena)
{
    arena->offset     = 0;
    arena->prevOffset = 0;
}