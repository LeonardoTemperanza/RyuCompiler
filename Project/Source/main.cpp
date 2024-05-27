
#include "base.h"
#include "lexer.h"
#include "parser.h"
#include "dependency_graph.h"
//#include "llvm_ir_generation.h"
#include "semantics.h"
#include "interpreter.h"
#include "bytecode_builder.h"
#include "messages.h"

#include "tilde_codegen.h"

struct FilePaths
{
    Array<char*> srcFiles;
    Array<char*> objFiles;
};

// Times measured in number of ticks returned by __rdtsc()
struct Timings
{
    double frontend = 0;
    double irGen = 0;
    double backend = 0;
    double linker = 0;
};

Timings timings;

FilePaths ParseCmdLineArgs(Slice<char*> args);

int main(int argCount, char** argValue)
{
    FilePaths filePaths = ParseCmdLineArgs({ argValue, argCount });
    defer( filePaths.srcFiles.FreeAll(); filePaths.objFiles.FreeAll(); );
    
    bool noFiles = filePaths.srcFiles.len <= 0 && filePaths.objFiles.len <= 0;
    
    // OS-specific initialization
    OS_Init();
    
    if(args.help)
        PrintHelp();
    else if(noFiles)
    {
        SetErrorColor();
        fprintf(stderr, "Error");
        ResetColor();
        fprintf(stderr, ": No input files.\n");
        return 1;
    }
    
    if(noFiles) return 0;
    
#ifdef Profile
    InitSpall();
    defer(QuitSpall());
#endif
    
    // Start of application
    ProfileFunc(prof);
    
    uint64 frontendTimeStart = __rdtsc();
    
    // Main thread context
    ThreadContext threadCtx;
    ThreadCtx_Init(&threadCtx, GB(2), KB(32));
    SetThreadContext(&threadCtx);
    
    char* fileContents = ReadEntireFileIntoMemoryAndNullTerminate(filePaths.srcFiles[0]);
    if(!fileContents)
        return 1;
    
    size_t size = GB(1);
    size_t commitSize = MB(2);
    Arena permArena   = Arena_VirtualMemInit(size, commitSize);
    Arena astArena    = Arena_VirtualMemInit(size, commitSize);
    Arena entityArena = Arena_VirtualMemInit(size, commitSize);
    
    String firstFilePath = { filePaths.srcFiles[0], (int64)strlen(filePaths.srcFiles[0]) };
    Tokenizer tokenizer = InitTokenizer(&astArena, fileContents, firstFilePath);
    Parser parser = InitParser(&astArena, &tokenizer, &permArena, &entityArena);
    
    // Main stuff
    LexFile(&tokenizer);
    Ast_FileScope* fileAst = ParseFile(&parser);
    
    bool status = !parser.foundError;
    if(status)
    {
        // Main program loop
        Interp interp;
        status &= MainDriver(&parser, &interp, fileAst);
    }
    
    timings.frontend += 1.0 / GetRdtscFreq() * (__rdtsc() - frontendTimeStart);
    
    if(status)
    {
        TestCodegen();
        //Tc_CodegenAndLink(fileAst, &interp, filePaths.objFiles);
    }
    
    if(args.time)
        PrintTimeBenchmark(timings.frontend, timings.irGen, timings.backend, timings.linker);
    
    // Render compilation messages
    SortMessages();
    ShowMessages();
    
    return !status;
}

// TODO This seems a bit too complicated, but it is c++ so not sure we can do better...
template<typename t>
t ParseArg(Slice<char*> args, int* at) { static_assert(false, "Unknown command line argument type."); }
template<> int ParseArg<int>(Slice<char*> args, int* at) { int val = atoi(args[*at]); ++*at; return val; }
template<> bool ParseArg<bool>(Slice<char*> args, int* at) { return true; }
template<> char* ParseArg<char*>(Slice<char*> args, int* at) { char* val = args[*at]; ++*at; return val; }

enum FileExtension
{
    File_Unknown,
    File_Ryu,
    File_Obj
};

constexpr FileExtension extensionTypes[] = { File_Ryu, File_Obj, File_Obj };
constexpr char* extensionStrings[] = { "ryu", "o", "obj" };

// The returned char* should not be freed individually, it's a substring of path
FileExtension GetExtensionFromPath(char* path, char** outExt)
{
    Assert(outExt);
    
    int dotIdx = -1;
    for(int i = 0; path[i] != 0; ++i)
    {
        if(path[i] == '.')
            dotIdx = i;  // Don't break, keep going to find the last one
    }
    
    if(dotIdx == -1)
    {
        // Ptr to null terminator
        *outExt = &path[strlen(path)];
        return File_Unknown;
    }
    
    // Found '.'
    *outExt = &path[dotIdx+1];
    
    for(int i = 0; i < StArraySize(extensionStrings); ++i)
    {
        if(strcmp(*outExt, extensionStrings[i]) == 0)
            return (FileExtension)extensionTypes[i];
    }
    
    return File_Unknown;
}

FilePaths ParseCmdLineArgs(Slice<char*> argStrings)
{
    FilePaths paths;
    
    // First argument is executable name
    int at = 1;
    while(at < argStrings.len)
    {
        String argStr = { argStrings[at], (int64)strlen(argStrings[at]) };
        
        if(argStr.len <= 0 || argStr[0] != '-')
        {
            // This is required to be a file name.
            
            // Find out the extension of the file
            char* extStr = 0;
            auto extType = GetExtensionFromPath(argStr.ptr, &extStr);
            if(extType == File_Ryu)
                paths.srcFiles.Append(argStr.ptr);
            else if(extType == File_Obj)
                paths.objFiles.Append(argStr.ptr);
            else if(extType == File_Unknown)
                fprintf(stderr, "Unknown file extension '.%s', will be ignored.\n", extStr);
            
            ++at;
            continue;
        }
        
        ++argStr.ptr;
        --argStr.len;
        
        // Use the X macro to parse arguments based on their type, default value, etc.
#define X(varName, string, type, defaultVal, desc) \
if(strcmp(argStr.ptr, string) == 0) { ++at; args.varName = ParseArg<type>(argStrings, &at); continue; }
        
        CmdLineArgsInfo
            
            // If not any other case
        {
            ++at;
            fprintf(stderr, "Unknown command line argument: '%.*s', will be ignored. Use '-h' for more info.\n", (int)argStr.len, argStr.ptr);
        }
#undef X
    }
    
    return paths;
}
