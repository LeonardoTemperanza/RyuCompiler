
#include "memory_management.cpp"
#include "base.cpp"

#include <windows.h>
#include <stdio.h>
#include <iostream>
// Using this because C doesn't have a decent one and i don't feel like implementing this myself
#include <filesystem>

namespace fs = std::filesystem;

const char* testsPath = "../Project/Tests/";

// A compile-time mechanism would be a pain to set up
// in C++, so instead i chose a runtime approach here.
// This means it's more expensive and you don't get compile-time
// errors, but for this, it's fine. Again, not ideal, but good enough
// for now.

enum Type
{
    Type_Int,
    Type_Bool
};

struct Arg
{
    const char* name;
    Type type;
    void* value;
};

// Contains the valid strings, and the respective default values (as well as their types, implicitly)

static const Arg defArgs[] =
{
    {"compiles",     Type_Bool, (void*)true},
    {"program_exit", Type_Int,  (void*)0},
    {"ignore_test",  Type_Bool, (void*)false}
};

constexpr int numArgs = StArraySize(defArgs);

static const Arg nullArg = {"", Type_Bool, (void*)false};

void InitArgs(Arg (&args)[numArgs])
{
    for(int i = 0; i < StArraySize(args); ++i)
        args[i] = defArgs[i];
}

template<typename t>
Type GetType()
{
    if(typeid(t) == typeid(int64)) return Type_Int;
    if(typeid(t) == typeid(bool))  return Type_Bool;
    
    assert(!"GetType Not implemented for this type");
    return Type_Bool;
}

void PrintType(Type type, void* value)
{
    switch(type)
    {
        case Type_Int:
        {
            printf("%lld", (int64)value);
            break;
        }
        case Type_Bool:
        {
            if((bool)value)
                printf("true");
            else
                printf("false");
            
            break;
        }
        default:
        {
            assert(!"PrintType does not support this type");
            break;
        }
    }
}

// To be used with compile time constant strings
template<typename t>
t GetArg(const char* identifier, Arg (&args)[numArgs])
{
    for(int i = 0; i < numArgs; ++i)
    {
        if(strcmp(args[i].name, identifier) == 0)
        {
            assert(GetType<t>() == args[i].type);
            return (t)args[i].value;
        }
    }
    
    assert(!"Argument name not found");
    t zero = {0};
    return zero;
}

template<typename t>
void SetArg(const char* ident, t setTo, Arg (&args)[numArgs])
{
    bool found = false;
    Arg* ptr = GetRecord(ident, &found, args);
    assert(found && "Trying to access argument name which does not exist");
    if(!found) return;
    
    assert(GetType<t>() == ptr->type);
    ptr->value = (void*)setTo;
}

// To be used with any string (used for parsing)
Arg* GetRecord(String ident, bool* outFound, Arg (&args)[numArgs])
{
    assert(outFound);
    *outFound = true;
    
    for(int i = 0; i < StArraySize(args); ++i)
    {
        if(ident == (char*)args[i].name)
            return &args[i];
    }
    
    *outFound = false;
    return nullptr;
}

Arg* GetRecord(const char* ident, bool* outFound, Arg (&args)[numArgs])
{
    String str = {.ptr=(char*)ident, .length=(int64)strlen(ident)};
    return GetRecord(str, outFound, args);
}

//// @copypasta From compiler code
inline bool IsWhitespace(char c)
{
    return (c == ' ')
        || (c == '\t')
        || (c == '\v')
        || (c == '\f')
        || (c == '\r');
}

inline bool IsAlphabetic(char c)
{
    return ((c >= 'a') && (c <= 'z'))
        || ((c >= 'A') && (c <= 'Z'));
}

inline bool IsAllowedForStartIdent(char c)
{
    return IsAlphabetic(c)
        || c == '_';
}

inline bool IsNumeric(char c)
{
    return c >= '0' && c <= '9';
}

inline bool IsAllowedForMiddleIdent(char c)
{
    return IsAllowedForStartIdent(c)
        || IsNumeric(c);
}
////

void EatWhitespace(char** at, bool multilineComment);
bool ParseValue(char** at, const char* fileName, String ident, Arg (&args)[numArgs]);
bool ParseFile(const char* fileName, char* contents, Arg (&args)[numArgs]);
void RunAndCheckBehavior(const char* sourcePath, const char* exePath, int optLevel, Arg (&args)[numArgs]);

enum LaunchProcess_Outcome
{
    Success,
    Timeout,
    GenericFailure,
    CouldNotStart
};

int64 LaunchProcess(const char* cmd, int msTimeout, LaunchProcess_Outcome* outOutcome);

