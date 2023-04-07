
#pragma once

#include "llvm-c/Analysis.h"
#include "llvm-c/Core.h"
#include "llvm-c/ExecutionEngine.h"

#include "base.h"
#include "parser.h"
#include "memory_management.h"

struct IR_Context
{
    Parser* parser;
    
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMValueRef currentFunction;
    
    SymbolTable globalSymTable;
    SymbolTable* curSymTable = 0;
    
    uint32* curScopeStack = 0;
    uint32 curScopeIdx = 0;
    uint32 scopeCounter = 0;
    Arena* scopeStackArena;
    
    Arena* arena;
    Arena* tempArena;
    
    // For error printing
    char* fileContents;
    bool errorOccurred = false;
};

IR_Context InitIRContext(Arena* arena, Arena* tempArena, Arena* scopeStackArena, Parser* parser);
int GenerateIR(IR_Context *ctx, Ast_Root *root);
