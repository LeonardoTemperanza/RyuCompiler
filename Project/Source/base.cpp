
#include "base.h"

char* ReadEntireFileIntoMemoryAndNullTerminate(char* fileName)
{
    char* result = NULL;
    
    FILE* file = fopen(fileName, "rb");
    if(file)
    {
        // Get the file size by fseeking
        // to the end and then calling
        // ftell, proceed to reset the
        // pointer with fseek(0)
        fseek(file, 0, SEEK_END);
        size_t fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        result = (char*)malloc(fileSize + 1);
        fread(result, 1, fileSize, file);
        result[fileSize] = 0;
        
        fclose(file);
    }
    else
    {
        printf("Usage Error: Could not find the file ");
        printf(fileName);
        printf("\n");
    }
    
    return result;
}

// Array utilities
template<typename t>
void Array<t>::AddElement(Arena* a, t element)
{
    // If empty, allocate a new array
    if(this->length == 0)
        this->ptr = Arena_FromStack(a, element);
    else
    {
        auto ptr = (t*)Arena_ResizeLastAlloc(a, this->ptr,
                                             sizeof(t) * this->length,
                                             sizeof(t) * (this->length + 1));
        Assert(ptr == this->ptr && "Adding an element to an array that was not the last allocation performed in the linear arena is not allowed");
        this->ptr = ptr;
        this->ptr[this->length] = element;
    }
    
    ++this->length;
}

template<typename t>
Array<t> Array<t>::CopyToArena(Arena* to)
{
    Array<t> result = *this;
    result.ptr = (t*)Arena_AllocAndCopy(to, this->ptr, sizeof(t) * this->length);
    return result;
}

// Array utilities
#define DynArray_MinCapacity 10
template<typename t>
void DynArray<t>::AddElement(t element)
{
    ++this->length;
    if(this->capacity < this->length)
    {
        if(this->capacity < DynArray_MinCapacity)
            this->capacity = DynArray_MinCapacity;
        else
            this->capacity = (this->capacity + 1) * 3 / 2;  // TODO: better grow formula?
        
        this->ptr = (t*)realloc(this->ptr, sizeof(t) * this->capacity);
    }
    
    this->ptr[this->length - 1] = element;
}

void StringBuilder::Append(String str, Arena* dest)
{
    if(this->string.ptr)
    {
        auto ptr = (char*)Arena_ResizeLastAlloc(dest, this->string.ptr,
                                                sizeof(char) * this->string.length,
                                                sizeof(char) * (this->string.length + str.length));
        Assert(ptr == this->string.ptr && "Appending to a string that was not the last allocation performed in the linear arena is not allowed");
        this->string.ptr = ptr;
        memcpy((void*)(this->string.ptr + this->string.length), str.ptr, str.length);
        this->string.length += str.length;
    }
    else
    {
        this->string.ptr = Arena_AllocArray(dest, str.length, char);
        memcpy(this->string.ptr, str.ptr, str.length);
        this->string.length = str.length;
    }
}

void StringBuilder::Append(Arena* dest, int numStr, ...)
{
    Assert(numStr > 0);
    
    va_list list;
    va_start(list, numStr);
    
    for(int i = 0; i < numStr; ++i)
        this->Append(va_arg(list, String), dest);
    
    va_end(list);
}

String StringBuilder::ToString(Arena* dest)
{
    return this->string.CopyToArena(dest);
}

// String utilities
bool operator ==(String s1, String s2)
{
    if(s1.length != s2.length)
        return false;
    
    for(int i = 0; i < s1.length; ++i)
    {
        if(s1[i] != s2[i])
            return false;
    }
    
    return true;
}

// char* strings are null-terminated strings.
bool operator ==(String s1, char* s2)
{
    for(int i = 0; i < s1.length; ++i)
    {
        if(s2[i] == 0 || s1[i] != s2[i])
            return false;
    }
    
    return true;
}

// Copied from the operator above because another
// function call might slow down the program if lots
// of string compares are being performed
bool operator ==(char* s1, String s2)
{
    for(int i = 0; i < s2.length; ++i)
    {
        if(s1[i] == 0 || s2[i] != s1[i])
            return false;
    }
    
    return true;
}

// Exits if it sees a null terminator
bool String_FirstCharsMatchEntireString(char* stream, String str)
{
    for(int i = 0; i < str.length; ++i)
    {
        if(stream[i] == 0 || stream[i] != str[i])
            return false;
    }
    
    return true;
}

// Thread context
void ThreadCtx_Init(ThreadContext* threadCtx, size_t scratchReserveSize, size_t scratchCommitSize)
{
    Arena* scratch = threadCtx->scratchPool;
    for(int i = 0; i < StArraySize(threadCtx->scratchPool); ++i)
    {
        auto baseMemory = ReserveMemory(scratchReserveSize);
        Arena_Init(&scratch[i], baseMemory, scratchReserveSize, scratchCommitSize);
    }
}

// Some procedures might accept arenas as arguments,
// for dynamically allocated results yielded to the caller.
// The argument arena might, in turn, be a scratch arena used
// by the caller for some temporary computation. This means
// that whenever a procedure wants a thread-local scratch buffer
// it needs to provide the arenas that they're already using,
// so that the provided scratch arena does not conflict with anything
//
// For more information on memory arenas: https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator
Arena* GetScratchArena(ThreadContext* threadCtx, Arena** conflictArray, int count)
{
    Assert(count < ThreadCtx_NumScratchArenas);
    Arena* result = 0;
    Arena* scratch = threadCtx->scratchPool;
    for(int i = 0; i < StArraySize(threadCtx->scratchPool); ++i)
    {
        bool conflict = false;
        for(int j = 0; j < count; ++j)
        {
            if(&scratch[i] == conflictArray[j])
            {
                conflict = true;
                break;
            }
        }
        
        if(!conflict)
        {
            result = &scratch[i];
            break;
        }
    }
    
    return result;
}

#ifdef Profile
void InitSpall()
{
    spallCtx = spall_init_file("constellate.spall", 1000000.0 / GetRdtscFreq());
    
    size_t bufferSize = MB(100);
    uchar* buffer = (uchar*)malloc(bufferSize);
    memset(buffer, 1, bufferSize);
    
    spallBuffer.length = bufferSize;
    spallBuffer.data   = buffer;
}

void QuitSpall()
{
    spall_buffer_quit(&spallCtx, &spallBuffer);
    spall_quit(&spallCtx);
}

ProfileFuncGuard::ProfileFuncGuard(char* funcName)
{
    spall_buffer_begin(&spallCtx, &spallBuffer, funcName, sizeof(funcName), __rdtsc());
}

ProfileFuncGuard::~ProfileFuncGuard()
{
    spall_buffer_end(&spallCtx, &spallBuffer, __rdtsc());
}

#endif  /* Profile */