int main()
{
    printf("Ryu Tester\n");
    fflush(stdout);
    
    for(const fs::directory_entry& entry : fs::recursive_directory_iterator(testsPath))
    {
        fs::path path(entry.path());
        if(entry.is_regular_file() && path.extension() == ".ryu")
        {
            fs::path outExePath(path);
            outExePath.replace_extension(".exe");
            
            std::string fileNameStr = path.string();
            std::string exeNameStr = outExePath.string();
            const char* fileName = fileNameStr.c_str();
            const char* exeName  = exeNameStr.c_str();
            char* contents = ReadEntireFileIntoMemoryAndNullTerminate(fileName);
            
            Arg args[numArgs];
            InitArgs(args);
            
            bool ok = ParseFile(fileName, contents, args);
            if(ok && !GetArg<bool>("ignore_test", args))
            {
                RunAndCheckBehavior(fileName, exeName, 0, args);
            }
        }
    }
    
    return 0;
}

void EatWhitespace(char** at, bool multilineComment)
{
    bool cont = true;
    while(cont)
    {
        if(*at == '\0') break;
        
        if(multilineComment)
            cont = IsWhitespace(**at) || **at == '\n';
        else
            cont = IsWhitespace(**at);
        
        if(cont) ++*at;
    }
}

bool ParseValue(char** at, const char* fileName, String ident, Arg (&args)[numArgs])
{
    bool found = false;
    Arg* record = GetRecord(ident, &found, args);
    if(!found)
    {
        fprintf(stderr, "%s contains an unidentified directive: '%.*s'\n", fileName, (int32)ident.length, ident.ptr);
        return false;
    }
    
    switch(record->type)
    {
        case Type_Int:
        {
            char* endPtr = nullptr;
            errno = 0;
            int64 val = strtoll(*at, &endPtr, 10);
            bool convFailed = (val == 0 && errno != 0) || endPtr == *at;
            if(convFailed)
            {
                fprintf(stderr, "%s contains a syntax error, expecting an integer value for '%.*s' but got something else instead.\n", fileName, (int32)ident.length, ident.ptr);
                return false;
            }
            
            record->value = (void*)val;
            *at = endPtr;
            break;
        }
        case Type_Bool:
        {
            if(StringBeginsWith(*at, StrLit("true")))
            {
                record->value = (void*)true;
                *at += strlen("true");
            }
            else if(StringBeginsWith(*at, StrLit("false")))
            {
                record->value = (void*)false;
                *at += strlen("false");
            }
            else
            {
                fprintf(stderr, "%s contains a syntax error, expecting a boolean value (true|false) for '%.*s' but got something else instead.\n", fileName, (int32)ident.length, ident.ptr);
                return false;
            }
            break;
        }
        default:
        {
            assert(!"ParseValue not implemented for this type");
            break;
        }
    }
    
    return true;
}

bool ParseFile(const char* fileName, char* contents, Arg (&args)[numArgs])
{
    char* at = contents;
    while(IsWhitespace(*at) || *at == '\n') ++at;
    
    bool isMultiline = false;
    if(at[0] == '/' && at[1] == '*')  // Multiline comment
    {
        isMultiline = true;
        at += 2;  // Eat '/' and '*'
    }
    else if(at[0] == '/' && at[1] == '/')  // Single line comment
    {
        isMultiline = false;
        at += 2;  // Eat '/' and '/'
    }
    else return true;  // No comment, ignore file
    
    EatWhitespace(&at, isMultiline);
    
    // If the keyword for testing params is not present,
    // it's just a regular comment at the top of the file.
    const char* tag = "@test";
    bool commentTagPresent = false;
    if(StringBeginsWith(at, StrLit("@test")))
    {
        const char nextChar = at[strlen(tag)];
        commentTagPresent = nextChar == '\0' || IsWhitespace(nextChar);
    }
    
    if(!commentTagPresent) return true;
    
    at += strlen(tag);
    
    bool endComment = false;
    do
    {
        EatWhitespace(&at, isMultiline);
        
        if(*at == '\0') return true;
        
        // Parse identifier
        if(!IsAllowedForStartIdent(*at))
        {
            fprintf(stderr, "%s contains a syntax error, expecting keyword but got something else instead.\n", fileName);
            return false;
        }
        
        char* startIdent = at;
        
        do ++at;
        while(IsAllowedForMiddleIdent(*at) && *at != '\0');
        
        String ident = {.ptr=startIdent, .length=at-startIdent};
        EatWhitespace(&at, isMultiline);
        
        // Eat :
        if(*at != ':')
        {
            fprintf(stderr, "%s contains a syntax error, expected ':' following '%.*s'\n", fileName, (int32)ident.length, ident.ptr);
            return false;
        }
        
        ++at;
        
        EatWhitespace(&at, isMultiline);
        
        if(!ParseValue(&at, fileName, ident, args)) return false;
        
        EatWhitespace(&at, isMultiline);
        
        if(isMultiline)
            endComment = at[0] == '*' && at[1] == '/';
        else
            endComment = at[0] == '\n';
        
        endComment |= at[0] == '\0';
    }
    while(!endComment);
    
    return true;
}

