
#pragma once

// Here we will figure out the types of everything,
// and also do type checking in the process.

#include "lexer.h"
#include "parser.h"

struct Typer
{
    Tokenizer* tokenizer;
    Parser* parser;
    
    Arena* arena;  // Arena where the types are stored
    
    Ast_Block* globalScope = 0;
    
    // TODO: What is this exactly?
    // Same thing here right? Array when small, hash-table when big, maybe?
    DynArray<OverloadSet> overloads = { 0, 0 };
};

Typer InitTyper(Arena* arena, Parser* parser);
void SemanticError(Typer* t, Token* token, String message);
void SemanticErrorContinue(Typer* t, Token* token, String message);
void CannotConvertToScalarTypeError(Typer* t, TypeInfo* type, Token* where);
void CannotDereferenceTypeError(Typer* t, TypeInfo* type, Token* where);
void IncompatibleTypesError(Typer* t, TypeInfo* type1, TypeInfo* type2, Token* where);

// Semantics
inline bool IsExprAssignable(Ast_Expr* expr)
{
    // Pointer dereferencing
    if(expr->kind == AstKind_UnaryExpr && ((Ast_UnaryExpr*)expr)->op == '*')
        return true;
    
    switch(expr->kind)
    {
        default: return false;
        case AstKind_Subscript:
        case AstKind_MemberAccess:
        case AstKind_Ident:
        return true;
    }
    
    return true;
}

TypeInfo* CheckExpression(Typer* t, Ast_Expr* expr, Ast_Block* block);
TypeInfo* CheckStmt(Typer* t, Ast_Stmt* stmt, Ast_Block* block);
void CheckBlock(Typer* t, Ast_Block* ast);
void TypingStage(Typer* typer, Ast_Node* ast, Ast_Block* curScope);
void TypingStage(Typer* t, Ast_Block* fileScope);

// Identifier resolution
Ast_Declaration* IdentResolution(Typer* t, Ast_Block* scope, String ident);
bool CheckRedefinition(Typer* t, Ast_Block* scope, Ast_Declaration* decl);
void AddDeclaration(Typer* t, Ast_Block* scope, Ast_Declaration* decl);

// Types
inline bool IsTypeScalar(TypeInfo* type)
{
    return type->typeId >= Typeid_Bool && type->typeId <= Typeid_Double;
}

inline bool IsTypeIntegral(TypeInfo* type)
{
    return type->typeId >= Typeid_Bool && type->typeId <= Typeid_Int64;
}

inline bool IsTypeFloat(TypeInfo* type)
{
    return type->typeId == Typeid_Float || type->typeId == Typeid_Double;
}

inline bool IsTypeSigned(TypeInfo* type)
{
    // Floating point types are automatically signed
    if(IsTypeFloat(type)) return true;
    
    return type->typeId >= Typeid_Char && type->typeId <= Typeid_Int64
        && type->typeId < Typeid_Uint8 && type->typeId > Typeid_Uint64;
}

inline bool IsTypeDereferenceable(TypeInfo* type)
{
    return type->typeId == Typeid_Ptr || type->typeId == Typeid_Arr;
}

TypeInfo* GetBaseType(TypeInfo* type);
bool TypesCompatible(Typer* t, TypeInfo* type1, TypeInfo* type2);
TypeInfo* GetCommonType(Typer* t, TypeInfo* type1, TypeInfo* type2);
bool TypesIdentical(Typer* t, TypeInfo* type1, TypeInfo* type2);
bool ImplicitConversion(Typer* t, Ast_Expr* exprSrc, TypeInfo* src, TypeInfo* dst);
String TypeInfo2String(TypeInfo* type, Arena* dest);