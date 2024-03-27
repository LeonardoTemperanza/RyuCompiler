
#include "base.h"
#include "lexer.h"
#include "parser.h"
#include "dependency_graph.h"
//#include "llvm_ir_generation.h"
#include "semantics.h"
#include "interpreter.h"
#include "bytecode_builder.h"
#include "cmdline_args.h"

#include "tilde_codegen.h"

// Set at the start of main,
// can be used in any module
CmdLineArgs cmdLineArgs;

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
void PrintHelp();
template<typename t>
void PrintArg(char* argName, char* desc, t defVal, int lpad, int rpad);
void PrintTimings();

// TODO: @cleanup Push some of the stuff here to the proper modules
int main(int argCount, char** argValue)
{
    PrintNodeSizes();
    
    FilePaths filePaths = ParseCmdLineArgs({ argValue, argCount });
    defer({
              filePaths.srcFiles.FreeAll();
              filePaths.objFiles.FreeAll();
          });
    
    bool noFiles = filePaths.srcFiles.length <= 0 && filePaths.objFiles.length <= 0;
    
    if(cmdLineArgs.help)
    {
        PrintHelp();
        if(noFiles)
            return 0;
    }
    else if(noFiles)
    {
        OS_OutputColorInit();
        
        SetErrorColor();
        fprintf(stderr, "Error");
        ResetColor();
        fprintf(stderr, ": No input files.\n");
        return 1;
    }
    
    // OS-specific initialization
    OS_Init();
    
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
    Arena astArena = Arena_VirtualMemInit(size, commitSize);
    Arena internArena = Arena_VirtualMemInit(size, commitSize);
    Arena entityArena = Arena_VirtualMemInit(size, commitSize);
    
    Tokenizer tokenizer = InitTokenizer(&astArena, &internArena, fileContents, { filePaths.srcFiles[0], (int64)strlen(filePaths.srcFiles[0]) });
    Parser parser = { &astArena, &tokenizer };
    parser.entityArena = &entityArena;
    
    // Main stuff
    LexFile(&tokenizer);
    Ast_FileScope* fileAst = ParseFile(&parser);
    
    if(!parser.status)
    {
        printf("There were syntax errors!\n");
        return 1;
    }
    
#ifdef Debug
    fflush(stdout);
    fflush(stderr);
#endif
    
    // Main program loop
    Interp interp;
    bool status = MainDriver(&parser, &interp, fileAst);
    
    timings.frontend += 1.0 / GetRdtscFreq() * (__rdtsc() - frontendTimeStart);
    
    if(!status)
    {
        printf("There were semantic errors!\n");
        return 1;
    }
    
#ifdef Debug
    fflush(stdout);
    fflush(stderr);
#endif
    
    Tc_CodegenAndLink(fileAst, &interp, filePaths.objFiles);
    
    if(cmdLineArgs.time) PrintTimings();
    
    return !status;
}

template<typename t>
t ParseArg(Slice<char*> args, int* at) { static_assert(false, "Unknown command line argument type"); }
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

FilePaths ParseCmdLineArgs(Slice<char*> args)
{
    FilePaths paths;
    
    // First argument is executable name
    int at = 1;
    while(at < args.length)
    {
        String argStr = { args[at], (int64)strlen(args[at]) };
        
        if(argStr.length <= 0 || argStr[0] != '-')
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
        --argStr.length;
        
        // Use the X macro to parse arguments based on their type, default value, etc.
#define X(varName, string, type, defaultVal, desc) \
if(strcmp(argStr.ptr, string) == 0) { ++at; cmdLineArgs.varName = ParseArg<type>(args, &at); continue; }
        
        CmdLineArgsInfo
            
            // If not any other case
        {
            ++at;
            fprintf(stderr, "Unknown command line argument: '%.*s', will be ignored.\n", (int)argStr.length, argStr.ptr);
        }
#undef X
    }
    
    return paths;
}

void PrintHelp()
{
    constexpr int lpad = 5;
    constexpr int rpad = 40;
    
    printf("Ryu Compiler v0.0\n");
    printf("Usage: ryu file1.ryu [ options ]\n");
    
    printf("Options:\n");
    
    // Use the X macro to print the arguments
#define X(varName, string, type, defaultVal, desc) PrintArg<type>(string, desc, defaultVal, lpad, rpad);
    CmdLineArgsInfo
#undef X
}

// Only specializations can be used
template<typename t> int PrintArgAttributes(t defVal) { static_assert(false, "Unknown command line argument type"); }
template<> int PrintArgAttributes<int>(int defVal) { return printf("<int value> (default: %d)", defVal); }
template<> int PrintArgAttributes<bool>(bool defVal) { return 0; }
template<> int PrintArgAttributes<char*>(char* defVal) { return printf("<string value> (default: %s)", defVal); }

template<typename t>
void PrintArg(char* argName, char* desc, t defVal, int lpad, int rpad)
{
    // Print left padding
    printf("%*c", lpad, ' ');
    
    printf("-");
    int numChars = printf("%s", argName);
    numChars += printf(" ");
    
    numChars += PrintArgAttributes<t>(defVal);
    
    // Print right padding
    printf("%*c", max(0, rpad - numChars), ' ');
    
    printf("%s", desc);
    
    printf("\n");
}

void PrintTimings()
{
    const double totalExceptLinker = timings.frontend + timings.irGen + timings.backend;
    const double total = totalExceptLinker + timings.linker;
    const int pad = 19;
    int numChars = 0;
    
    printf("----- Timings -----\n");
    numChars = printf("Frontend:", timings.frontend);
    printf("%*c%lfs\n", max(1, pad - numChars), ' ', timings.frontend);
    numChars = printf("IR Generation:");
    printf("%*c%lfs\n", max(1, pad - numChars), ' ', timings.irGen);
    // TODO: Update this with the llvm backend
    numChars = printf("Backend (Tilde):");
    printf("%*c%lfs\n", max(1, pad - numChars), ' ', timings.backend);
    
    numChars = printf("Total (no link):");
    printf("%*c%lfs\n\n", max(1, pad - numChars), ' ', totalExceptLinker);
    
    // TODO: Linker name is platform dependant
    if(cmdLineArgs.useTildeLinker)
        numChars = printf("Linker (Tilde):");
    else
        numChars = printf("Linker (%s):", GetPlatformLinkerName());
    
    printf("%*c%lfs\n\n", max(1, pad - numChars), ' ', timings.linker);
    
    numChars = printf("Total:", total);
    printf("%*c%lfs\n", max(1, pad - numChars), ' ', total);
    
    printf("-------------------\n");
}
