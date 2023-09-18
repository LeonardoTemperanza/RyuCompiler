
#include "base.h"
#include "lexer.h"
#include "parser.h"
#include "dependency_graph.h"
//#include "llvm_ir_generation.h"
#include "semantics.h"
#include "interpreter.h"

#include "tilde_codegen.h"

int main(int argCount, char** argValue)
{
    // OS-specific initialization
    OS_Init();
    
#ifdef Profile
    InitSpall();
    defer(QuitSpall());
#endif
    
    // Start of application
    ProfileFunc(prof);
    
    // TODO: Command-line argument parsing
    
    // Main thread context
    ThreadContext threadCtx;
    ThreadCtx_Init(&threadCtx, GB(2), KB(32));
    SetThreadContext(&threadCtx);
    
    // Start of application
    ProfileFunc(prof2);
    
    if(argCount == 1)
    {
        fprintf(stderr, "Incorrect Usage: source file not given\n");
        return 1;
    }
    
    char* fileContents = ReadEntireFileIntoMemoryAndNullTerminate(argValue[1]);
    if(!fileContents)
        return 1;
    
    // 4GB of allocated -virtual- space. Initially, memory
    // is only reserved, not committed. Only 1GB of virtual
    // address space is reserved, while memory is actually
    // committed *commitSize* bytes at a time.
    size_t size = GB(1);
    size_t commitSize = MB(2);
    
    Arena astArena = Arena_VirtualMemInit(size, commitSize);
    Arena internArena = Arena_VirtualMemInit(size, commitSize);
    
    Tokenizer tokenizer = { fileContents, fileContents };
    tokenizer.arena = &astArena;
    Parser parser = { &astArena, &tokenizer };
    parser.internArena = &internArena;
    
    // Main stuff
    Ast_Block globalScope;
    parser.globalScope = &globalScope;
    
    LexFile(&tokenizer);
    Ast_FileScope* fileAst = ParseFile(&parser);
    
    if(tokenizer.status == CompStatus_Success)
        printf("Parsing was successful\n");
    else
    {
        printf("There were syntax errors!\n");
        return 1;
    }
    
    // Interning
    Array<Array<ToIntern>> intern = { &parser.internArray, 1 };
    Atom_InternStrings(intern);
    
    // Main program loop
    int outcome = MainDriver(&parser, fileAst);
    
    if(outcome == 0)
        printf("Compilation was successful\n");
    else
    {
        printf("There were semantic errors!\n");
        return 1;
    }
    
    printf("Interpreter codegen test:\n");
    
    Interp interp = Interp_Init();
    for_array(i, fileAst->scope.stmts)
    {
        auto node = fileAst->scope.stmts[i];
        if(node->kind == AstKind_ProcDef)
        {
            auto proc = Interp_ConvertProc(&interp, (Ast_ProcDef*)node);
            Interp_PrintProc(proc);
        }
    }
    
    printf("Tilde codegen test:\n");
    
    Tc_TestCode(fileAst, &interp);
    
    return outcome;
}

