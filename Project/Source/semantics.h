
#pragma once

#include "lexer.h"
#include "parser.h"

struct DepGraph;

struct Typer
{
    Tokenizer* tokenizer;  // This will be an array of tokenizers I assume?
    
    Arena* arena;  // Arena where the types are stored
    
    Ast_Block* globalScope = 0;
    Ast_Block* curScope    = 0;
    Ast_FileScope* fileScope = 0;
    
    Ast_ProcDef* currentProc = 0;
    
    bool checkedReturnStmt = false;
    bool inLoopBlock   = false;
    bool inSwitchBlock = false;
    bool inDeferBlock  = false;
    
    DepGraph* graph;
    
    bool status = true;
};

Typer InitTyper(Arena* arena, Parser* parser);
void SemanticError(Typer* t, Token* token, String message, ...);
void SemanticErrorContinue(Typer* t, Token* token, String message, ...);
void CannotConvertToScalarTypeError(Typer* t, TypeInfo* type, Token* where);
void CannotConvertToIntegralTypeError(Typer* t, TypeInfo* type, Token* where);
void CannotDereferenceTypeError(Typer* t, TypeInfo* type, Token* where);
void IncompatibleTypesError(Typer* t, TypeInfo* type1, TypeInfo* type2, Token* where);
void IncompatibleReturnsError(Typer* t, Token* where, int numStmtRets, int numProcRets);

// Semantics
inline bool IsNodeLValue(Ast_Node* node)
{
    // Pointer dereferencing
    if(node->kind == AstKind_UnaryExpr && ((Ast_UnaryExpr*)node)->op == '*')
        return true;
    
    switch_nocheck(node->kind)
    {
        default: return false;
        case AstKind_Subscript:
        case AstKind_MemberAccess:
        case AstKind_Ident:
        case AstKind_VarDecl:
        return true;
    } switch_nocheck_end;
    
    return false;
}

bool IsNodeConst(Typer* t, Ast_Node* node);

bool CheckNode(Typer* t, Ast_Node* node);
bool CheckProcDecl(Typer* t, Ast_ProcDecl* decl);
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
bool CheckFallthrough(Typer* t, Ast_Fallthrough* stmt);
bool CheckMultiAssign(Typer* t, Ast_MultiAssign* stmt);
bool CheckConstValue(Typer* t, Ast_ConstValue* expr);
Ast_Declaration* CheckIdent(Typer* t, Ast_IdentExpr* expr);
bool CheckFuncCall(Typer* t, Ast_FuncCall* call, bool isMultiAssign);
bool CheckBinExpr(Typer* t, Ast_BinaryExpr* expr);
bool CheckUnaryExpr(Typer* t, Ast_UnaryExpr* expr);
bool CheckTypecast(Typer* t, Ast_Typecast* expr);
bool CheckSubscript(Typer* t, Ast_Subscript* expr);
bool CheckMemberAccess(Typer* t, Ast_MemberAccess* expr);
TypeInfo* CheckDeclOrExpr(Typer* t, Ast_Node* node);
TypeInfo* CheckCondition(Typer* t, Ast_Node* node);
bool CheckType(Typer* t, TypeInfo* type, Token* where);
bool ComputeSize(Typer* t, Ast_Node* node);
struct ComputeSize_Ret { uint64 size; uint64 align; };
bool FillInTypeSize(Typer* t, TypeInfo* type, Token* errTok);
ComputeSize_Ret ComputeStructSize(Typer* t, Ast_StructType* declStruct, Token* errTok, bool* outcome);

// Identifier resolution
Ast_Declaration* IdentResolution(Typer* t, Ast_Block* scope, Token* where, Atom* ident);
bool AddDeclaration(Typer* t, Ast_Block* scope, Ast_Declaration* decl);
bool CheckNotAlreadyDeclared(Typer* t, Ast_Block* scope, Ast_Declaration* decl);

// Types
cforceinline bool IsTypeScalar(TypeInfo* type)
{
    return type->typeId == Typeid_Bool || type->typeId == Typeid_Char
        || type->typeId == Typeid_Integer || type->typeId == Typeid_Ptr
        || type->typeId == Typeid_Float;
}

cforceinline bool IsTypeIntegral(TypeInfo* type)
{
    return type->typeId == Typeid_Bool || type->typeId == Typeid_Char
        || type->typeId == Typeid_Integer || type->typeId == Typeid_Ptr;
}

cforceinline bool IsTypeNumeric(TypeInfo* type)
{
    return type->typeId == Typeid_Char
        || type->typeId == Typeid_Integer
        || type->typeId == Typeid_Float;
}

cforceinline bool IsTypePrimitive(TypeInfo* type)
{
    return type->typeId <= Typeid_BasicTypesEnd && type->typeId >= Typeid_BasicTypesBegin;
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
