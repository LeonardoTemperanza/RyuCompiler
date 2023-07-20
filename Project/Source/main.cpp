
#include "base.h"
#include "lexer.h"
#include "parser.h"
//#include "llvm_ir_generation.h"
#include "semantics.h"

int MainDriver(Tokenizer* tokenizer,
               Parser* parser,
               Typer* typer);

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
    
    Arena astArena;
    void* astStorage = ReserveMemory(size);
    Assert(astStorage);
    Arena_Init(&astArena, astStorage, size, commitSize);
    
    Arena typeArena;
    void* typeStorage = ReserveMemory(size);
    Assert(typeStorage);
    Arena_Init(&typeArena, typeStorage, size, commitSize);
    
    Tokenizer tokenizer = { fileContents, fileContents };
    tokenizer.arena = &astArena;
    Parser parser = { &astArena, &tokenizer };
    Typer typer = InitTyper(&typeArena, &parser);
    
    int result = MainDriver(&tokenizer, &parser, &typer);
    return result;
}

int MainDriver(Tokenizer* tokenizer, Parser* parser, Typer* typer)
{
    ProfileFunc(prof);
    
    Ast_Block globalScope;
    parser->globalScope = &globalScope;
    
    LexFile(tokenizer);
    Ast_Block* fileAst = ParseFile(parser);
    
    Assert(fileAst->kind == AstKind_Block);
    
    if(tokenizer->status == CompStatus_Success)
        printf("Parsing was successful\n");
    else
    {
        printf("There were syntax errors!\n");
        return 1;
    }
    
    CheckBlock(typer, fileAst);
    
    if(tokenizer->status == CompStatus_Success)
        printf("Typechecking was successful\n");
    else
    {
        printf("There were semantic errors!\n");
        return 1;
    }
    
    return 0;
}
