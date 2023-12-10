
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

// Slice utilities
template<typename t>
void Slice<t>::Append(Arena* a, t element)
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
void Slice<t>::Resize(Arena* a, uint32 newSize)
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
void Slice<t>::ResizeAndInit(Arena* a, uint32 newSize)
{
    uint32 oldLength = this->length;
    this->Resize(a, newSize);
    
    // Call default constructor on new elements
    for(int i = oldLength; i < newSize; ++i)
        new (&this->ptr[i]) t;
}

template<typename t>
Slice<t> Slice<t>::CopyToArena(Arena* to)
{
    Slice<t> result = *this;
    result.ptr = (t*)Arena_AllocAndCopy(to, this->ptr, sizeof(t) * this->length);
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

uint64 HashString(String str, uint64 seed)
{
    uint64 hash = seed;
    for(int i = 0; i < str.length; ++i)
        hash = RotateLeft(hash, 9) + (uchar)str.ptr[i];
    
    // Thomas Wang 64-to-32 bit mix function, hopefully also works in 32 bits
    hash ^= seed;
    hash = (~hash) + (hash << 18);
    hash ^= hash ^ RotateRight(hash, 31);
    hash = hash * 21;
    hash ^= hash ^ RotateRight(hash, 11);
    hash += (hash << 6);
    hash ^= RotateRight(hash, 22);
    return hash + seed;
}

uint64 HashString(char* str, uint64 seed)
{
    uint64 hash = seed;
    while(*str)
        hash = RotateLeft(hash, 9) + (uchar)*str++;
    
    // Thomas Wang 64-to-32 bit mix function, hopefully also works in 32 bits
    hash ^= seed;
    hash = (~hash) + (hash << 18);
    hash ^= hash ^ RotateRight(hash, 31);
    hash = hash * 21;
    hash ^= hash ^ RotateRight(hash, 11);
    hash += (hash << 6);
    hash ^= RotateRight(hash, 22);
    return hash + seed;
}

// Slice utilities
template<typename t>
void Array<t>::Append(t element)
{
    this->Resize(this->length + 1);
    this->ptr[this->length - 1] = element;
}

template<typename t>
void Array<t>::InsertAtIdx(Slice<t> elements, int idx)
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
void Array<t>::InsertAtIdx(t element, int idx)
{
    Assert(idx < this->length && idx >= 0);
    this->Resize(this->length + 1);
    
    int count = this->length - idx - 1;
    memmove(&this->ptr[idx + 1], &this->ptr[idx], count*sizeof(t));
    this->ptr[idx] = element;
}

template<typename t>
t* Array<t>::Reserve()
{
    this->Resize(this->length + 1);
    return &this->ptr[this->length - 1];
}

// TODO: the buffer doesn't shrink when reducing
// the size

template<typename t>
void Array<t>::Resize(uint32 newSize)
{
    this->length = newSize;
    if(this->capacity < this->length)
    {
        if(this->capacity < Array_MinCapacity)
            capacity = Array_MinCapacity;
        else
            this->capacity = this->length * 3 / 2;
        
        this->ptr = (t*)realloc(this->ptr, sizeof(t) * this->capacity);
        Assert(this->ptr && "realloc failed");
    }
}

template<typename t>
void Array<t>::ResizeAndInit(uint32 newSize)
{
    uint32 oldLength = this->length;
    this->Resize(newSize);
    
    // Call default constructor for new elements
    for(int i = oldLength; i < newSize; ++i)
        new (&this->ptr[i]) t;
}

template<typename t>
void Array<t>::FreeAll()
{
    free(this->ptr);
    this->ptr      = 0;
    this->capacity = 0;
    this->length   = 0;
}

template<typename k, typename v>
void HashTable<k, v>::Init(uint32 capacity)
{
    this->capacity = capacity;
    uint64 numBytes = sizeof(HashTableEntry<k, v>) * capacity;
    entries = (HashTableEntry<k, v>*)malloc(numBytes);
    memset(entries, 0, numBytes);
}

template<typename k, typename v>
uint32 HashTable<k, v>::HashFunction(uintptr key)
{
    uint32 res;
#if 1
    // 64 bit
    key = (~key) + (key << 21);
	key = key ^ (key >> 24);
	key = (key + (key << 3)) + (key << 8);
	key = key ^ (key >> 14);
	key = (key + (key << 2)) + (key << 4);
	key = key ^ (key << 28);
	res = (uint32)key;
#else
    // 32-bit
    u32 state = (cast(u32)key) * 747796405u + 2891336453u;
	u32 word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	res = (word >> 22u) ^ word;
#endif
    
    return res;
}

template<typename k, typename v>
v* HashTable<k, v>::Get(k key)
{
    uint32 hash = HashFunction((uintptr)key);
    for(int i = 0; i <= count + 1; ++i)
    {
        Assert(i != count + 1);
        
        uint32 idx = HashTable_ProbingScheme(hash, i, capacity);
        if(entries[idx].occupied && entries[idx].key == key)
            return &entries[idx].val;
        else return 0;
    }
    
    return 0;
}

template<typename k, typename v>
void HashTable<k, v>::Add(k key, v val)
{
    uint32 hash = HashFunction((uintptr)key);
    for(int i = 0; i <= count + 1; ++i)
    {
        Assert(i != count + 1);
        
        uint32 idx = HashTable_ProbingScheme(hash, i, capacity);
        if(entries[idx].occupied) continue;
        
        // Not occupied, add new value
        entries[idx].key = key;
        entries[idx].occupied = true;
        entries[idx].val = val;
        ++count;
        break;
    }
    
    if((float)count / capacity > HashTable_LoadFactor)
        Grow(capacity * 2);
}

template<typename k, typename v>
void HashTable<k, v>::Grow(uint32 newSize)
{
    uint64 numBytes = sizeof(HashTableEntry<k, v>) * newSize;
    auto newEntries = (HashTableEntry<k, v>*)malloc(numBytes);
    memset(newEntries, 0, numBytes);
    
    for(int i = 0; i < capacity; ++i)
    {
        if(!entries[i].occupied) continue;
        // TODO: @performance Instead of recalculating the hash,
        // They could all just be stored in a separate array. Not
        // sure if that's faster though
        uint32 hash = HashFunction((uintptr)entries[i].key);
        
        for(int j = 0; j <= count + 1; ++j)
        {
            Assert(j != count + 1);
            
            int idx = HashTable_ProbingScheme(hash, j, newSize);
            if(newEntries[idx].occupied) continue;
            
            newEntries[idx] = entries[i];
            break;
        }
    }
    
    free(entries);
    entries = newEntries;
    capacity = newSize;
}

template<typename k, typename v>
void HashTable<k, v>::Free()
{
    free(entries);
}

cforceinline uint32 HashTable_ProbingScheme(uint32 hash, uint32 iter, uint32 length)
{
    // Linear
    return (hash + iter) % length;
}

template<typename v>
void StringTable<v>::Init(uint32 capacity)
{
    this->capacity = capacity;
    uint64 numBytes = sizeof(StringTableEntry<v>) * capacity;
    entries = (StringTableEntry<v>*)malloc(numBytes);
    memset(entries, 0, numBytes);
}

template<typename v>
uint64 StringTable<v>::HashFunction(String key)
{
    return HashString(str);
}

template<typename v>
v* StringTable<v>::Get(String key)
{
    uint32 hash = HashFunction((uintptr)key);
    for(int i = 0; i <= count + 1; ++i)
    {
        Assert(i != count + 1);
        
        uint32 idx = StringTable_ProbingScheme(hash, i, capacity);
        if(entries[idx].occupied && entries[idx].key == key)
            return &entries[idx].val;
        else return 0;
    }
    
    return 0;
}

template<typename v>
void StringTable<v>::Add(String key, v val)
{
    uint32 hash = HashFunction((uintptr)key);
    for(int i = 0; i <= count + 1; ++i)
    {
        Assert(i != count + 1);
        
        uint32 idx = StringTable_ProbingScheme(hash, i, capacity);
        if(entries[idx].occupied) continue;
        
        // Not occupied, add new value
        entries[idx].key = key;
        entries[idx].occupied = true;
        entries[idx].val = val;
        ++count;
        break;
    }
    
    if((float)count / capacity > StringTable_LoadFactor)
        Grow(capacity * 2);
}

template<typename v>
void StringTable<v>::Grow(uint32 newSize)
{
    uint64 numBytes = sizeof(StringTableEntry<k, v>) * newSize;
    auto newEntries = (StringTableEntry<k, v>*)malloc(numBytes);
    memset(newEntries, 0, numBytes);
    
    for(int i = 0; i < capacity; ++i)
    {
        if(!entries[i].occupied) continue;
        // TODO: @performance Instead of recalculating the hash,
        // They could all just be stored in a separate array
        uint32 hash = HashFunction((uintptr)entries[i].key);
        
        for(int j = 0; j <= count + 1; ++j)
        {
            Assert(j != count + 1);
            
            int idx = StringTable_ProbingScheme(hash, j, newSize);
            if(newEntries[idx].occupied) continue;
            
            newEntries[idx] = entries[i];
            break;
        }
    }
    
    free(entries);
    entries = newEntries;
    capacity = newSize;
}

template<typename v>
void StringTable<v>::Free()
{
    Arena_TempEnd(temp);
    free(entries);
}

cforceinline uint32 StringTable_ProbingScheme(uint32 hash, uint32 iter, uint32 length)
{
    // Linear
    return (hash + iter) % length;
}

int numDigits(int n)
{
    int r = 1;
    if(n < 0)
        n = (n == INT_MIN) ? INT_MAX : -n;
    
    while(n > 9)
    {
        n /= 10;
        r++;
    }
    
    return r;
}

void StringBuilder::Append(String str)
{
    if(this->string.ptr)
    {
        auto ptr = (char*)Arena_ResizeLastAlloc(this->arena, this->string.ptr,
                                                sizeof(char) * this->string.length,
                                                sizeof(char) * (this->string.length + str.length));
        Assert(ptr == this->string.ptr && "Appending to a string that was not the last allocation performed in the linear arena is not allowed");
        this->string.ptr = ptr;
        memcpy((void*)(this->string.ptr + this->string.length), str.ptr, str.length);
        this->string.length += str.length;
    }
    else
    {
        this->string.ptr = Arena_AllocArray(this->arena, str.length, char);
        memcpy(this->string.ptr, str.ptr, str.length);
        this->string.length = str.length;
    }
}

void StringBuilder::Append(char* str)
{
    auto strLength = strlen(str);
    if(this->string.ptr)
    {
        auto ptr = (char*)Arena_ResizeLastAlloc(this->arena, this->string.ptr,
                                                sizeof(char) * this->string.length,
                                                sizeof(char) * (this->string.length + strLength));
        
        Assert(ptr == this->string.ptr && "Appending to a string that was not the last allocation performed in the linear arena is not allowed");
        this->string.ptr = ptr;
        memcpy((void*)(this->string.ptr + this->string.length), str, strLength);
        this->string.length += strLength;
    }
    else
    {
        this->string.ptr = Arena_AllocArray(this->arena, strLength, char);
        memcpy(this->string.ptr, str, strLength);
        this->string.length = strLength;
    }
}

void StringBuilder::Append(char c)
{
    if(this->string.ptr)
    {
        auto ptr = (char*)Arena_ResizeLastAlloc(this->arena, this->string.ptr,
                                                sizeof(char) * this->string.length,
                                                sizeof(char) * (this->string.length + 1));
        
        Assert(ptr == this->string.ptr && "Appending to a string that was not the last allocation performed in the linear arena is not allowed");
        this->string.ptr = ptr;
        this->string.ptr[this->string.length] = c;
        this->string.length += 1;
    }
    else
    {
        this->string.ptr = Arena_AllocArray(this->arena, 1, char);
        this->string.ptr[0] = c;
        this->string.length = 1;
    }
}

void StringBuilder::Append(int numStr, ...)
{
    Assert(numStr > 0);
    
    va_list list;
    va_start(list, numStr);
    
    for(int i = 0; i < numStr; ++i)
        this->Append(va_arg(list, String));
    
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

// Exits if it sees a null terminator,
// only checks if the first 
bool StringBeginsWith(char* stream, String str)
{
    for(int i = 0; i < str.length; ++i)
    {
        if(stream[i] == 0 || stream[i] != str[i])
            return false;
    }
    
    return true;
}

bool StringBeginsWith(String stream, char* str)
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
    spallCtx = spall_init_file("ryu_profile.spall", 1000000.0 / GetRdtscFreq());
    
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
    spall_buffer_begin(&spallCtx, &spallBuffer, funcName, stringLength, (double)__rdtsc());
}

ProfileFuncGuard::~ProfileFuncGuard()
{
    spall_buffer_end(&spallCtx, &spallBuffer, (double)__rdtsc());
}

#endif  /* Profile */