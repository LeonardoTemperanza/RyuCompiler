
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
        EPrint<"Error">();
        ResetColor();
        EPrintln<": No input files.">();
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
    
    // Lexing and parsing
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
        //Tc_CodegenAndLink(fileAst, &interp, filePaths.objFiles);
    }
    
    if(args.time)
        PrintTimeBenchmark(timings.frontend, timings.irGen, timings.backend, timings.linker);
    
    // Render compilation messages
    SortMessages();
    ShowMessages();
    
    return !status;
}
