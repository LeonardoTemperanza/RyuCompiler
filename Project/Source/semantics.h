
#pragma once

// Here we will figure out the types of everything,
// and also do type checking in the process.

// In the future this will have to work with the dependency graph

#include "lexer.h"
#include "parser.h"

void SemanticError(Token token, char* fileContents, char* message);

int IdentHashFunction(String str);
Symbol* GetSymbol(SymbolTable* symTable, String str, uint32* scopeStack, int scopeIdx);
Symbol* GetSymbolOrError(IR_Context* ctx, Token token);
struct InsertSymbol_Ret
{
    Symbol* res;
    bool alreadyDefined;
};
InsertSymbol_Ret InsertSymbol(Arena* arena, SymbolTable* symTable, String str, uint32 scopeId);
Symbol* InsertGlobalSymbolOrError(IR_Context* ctx, Token token);
Symbol* InsertSymbolOrError(IR_Context* ctx, Token token);
Symbol* GetGlobalSymbol(SymbolTable* symTable, String str);
Symbol* GetGlobalSymbolOrError(IR_Context* ctx, Token token);

// We need to change stuff about the error system, but we could do that later

void InsertPrimitiveTypes(SymbolTable* table);
void PerformTypingStage(Ast_Root* ast);
void TypeCheckExpr(Ast_ExprNode* expr);

void InsertPrimitiveTypes(SymbolTable* table)
{
    
}

void PerformTypingStage(Ast_Root* ast)
{
    printf("Performing typing stage!\n");
    
    for(int i = 0; i < ast->topStmts.length; ++i)
    {
        Ast_TopLevelStmt* topStmt = ast->topStmts[i];
        
        if(topStmt->type == Function)
        {
            
        }
        else if(topStmt->type == Prototype)
        {
            
        }
        else
            Assert(false && "Not yet implemented!");
    }
}

void TypeCheckExpr(Ast_ExprNode* expr)
{
    
}

