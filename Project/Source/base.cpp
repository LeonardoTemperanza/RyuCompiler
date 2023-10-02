
#include "base.h"

char* ReadEntireFileIntoMemoryAndNullTerminate(char* fileName)
{
    ProfileFunc(prof);
    
    char* result = NULL;
    
    FILE* file = fopen(fileName, "rb");
    if(file)
    {
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

size_t GetFileSize(FILE* file)
{
    if(!file) return 0;
    
    size_t prev = ftell(file);
    fseek(file, 0, SEEK_END);
    size_t res = ftell(file);
    fseek(file, 0, prev);
    return res;
}

// Array utilities
template<typename t>
void Array<t>::Append(Arena* a, t element)
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
void Array<t>::Resize(Arena* a, uint32 newSize)
{
    // If empty, allocate a new array
    if(this->length == 0)
        this->ptr = (t*)Arena_Alloc(a, sizeof(t) * newSize, alignof(t));
    else
    {
        auto ptr = (t*)Arena_ResizeLastAlloc(a, this->ptr,
                                             sizeof(t) * this->length,
                                             sizeof(t) * (newSize));
        Assert(ptr == this->ptr && "Adding an element to an array that was not the last allocation performed in the linear arena is not allowed");
        this->ptr = ptr;
    }
    
    this->length = newSize;
}

template<typename t>
void Array<t>::ResizeAndInit(Arena* a, uint32 newSize)
{
    uint32 oldLength = this->length;
    this->Resize(a, newSize);
    
    // Call default constructor on new elements
    for(int i = oldLength; i < newSize; ++i)
        new (&this->ptr[i]) t;
}

template<typename t>
Array<t> Array<t>::CopyToArena(Arena* to)
{
    Array<t> result = *this;
    result.ptr = (t*)Arena_AllocAndCopy(to, this->ptr, sizeof(t) * this->length);
    return result;
}

// PtrMap

uint32 PtrHashFunction(uintptr ptr)
{
    // If 32-bit is ever considered, update the implementation to include 32-bit
#ifdef Env32Bit
#error "PtrHashFunction Implementation currently doesn't support 32-bit."
#endif
    
    uint32 result;
    ptr = (~ptr) + (ptr << 21);
    ptr = ptr ^ (ptr >> 24);
    ptr = (ptr + (ptr << 3)) + (ptr << 8);
	ptr = ptr ^ (ptr >> 14);
    ptr = (ptr + (ptr << 2)) + (ptr << 4);
    ptr = ptr ^ (ptr << 28);
	result = (uint32)ptr;
    return result;
}

void String::Append(Arena* a, char element)
{
    // If empty, allocate a new array
    if(this->length == 0)
        this->ptr = Arena_FromStack(a, element);
    else
    {
        auto ptr = (char*)Arena_ResizeLastAlloc(a, this->ptr,
                                                sizeof(char) * this->length,
                                                sizeof(char) * (this->length + 1));
        Assert(ptr == this->ptr && "Adding an element to an array that was not the last allocation performed in the linear arena is not allowed");
        this->ptr = ptr;
        this->ptr[this->length] = element;
    }
    
    ++this->length;
}

String String::CopyToArena(Arena* to)
{
    String result = *this;
    result.ptr = (char*)Arena_AllocAndCopy(to, this->ptr, sizeof(char) * this->length);
    return result;
}

// Array utilities
template<typename t>
void DynArray<t>::Append(t element)
{
    this->Resize(this->length + 1);
    this->ptr[this->length - 1] = element;
}

template<typename t>
void DynArray<t>::InsertAtIdx(Array<t> elements, int idx)
{
    Assert(idx < this->length);
    this->Resize(this->length + elements.length);
    
    int count = this->length - idx - 1;
    int shiftBy = elements.length;
    memmove(&this->ptr[idx + shiftBy], &this->ptr[idx], count*sizeof(t));
    memcpy(&this->ptr[idx], elements.ptr, elements.length);
}

// Specialized for single element, probably faster
template<typename t>
void DynArray<t>::InsertAtIdx(t element, int idx)
{
    Assert(idx < this->length && idx >= 0);
    this->Resize(this->length + 1);
    
    int count = this->length - idx - 1;
    memmove(&this->ptr[idx + 1], &this->ptr[idx], count*sizeof(t));
    this->ptr[idx] = element;
}

template<typename t>
t* DynArray<t>::Reserve()
{
    this->Resize(this->length + 1);
    return &this->ptr[this->length - 1];
}

// TODO: the buffer doesn't shrink when reducing
// the size

template<typename t>
void DynArray<t>::Resize(uint32 newSize)
{
    this->length = newSize;
    if(this->capacity < this->length)
    {
        if(this->capacity < DynArray_MinCapacity)
            capacity = DynArray_MinCapacity;
        else
            this->capacity = this->length * 3 / 2;
        
        this->ptr = (t*)realloc(this->ptr, sizeof(t) * this->capacity);
        Assert(this->ptr && "realloc failed");
    }
}

template<typename t>
void DynArray<t>::ResizeAndInit(uint32 newSize)
{
    uint32 oldLength = this->length;
    this->Resize(newSize);
    
    // Call default constructor for new elements
    for(int i = oldLength; i < newSize; ++i)
        new (&this->ptr[i]) t;
}

template<typename t>
Array<t> DynArray<t>::ConvertToArray()
{
    Array<t> res = { this->ptr, this->length };
    return res;
}

template<typename t>
void DynArray<t>::FreeAll()
{
    free(this->ptr);
    this->ptr      = 0;
    this->capacity = 0;
    this->length   = 0;
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
    int i = 0;
    while(i < s1.length)
    {
        if(s2[i] == 0 || s1[i] != s2[i])
            return false;
        ++i;
    }
    
    return s2[i] == 0;
}

// Copied from the operator above because another
// function call might slow down the program if lots
// of string compares are being performed
bool operator ==(char* s1, String s2)
{
    int i = 0;
    while(i < s2.length)
    {
        if(s1[i] == 0 || s2[i] != s1[i])
            return false;
        ++i;
    }
    
    return s1[i] == 0;
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

bool String_FirstCharsMatchEntireString(String stream, char* str)
{
    for(int i = 0; str[i] != 0; ++i)
    {
        if(i >= stream.length || stream[i] != str[i])
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

Arena* GetScratchArena(ThreadContext* threadCtx, int idx)
{
    Assert(idx < ThreadCtx_NumScratchArenas && idx >= 0);
    return &threadCtx->scratchPool[idx];
}

#ifdef Profile
void InitSpall()
{
    spallCtx = spall_init_file("constellate.spall", 1000000.0 / GetRdtscFreq());
    
    size_t bufferSize = GB(1);
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

ProfileFuncGuard::ProfileFuncGuard(char* funcName, uint64 stringLength)
{
    spall_buffer_begin(&spallCtx, &spallBuffer, funcName, stringLength, __rdtsc());
}

ProfileFuncGuard::~ProfileFuncGuard()
{
    spall_buffer_end(&spallCtx, &spallBuffer, __rdtsc());
}

#endif  /* Profile */