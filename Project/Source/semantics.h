
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
    
    DepGraph* graph;
};

Typer InitTyper(Arena* arena, Parser* parser);
void SemanticError(Typer* t, Token* token, String message);
void SemanticErrorContinue(Typer* t, Token* token, String message);
void CannotConvertToScalarTypeError(Typer* t, TypeInfo* type, Token* where);
void CannotConvertToIntegralTypeError(Typer* t, TypeInfo* type, Token* where);
void CannotDereferenceTypeError(Typer* t, TypeInfo* type, Token* where);
void IncompatibleTypesError(Typer* t, TypeInfo* type1, TypeInfo* type2, Token* where);

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
bool CheckMultiAssign(Typer* t, Ast_MultiAssign* stmt);
bool CheckNumLiteral(Typer* t, Ast_NumLiteral* expr);
Ast_Declaration* CheckIdent(Typer* t, Ast_IdentExpr* expr);
bool CheckFuncCall(Typer* t, Ast_FuncCall* call, bool isMultiAssign);
bool CheckBinExpr(Typer* t, Ast_BinaryExpr* expr);
bool CheckUnaryExpr(Typer* t, Ast_UnaryExpr* expr);
bool CheckTypecast(Typer* t, Ast_Typecast* expr);
bool CheckSubscript(Typer* t, Ast_Subscript* expr);
bool CheckMemberAccess(Typer* t, Ast_MemberAccess* expr);
TypeInfo* CheckDeclOrExpr(Typer* t, Ast_Node* node);
TypeInfo* CheckDeclOrExpr(Typer* t, Ast_Node* node, Ast_Block* curScope);
bool CheckType(Typer* t, TypeInfo* type, Token* where);
bool ComputeSize(Typer* t, Ast_StructDef* structDef);
struct ComputeSize_Ret { uint32 size; uint32 align; };
ComputeSize_Ret ComputeSize(Typer* t, Ast_DeclaratorStruct* declStruct, Token* errTok, bool* outcome);

// Identifier resolution
Ast_Declaration* IdentResolution(Typer* t, Ast_Block* scope, Atom* ident);
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
    
    return type->typeId == Typeid_Char || (type->typeId >= Typeid_Int8 &&
                                           type->typeId <= Typeid_Int64);
}

cforceinline bool IsTypeDereferenceable(TypeInfo* type)
{
    return type->typeId == Typeid_Ptr || type->typeId == Typeid_Arr;
}

cforceinline int GetTypeSizeBits(TypeInfo* type)
{
    int result = -1;
    if(type->typeId == Typeid_Bool ||
       type->typeId == Typeid_Char ||
       type->typeId == Typeid_Uint8 ||
       type->typeId == Typeid_Int8)
        result = 8;
    else if(type->typeId == Typeid_Uint16 ||
            type->typeId == Typeid_Int16)
        result = 16;
    else if(type->typeId == Typeid_Uint32 ||
            type->typeId == Typeid_Int32 ||
            type->typeId == Typeid_Float)
        result = 32;
    else if(type->typeId == Typeid_Uint64 ||
            type->typeId == Typeid_Int64 ||
            type->typeId == Typeid_Double)
        result = 64;
    
    return result;
}

// @temporary What do we do with structs??? (not idents)
cforceinline int GetTypeSize(TypeInfo* type)
{
    if(type->typeId >= 0 && type->typeId < StArraySize(typeSize))
        return typeSize[type->typeId];
    
    if(type->typeId == Typeid_Ptr ||
       type->typeId == Typeid_Proc)
        return 8;
    
    if(type->typeId == Typeid_Ident)
    {
        auto structDef = ((Ast_DeclaratorIdent*)type)->structDef;
        auto structDecl = Ast_GetDeclStruct(structDef);
        return structDecl->size;
    }
    
    if(type->typeId == Typeid_Struct)
    {
        auto structDecl = (Ast_DeclaratorStruct*)type;
        return structDecl->size;
    }
    
    Assert(false && "not implemented");
    return -1;
}

cforceinline int GetTypeAlign(TypeInfo* type)
{
    if(type->typeId >= 0 && type->typeId < StArraySize(typeSize))
        return typeSize[type->typeId];
    
    if(type->typeId == Typeid_Ptr ||
       type->typeId == Typeid_Proc)
        return 8;
    
    if(type->typeId == Typeid_Ident)
    {
        auto structDef = ((Ast_DeclaratorIdent*)type)->structDef;
        auto structDecl = Ast_GetDeclStruct(structDef);
        return structDecl->align;
    }
    
    if(type->typeId == Typeid_Struct)
    {
        auto structDecl = (Ast_DeclaratorStruct*)type;
        return structDecl->align;
    }
    
    Assert(false && "not implemented");
    return -1;
}

TypeInfo* GetBaseType(TypeInfo* type);
TypeInfo* GetBaseTypeShallow(TypeInfo* type);
bool TypesCompatible(Typer* t, TypeInfo* type1, TypeInfo* type2);
TypeInfo* GetCommonType(Typer* t, TypeInfo* type1, TypeInfo* type2);
bool TypesIdentical(Typer* t, TypeInfo* type1, TypeInfo* type2);
bool ImplicitConversion(Typer* t, Ast_Expr* exprSrc, TypeInfo* src, TypeInfo* dst);
String TypeInfo2String(TypeInfo* type, Arena* dest);
