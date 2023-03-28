
#pragma once

#include "llvm-c/Analysis.h"
#include "llvm-c/Core.h"
#include "llvm-c/ExecutionEngine.h"

#include "base.h"
#include "parser.h"
#include "memory_management.h"

// The way the symbol table works needs to be reworked

enum TypeId
{
    TypeInt,
    TypeInt8,
    TypeInt16,
    TypeInt32,
    TypeInt64,
    TypeUInt8,
    TypeUInt16,
    TypeUInt32,
    TypeUInt64,
    TypeFloat,
    TypeDouble,
    TypeArray,
    TypeStruct
};

struct Symbol
{
    String ident;
    LLVMValueRef address;
    uint32 scopeId;
    
    // Type information
    TypeId type;
    
    
    Symbol* next = 0;
};

struct SymbolTableEntry
{
    bool occupied = false;
    Symbol symbol;
};

#define SymTable_ArraySize 200
struct SymbolTable
{
    SymbolTableEntry symArray[SymTable_ArraySize];
};

struct IR_Context
{
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

IR_Context InitIRContext(Arena* arena, Arena* tempArena, Arena* scopeStackArena, char* fileContents);
int GenerateIR(IR_Context *ctx, Ast_Root *root);