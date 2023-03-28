
#include "base.h"
#include "lexer.h"
#include "parser.h"
#include "llvm_ir_generation.h"

int MainDriver(Tokenizer* tokenizer,
               Parser* parser,
               IR_Context* ctx);

int main(int argCount, char** argValue)
{
#ifdef Profile
    InitSpall();
    defer(QuitSpall());
#endif
    
    ProfileFunc();
    
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
    
    Arena permanentArena;
    void* permanentStorage = ReserveMemory(size);
    Assert(permanentStorage);
    Arena_Init(&permanentArena, permanentStorage, size, commitSize);
    
    // This arena is used for temporary allocations, for
    // computations that are local to a single function
    Arena tempArena;
    void* tempStorage = ReserveMemory(size);
    Arena_Init(&tempArena, tempStorage, size, commitSize);
    
    Arena astArena;
    void* astStorage = ReserveMemory(size);
    Assert(astStorage);
    Arena_Init(&astArena, astStorage, size, commitSize);
    
    Arena scopeStackArena;
    void* scopeStackStorage = ReserveMemory(2048);
    Assert(scopeStackStorage);
    Arena_Init(&scopeStackArena, scopeStackStorage, 2048, 256);
    
    Tokenizer tokenizer = { fileContents, fileContents };
    
    Parser parser = { &tempArena, &astArena, &tokenizer };
    
    IR_Context IRCtx = InitIRContext(&permanentArena, &tempArena, &scopeStackArena, fileContents);
    
    int result = MainDriver(&tokenizer, &parser, &IRCtx);
    
    FreeMemory(permanentStorage, size);
    FreeMemory(tempStorage, size);
    FreeMemory(astStorage, size);
    return result;
}

int MainDriver(Tokenizer* tokenizer, Parser* parser, IR_Context* ctx)
{
    ProfileFunc();
    
    bool success = ParseRootNode(parser, tokenizer);
    if(!success)
        return 1;
    
    //InterpretTestCode();
    
    int errorCode = GenerateIR(ctx, &(parser->root));
    return 0;
}