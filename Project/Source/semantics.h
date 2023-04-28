
#pragma once

// Here we will figure out the types of everything,
// and also do type checking in the process.

#include "lexer.h"
#include "parser.h"

struct Typer
{
    Tokenizer* tokenizer;
    Parser* parser;
    
    Arena* arena;  // Arena where the ast is stored
    Arena* tempArena;
    
    Ast_Block* globalScope = 0;
    // Same thing here right? Array when small, hash-table when big, maybe?
    DynArray<OverloadSet> overloads;
};

Typer InitTyper(Arena* arena, Parser* parser);
void SemanticError(Typer* t, Token* token, String message);

// Identifier resolution

Ast_Declaration* IdentResolution();
bool CheckRedefinition(Typer* t, Ast_Block* scope, Ast_Declaration* decl);
void AddDeclaration(Typer* t, Ast_Block* scope, Ast_Declaration* decl);

// Types
TypeInfo* GetBaseType(TypeInfo* type);
bool TypesCompatible(Typer* t, TypeInfo* type1, TypeInfo* type2);
bool TypesIdentical(Typer* t, TypeInfo* type1, TypeInfo* type2);
bool TypesImplicitlyCompatible(Typer* t, TypeInfo* type1, TypeInfo* type2);
String TypeInfo2String(TypeInfo* type, Arena* dest);

// Semantics
void Semantics_Expr(Typer* t, Ast_Expr* expr, Ast_Block* scope);
void Semantics_Block(Typer* t, Ast_Block* ast);
void TypingStage(Typer* typer, Ast_Node* ast, Ast_Block* curScope);
void TypingStage(Typer* t, Ast_Block* fileScope);