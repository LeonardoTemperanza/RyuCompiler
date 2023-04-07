
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
void Array_AddElement(Array<t>* arr, Arena* a, t element)
{
    // If empty, allocate a new array
    if(arr->length == 0)
        arr->ptr = Arena_FromStack(a, element);
    else
    {
        auto ptr = (t*)Arena_ResizeLastAlloc(a, arr->ptr,
                                             sizeof(t) * arr->length,
                                             sizeof(t) * (arr->length + 1));
        Assert(ptr == arr->ptr && "You tried to resize an allocation that was not the last one");
        arr->ptr = ptr;
        arr->ptr[arr->length] = element;
    }
    
    ++arr->length;
}

template<typename t>
Array<t> Array_CopyToArena(Array<t> arr, Arena* to)
{
    arr.ptr = (t*)Arena_AllocAndCopy(to, arr.ptr, sizeof(t) * arr.length);
    return arr;
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

#ifdef Profile
void InitSpall()
{
    // Draw the amount of time this function takes, too.
    //uint64 beginTime = __rdtsc();
    
    spallCtx = spall_init_file("constellate.spall", 1000000.0 / GetRdtscFreq());
    
    size_t bufferSize = GB(1);
    uchar* buffer = (uchar*)malloc(bufferSize);
    memset(buffer, 1, bufferSize);
    
    spallBuffer.length = bufferSize;
    spallBuffer.data   = buffer;
    
    //spall_buffer_begin(&spallCtx, &spallBuffer, __FUNCTION__, sizeof(__FUNCTION__), beginTime);
    //spall_buffer_end(&spallCtx, &spallBuffer, __rdtsc());
}

void QuitSpall()
{
    spall_buffer_quit(&spallCtx, &spallBuffer);
    spall_quit(&spallCtx);
}
#endif