#include "memory_management.h"

// Helper function used by the allocation function
static uintptr AlignForward(uintptr ptr, size_t align);

void Arena_Init(Arena* arena,
                void* backingBuffer,
                size_t backingBufferLength,
                size_t commitSize)
{
    arena->buffer     = (uchar*) backingBuffer;
    arena->length     = backingBufferLength;
    arena->offset     = 0;
    arena->prevOffset = 0;
    arena->commitSize = commitSize;
    
    if(commitSize > 0)
        CommitMemory(backingBuffer, commitSize);
}

void* Arena_Alloc(Arena* arena, size_t size, size_t align)
{
    uintptr curPtr = (uintptr) arena->buffer + (uintptr) arena->offset;
    uintptr offset = AlignForward(curPtr, align);
    
    // Convert to relative offset
    offset -= (uintptr) arena->buffer;
    
    // Check to see if the backing memory has space left
    if(offset + size <= arena->length)
    {
        uintptr nextOffset = offset + size;
        
        if(arena->commitSize > 0)
        {
            uintptr commitAligned = nextOffset - (nextOffset %
                                                  arena->commitSize); 
            if(commitAligned > offset)
            {
                size_t toCommit = commitAligned + arena->commitSize;
                CommitMemory(arena->buffer, toCommit);
            }
        }
        
        void* ptr = &arena->buffer[offset];
        arena->offset  = nextOffset;
        arena->prevOffset = offset;
        
        // Zero new memory for debugging
#ifdef DEBUG
        memset(ptr, 0, size);
#endif
        
        return ptr;
    }
    
    return 0;
}

void* Arena_ResizeLastAlloc(Arena* arena, void* oldMemory, size_t oldSize, size_t newSize, size_t align)
{
    uchar* oldMem = (uchar*)oldMemory;
    Assert(IsPowerOf2(align));
    
    if(!oldMem || oldSize == 0)
        return Arena_Alloc(arena, newSize, align);
    else if(arena->buffer <= oldMem && oldMem < (arena->buffer + arena->length))
    {
        if((arena->buffer + arena->prevOffset) == oldMem)
        {
            arena->offset = arena->prevOffset + newSize;
            if(newSize > oldSize)
            {
                // Zero new memory for debugging
#ifdef DEBUG
                memset(&arena->buffer[arena->offset], 0, newSize - oldSize);
#endif
            }
            
            return oldMemory;
        }
        else
        {
            void* newMemory = Arena_Alloc(arena, newSize, align);
            size_t copySize = oldSize < newSize ? oldSize : newSize;
            // Copy across old memory to the new memory
            memmove(newMemory, oldMemory, copySize);
            return newMemory;
        }
    }
    
    Assert(false && "Memory is out of bounds of the buffer in this arena");
    return 0;
}

template<typename t>
inline void Arena_ResizeLastAllocatedArray(Arena* arena, Array<t>* array, size_t newLength, size_t align)
{
    array->ptr = (t*)Arena_ResizeLastAlloc(arena, array->ptr, sizeof(t) * array->length, sizeof(t) * newLength, align);
    array->length = newLength;
}

void* Arena_AllocAndCopy(Arena* arena, void* toCopy, size_t size, size_t align)
{
    void* result = Arena_Alloc(arena, size, align);
    memcpy(result, toCopy, size);
    return result;
}

char* Arena_PushString(Arena* arena, void* toCopy, size_t size)
{
    char* result = (char*)Arena_Alloc(arena, size+1, 1);
    memcpy(result, toCopy, size);
    result[size] = 0;
    return result;
}

static uintptr AlignForward(uintptr ptr, size_t align)
{
    Assert(IsPowerOf2(align));
    
    uintptr a = (uintptr) align;
    
    // This is the same as (ptr % a) but faster
    // since a is known to be a power of 2
    uintptr modulo = ptr & (a-1);
    
    // If the address is not aligned, push the address
    // to the next value which is aligned
    if(modulo != 0)
        ptr += a - modulo;
    
    return ptr;
}
