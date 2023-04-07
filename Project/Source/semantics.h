
#pragma once

// Here we will figure out the types of everything,
// and also do type checking in the process.

#include "lexer.h"
#include "parser.h"

struct SymbolTable {};

struct Typer
{
    Parser* parser;
    
    Arena* arena;
    Arena* tempArena;
    
    Arena* scopeStackArena;
    uint32* curScopeStack = 0;
    uint32 curScopeIdx = 0;
    uint32 scopeCounter = 0;
    
    SymbolTable globalSymTable;
    SymbolTable* curSymTable;
    bool errorOccurred = false;
};

Typer InitTyper(Arena* arena, Arena* tempArena, Arena* scopeStackArena, Parser* parser)
{
    Typer t;
    return t;
}

/*
enum SymbolType
{
    IsSymbolType,
    IsSymbolFunc,
    IsSymbolVar
};

struct Symbol
{
    String ident;
    uint32 scopeId;
    
    SymbolType symType;
    union
    {
        // Ast_StructDecl* structDecl; // Struct declaration
        Ast_Declaration* decl;
        Ast_Function* func;
    };
    
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

Typer InitTyper(Arena* arena, Arena* tempArena, Arena* scopeStackArena, Parser* parser);

void SemanticError(Token token, Typer* ctx, char* message);

int IdentHashFunction(String str);
Symbol* GetSymbol(SymbolTable* symTable, String str, uint32* scopeStack, int scopeIdx);
Symbol* GetSymbolOrError(Typer* ctx, Token token);
struct InsertSymbol_Ret
{
    Symbol* res;
    bool alreadyDefined;
};
InsertSymbol_Ret InsertSymbol(Arena* arena, SymbolTable* symTable, String str, uint32 scopeId);
Symbol* InsertGlobalSymbolOrError(Typer* ctx, Token token);
Symbol* InsertSymbolOrError(Typer* ctx, Token token);
Symbol* GetGlobalSymbol(SymbolTable* symTable, String str);
Symbol* GetGlobalSymbolOrError(Typer* ctx, Token token);

// We need to change stuff about the error system, but we could do that later

void PerformTypingStage(Typer* ctx, Ast_Root* ast);

void InsertPrimitiveTypes(SymbolTable* table);
void PerformTypingStage(Typer* ctx, Ast_Root* ast);
void TypeCheckExpr(Typer* ctx, Ast_ExprNode* expr);

void Semantics_Block(Typer* ctx, Ast_BlockStmt* block);
void Semantics_Declaration(Typer* ctx, Ast_Declaration* decl);
void Semantics_Stmt(Typer* ctx, Ast_Stmt* stmt);
*/