// This only works on windows for now. I don't think we're going to need tests on
// other platforms anyways because there's not a lot of platform-dependent behavior
void RunAndCheckBehavior(const char* sourcePath, const char* exePath, int optLevel, Arg (&args)[numArgs])
{
    constexpr int bufSize = 1024;
    char cmdStr[bufSize];
    snprintf(cmdStr, bufSize, "ryu.exe %s -O %d -o %s", sourcePath, optLevel, exePath);
    
    defer(remove(exePath));
    
    // This describes the actual behavior of the program. It will
    // then be compared to the desired behavior of the program.
    Arg actualArgs[numArgs];
    for(int i = 0; i < numArgs; ++i)
        actualArgs[i] = args[i];
    
    const int msTimeout = 7000;
    LaunchProcess_Outcome outcome;
    LaunchProcess(cmdStr, msTimeout, &outcome);
    switch(outcome)
    {
        case Success:
        break;
        
        case Timeout:
        fprintf(stderr, "%s: Compiler took too long (longer than %ds) to generate the exe. Aborting.\n", sourcePath, msTimeout/1000);
        break;
        
        case GenericFailure:
        fprintf(stderr, "%s: Internal Error: Could not wait for the compiler to finish.\n", sourcePath);
        break;
        
        case CouldNotStart:
        fprintf(stderr, "%s: Internal Error: Could not launch compiler.\n", sourcePath);
        break;
        
        default:
        assert(!"Unknown outcome type");
        break;
    }
    
    FILE* toCheck = fopen(exePath, "r");
    if(toCheck)
    {
        fclose(toCheck);
        
        // Compiled program exists, so we can try to execute it
        SetArg<bool>("compiles", true, actualArgs);
        
        LaunchProcess_Outcome outcome;
        int64 exitCode = LaunchProcess(exePath, msTimeout, &outcome);
        switch(outcome)
        {
            case Success:
            break;
            
            case Timeout:
            fprintf(stderr, "%s: Compiled program took too long (longer than %ds) to generate the exe. Aborting.\n", sourcePath, msTimeout/1000);
            break;
            
            case GenericFailure:
            fprintf(stderr, "%s: Internal Error: Could not wait for the compiled program to finish.\n", sourcePath);
            break;
            
            case CouldNotStart:
            fprintf(stderr, "%s: Internal Error: Could not launch compiled program.\n", sourcePath);
            break;
            
            default:
            assert(!"Unknown outcome type");
            break;
        }
        
        SetArg("program_exit", exitCode, actualArgs);
    }
    else 
    {
        SetArg<bool>("compiles", false, actualArgs);
    }
    
    // Compare actual behavior to desired behavior
    int numErrors = 0;
    for(int i = 0; i < numArgs; ++i)
    {
        if(actualArgs[i].value != args[i].value)
            ++numErrors;
    }
    
    printf("  %s > ", sourcePath);
    
    if(numErrors > 0)
        printf("X! Violated constraints: ");
    else
        printf("V\n");
    
    if(numErrors > 0)
    {
        bool needComma = false;
        for(int i = 0; i < numArgs; ++i)
        {
            if(actualArgs[i].value != args[i].value)
            {
                if(needComma) printf(", ");
                
                printf("%s (should be '", actualArgs[i].name);
                PrintType(args[i].type, args[i].value);
                printf("', it's '");
                PrintType(actualArgs[i].type, actualArgs[i].value);
                printf("')");
                
                needComma = true;
            }
        }
        
        printf("\n");
    }
}

int64 LaunchProcess(const char* cmd, int msTimeout, LaunchProcess_Outcome* outOutcome)
{
    assert(outOutcome);
    *outOutcome = Success;
    DWORD exit = 0;
    
    STARTUPINFO startupInfo = {0};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.hStdInput  = nullptr;
    startupInfo.hStdOutput = nullptr;
    startupInfo.hStdError  = nullptr;
    startupInfo.dwFlags |= STARTF_USESTDHANDLES;
    
    PROCESS_INFORMATION processInfo;
    if(!CreateProcess(0, (char*)cmd, 0, 0, false, 0, 0, 0, &startupInfo, &processInfo))
    {
        *outOutcome = CouldNotStart;
        return exit;
    }
    
    DWORD res = WaitForSingleObject(processInfo.hProcess, msTimeout);
    
    if(res == WAIT_TIMEOUT)
        *outOutcome = Timeout;
    else if(res != WAIT_OBJECT_0)
        *outOutcome = GenericFailure;
    else
        GetExitCodeProcess(processInfo.hProcess, &exit);
    
    if(res != WAIT_OBJECT_0)
        TerminateProcess(processInfo.hProcess, 1);
    
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    
    return exit;
}