
#pragma once

#include "lexer.h"
#include "parser.h"

struct Typer
{
    Tokenizer* tokenizer;
    Parser* parser;
    
    Arena* arena;  // Arena where the types are stored
    
    Ast_Block* globalScope = 0;
    Ast_Block* curScope = 0;
    
    Ast_ProcDef* currentProc = 0;
    bool checkedReturnStmt = false;
};

Typer InitTyper(Arena* arena, Parser* parser);
void SemanticError(Typer* t, Token* token, String message);
void SemanticErrorContinue(Typer* t, Token* token, String message);
void CannotConvertToScalarTypeError(Typer* t, TypeInfo* type, Token* where);
void CannotConvertToIntegralTypeError(Typer* t, TypeInfo* type, Token* where);
void CannotDereferenceTypeError(Typer* t, TypeInfo* type, Token* where);
void IncompatibleTypesError(Typer* t, TypeInfo* type1, TypeInfo* type2, Token* where);

// Semantics
inline bool IsExprLValue(Ast_Expr* expr)
{
    // Pointer dereferencing
    if(expr->kind == AstKind_UnaryExpr && ((Ast_UnaryExpr*)expr)->op == '*')
        return true;
    
    switch_nocheck(expr->kind)
    {
        default: return false;
        case AstKind_Subscript:
        case AstKind_MemberAccess:
        case AstKind_Ident:
        return true;
    } switch_nocheck_end;
    
    return true;
}

bool CheckNode(Typer* t, Ast_Node* node);
bool CheckProcDef(Typer* t, Ast_ProcDef* proc);
bool CheckStructDef(Typer* t, Ast_StructDef* structDef);
bool CheckVarDecl(Typer* t, Ast_VarDecl* decl);
bool CheckBlock(Typer* t, Ast_Block* block);
bool CheckIf(Typer* t, Ast_If* stmt);
bool CheckFor(Typer* t, Ast_For* stmt);
bool CheckWhile(Typer* t, Ast_While* stmt);
bool CheckDoWhile(Typer* t, Ast_DoWhile* stmt);
bool CheckSwitch(Typer* t, Ast_Switch* stmt);
bool CheckDefer(Typer* t, Ast_Defer* stmt);
bool CheckReturn(Typer* t, Ast_Return* stmt);
bool CheckBreak(Typer* t, Ast_Break* stmt);
bool CheckContinue(Typer* t, Ast_Continue* stmt);
bool CheckNumLiteral(Typer* t, Ast_NumLiteral* expr);
Ast_Declaration* CheckIdent(Typer* t, Ast_IdentExpr* expr);
bool CheckFuncCall(Typer* t, Ast_FuncCall* call);
bool CheckBinExpr(Typer* t, Ast_BinaryExpr* expr);
bool CheckUnaryExpr(Typer* t, Ast_UnaryExpr* expr);
bool CheckTypecast(Typer* t, Ast_Typecast* expr);
bool CheckSubscript(Typer* t, Ast_Subscript* expr);
bool CheckMemberAccess(Typer* t, Ast_MemberAccess* expr);
TypeInfo* CheckDeclOrExpr(Typer* t, Ast_Node* node);
TypeInfo* CheckDeclOrExpr(Typer* t, Ast_Node* node, Ast_Block* curScope);
bool CheckType(Typer* t, TypeInfo* type, Token* where);
bool ComputeSize(Typer* t, Ast_StructDef* structDef);

// Identifier resolution
Ast_Declaration* IdentResolution(Typer* t, Ast_Block* scope, String ident);
bool CheckRedefinition(Typer* t, Ast_Block* scope, Ast_Declaration* decl);
bool AddDeclaration(Typer* t, Ast_Block* scope, Ast_Declaration* decl);

// Types
cforceinline bool IsTypeScalar(TypeInfo* type)
{
    return (type->typeId >= Typeid_Bool && type->typeId <= Typeid_Double) || type->typeId == Typeid_Ptr;
}

cforceinline bool IsTypeIntegral(TypeInfo* type)
{
    return type->typeId >= Typeid_Bool && type->typeId <= Typeid_Int64;
}

cforceinline bool IsTypeFloat(TypeInfo* type)
{
    return type->typeId == Typeid_Float || type->typeId == Typeid_Double;
}

cforceinline bool IsTypeSigned(TypeInfo* type)
{
    // Floating point types are automatically signed
    if(IsTypeFloat(type)) return true;
    
    return type->typeId >= Typeid_Char && type->typeId <= Typeid_Int64
        && type->typeId < Typeid_Uint8 && type->typeId > Typeid_Uint64;
}

cforceinline bool IsTypeDereferenceable(TypeInfo* type)
{
    return type->typeId == Typeid_Ptr || type->typeId == Typeid_Arr;
}

TypeInfo* GetBaseType(TypeInfo* type);
TypeInfo* GetBaseTypeShallow(TypeInfo* type);
bool TypesCompatible(Typer* t, TypeInfo* type1, TypeInfo* type2);
TypeInfo* GetCommonType(Typer* t, TypeInfo* type1, TypeInfo* type2);
bool TypesIdentical(Typer* t, TypeInfo* type1, TypeInfo* type2);
bool ImplicitConversion(Typer* t, Ast_Expr* exprSrc, TypeInfo* src, TypeInfo* dst);
String TypeInfo2String(TypeInfo* type, Arena* dest);
