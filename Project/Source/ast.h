
// TODO(Leo): @refactor notes (more like ramblings):
// Should struct and ident just be the same? I mean, they are indeed exactly
// the same. It should just be an unnamed struct.
// But I guess it's because structs are considered declarations....
// I think the issue with the code is this:
// We always know when we're expecting a type, right?
// So, maybe type lookup should be different, as a procedure.

#pragma once

#include "base.h"
#include "dependency_graph.h"
#include "atom.h"

// This stuff is @temporary
struct TB_Node;
struct TB_Function;
struct TB_FunctionPrototype;
struct Interp_Symbol;

struct Ast_FileScope;

struct Ast_Expr;
struct Ast_Stmt;
struct Ast_Block;

// Directives
struct Ast_RunDir;

// Statements
struct Ast_If;
struct Ast_For;
struct Ast_While;
struct Ast_DoWhile;
struct Ast_Switch;
struct Ast_Defer;
struct Ast_Return;
struct Ast_Break;
struct Ast_Continue;
struct Ast_Fallthrough;

// Declarations
struct Ast_Declaration;
struct Ast_VarDecl;
struct Ast_ProcDef;
struct Ast_StructDef;

// Syntax-Trees related to types
struct TypeInfo;
struct Ast_PtrType;
struct Ast_ArrType;
struct Ast_ProcType;
struct Ast_IdentType;
struct Ast_StructType;

// Expressions
struct Ast_BinaryExpr;
struct Ast_UnaryExpr;
struct Ast_TernaryExpr;
struct Ast_FuncCall;
struct Ast_Subscript;
struct Ast_IdentExpr;
struct Ast_MemberAccess;
struct Ast_Typecast;

// Specifiers
// TODO: Many of these are not supported
enum Ast_DeclSpecEnum
{
    Decl_Extern         = 1 << 0,
    Decl_Persist        = 1 << 1,
    Decl_Local          = 1 << 2,
    Decl_CExtern        = 1 << 3,
    Decl_Atomic         = 1 << 4,
    Decl_ThreadLocal    = 1 << 5
};

typedef uint8 Ast_DeclSpec;

// Allowed specifiers
constexpr auto localVarDeclSpecs  = (Ast_DeclSpec) Decl_Persist;
constexpr auto globalVarDeclSpecs = (Ast_DeclSpec)(Decl_Extern | Decl_CExtern);
constexpr auto procDeclSpecs      = (Ast_DeclSpec)(Decl_Extern | Decl_CExtern);

cforceinline Ast_DeclSpec Ast_GetUnallowedDeclSpecs(Ast_DeclSpec specs, Ast_DeclSpec allowed)
{
    return (Ast_DeclSpec)(specs & (~allowed));
}

// Returns 0 if not a specifier
cforceinline Ast_DeclSpec Ast_TokTypeToDeclSpec(TokenType tokType)
{
    switch_nocheck(tokType)
    {
        case Tok_Extern: return Decl_Extern;
    } switch_nocheck_end;
    
    return (Ast_DeclSpec)0;
}

// Ast node types
enum Ast_NodeKind : uint8
{
    // Declarations
    AstKind_DeclBegin,
    AstKind_StructDef,
    AstKind_ProcDef,
    AstKind_VarDecl,
    AstKind_EnumDecl,
    AstKind_DeclEnd,
    
    // Statements
    AstKind_StmtBegin,
    AstKind_Block,
    AstKind_If,
    AstKind_For,
    AstKind_While,
    AstKind_DoWhile,
    AstKind_Switch,
    AstKind_Defer,
    AstKind_Return,
    AstKind_Break,
    AstKind_Continue,
    AstKind_Fallthrough,
    AstKind_MultiAssign,
    AstKind_EmptyStmt,
    AstKind_StmtEnd,
    
    // Expressions
    AstKind_ExprBegin,
    AstKind_Ident,
    AstKind_FuncCall,
    AstKind_BinaryExpr,
    AstKind_UnaryExpr,
    AstKind_TernaryExpr,
    AstKind_Typecast,
    AstKind_Subscript,
    AstKind_MemberAccess,
    AstKind_ConstValue,
    AstKind_RunDir,
    AstKind_ExprEnd
};

// Examples of types:
// [3][4]int arr;   3x4 matrix
// ^int ptr;        int pointer
// [3]^int ptrArr;  array of int pointers
// [3]^[4]int var;  array of pointers to static arrays of size 4
// vec3(float) v;   polymorphic compound type
// There are also going to be const qualifiers and all that jazz
// NOTE(Leo): Bigger value means more accurate (/bigger) type

enum TypeId : uint8
{
    Typeid_BasicTypesBegin = 0,
    Typeid_None = 0,
    Typeid_Bool,
    Typeid_Char,
    Typeid_Integer,
    Typeid_Float,
    Typeid_BasicTypesEnd = Typeid_Float,
    
    Typeid_Ptr,
    Typeid_Arr,
    Typeid_Proc,
    Typeid_Struct,
    Typeid_Ident,
    Typeid_Raw
};

struct TypeInfo
{
    TypeId typeId;
    bool isSigned;
    uint64 size;
    uint64 align;
};

// Primitive types
// TODO: these should be const
TypeInfo Typer_None   = { Typeid_None, false, 0, 0 };
TypeInfo Typer_Raw    = { Typeid_Raw,  false, 0, 0 };
TypeInfo Typer_Bool   = { Typeid_Bool, false, 1, 1 };
TypeInfo Typer_Char   = { Typeid_Char, false, 1, 1 };
TypeInfo Typer_Uint8  = { Typeid_Integer, false, 1, 1 };
TypeInfo Typer_Uint16 = { Typeid_Integer, false, 2, 2 };
TypeInfo Typer_Uint32 = { Typeid_Integer, false, 4, 4 };
TypeInfo Typer_Uint64 = { Typeid_Integer, false, 8, 8 };
TypeInfo Typer_Int8   = { Typeid_Integer, true, 1, 1 };
TypeInfo Typer_Int16  = { Typeid_Integer, true, 2, 2 };
TypeInfo Typer_Int32  = { Typeid_Integer, true, 4, 4 };
TypeInfo Typer_Int64  = { Typeid_Integer, true, 8, 8 };
TypeInfo Typer_Float  = { Typeid_Float, true, 4, 4 };
TypeInfo Typer_Double = { Typeid_Float, true, 8, 8 };

cforceinline TypeInfo* TokToPrimitiveType(TokenType tokType)
{
    switch_nocheck(tokType)
    {
        case Tok_Bool:   return &Typer_Bool;
        case Tok_Char:   return &Typer_Char;
        case Tok_Uint8:  return &Typer_Uint8;
        case Tok_Uint16: return &Typer_Uint16;
        case Tok_Uint32: return &Typer_Uint32;
        case Tok_Uint64: return &Typer_Uint64;
        case Tok_Int8:   return &Typer_Int8;
        case Tok_Int16:  return &Typer_Int16;
        case Tok_Int32:  return &Typer_Int32;
        case Tok_Int64:  return &Typer_Int64;
        case Tok_Float:  return &Typer_Float;
        case Tok_Double: return &Typer_Double;
        case Tok_Raw:    return &Typer_Raw;
    } switch_nocheck_end;
    
    Assert(false && "Unreachable code");
    return 0;
}

// @cleanup the same function is in typer (should just be in typer)
template<typename t>
t* Ast_MakeType(Arena* arena)
{
    // Call the constructor for the type,
    // pack the types for better cache locality
    t* result = Arena_AllocAndInitPack(arena, t);
    return result;
}

struct Ast_Node
{
    // Primary token representing this node
    // (e.g. 'if' token for if statement)
    Token* where;
    Ast_NodeKind kind;
    
    Dg_Idx entityIdx = Dg_Null;
    CompPhase phase = CompPhase_Parse;
};

inline bool Ast_IsExpr(Ast_Node* node)
{
    if(!node)
        return true;
    
    return node->kind >= AstKind_ExprBegin && node->kind <= AstKind_ExprEnd;
}

inline bool Ast_IsStmt(Ast_Node* node)
{
    if(!node)
        return true;
    
    return Ast_IsExpr(node)
        || (node->kind >= AstKind_StmtBegin && node->kind <= AstKind_StmtEnd)
        || (node->kind == AstKind_VarDecl);
}

inline bool Ast_IsDecl(Ast_Node* node)
{
    if(!node)
        return true;
    
    return node->kind >= AstKind_DeclBegin && node->kind <= AstKind_DeclEnd;
}

struct Ast_Expr : public Ast_Node
{
    TypeInfo* type;
    // Type to convert to during codegen.
    // For example, in 3.4 + 4, 4 has convertTo = float
    TypeInfo* castType;
    Token* typeWhere = 0;
};

struct Ast_Stmt : public Ast_Node {};

struct Ast_Block : public Ast_Stmt
{
    Ast_Block() { kind = AstKind_Block; };
    
    DynArray<Ast_Declaration*> decls;
    
    Ast_Block* enclosing = 0;
    Array<Ast_Node*> stmts = { 0, 0 };
};

struct Ast_FileScope
{
    // Stuff about which files are imported here
    
    Ast_Block scope;
};

// Statements

// Some of the control structures have blocks, because
// they might implicitly have declarations that will be associated
// to the internal block for identifier lookup

struct Ast_If : public Ast_Stmt
{
    Ast_If() { kind = AstKind_If; };
    
    Ast_Node* condition;  // Declaration or expression
    Ast_Block* thenBlock;  // This needs to be a block because the condition might possibly have a declaration
    Ast_Stmt* elseStmt = 0;
};

struct Ast_For : public Ast_Stmt
{
    Ast_For() { kind = AstKind_For; };
    
    Ast_Node* initialization = 0;  // Declaration or expression
    Ast_Node* condition = 0;  // Declaration or expression
    Ast_Expr* update = 0;
    Ast_Block* body;
};

struct Ast_While : public Ast_Stmt
{
    Ast_While() { kind = AstKind_While; };
    
    Ast_Node* condition;  // Declaration or expression
    Ast_Block* doBlock;
};

struct Ast_DoWhile : public Ast_Stmt
{
    Ast_DoWhile() { kind = AstKind_DoWhile; };
    
    Ast_Expr* condition;
    Ast_Stmt* doStmt;
};

struct Ast_Switch : public Ast_Stmt
{
    Ast_Switch() { kind = AstKind_Switch; };
    
    Ast_Expr* switchExpr;
    Array<Ast_Expr*> cases = { 0, 0 };
    Array<Ast_Stmt*> stmts = { 0, 0 };
};

struct Ast_Defer : public Ast_Stmt
{
    Ast_Defer() { kind = AstKind_Defer; };
    
    Ast_Stmt* stmt;
};

struct Ast_Return : public Ast_Stmt
{
    Ast_Return() { kind = AstKind_Return; };
    
    //Ast_Expr* expr = 0;
    Array<Ast_Expr*> rets = { 0, 0 };
};

struct Ast_Break : public Ast_Stmt
{
    Ast_Break() { kind = AstKind_Break; };
};

struct Ast_Continue : public Ast_Stmt
{
    Ast_Continue() { kind = AstKind_Continue; };
};

struct Ast_Fallthrough : public Ast_Stmt
{
    Ast_Fallthrough() { kind = AstKind_Fallthrough; };
};

struct Ast_MultiAssign : public Ast_Stmt
{
    Ast_MultiAssign() { kind = AstKind_MultiAssign; };
    
    Array<Ast_Node*> lefts;  // Each can be a VarDecl or an Expr
    Array<Ast_Expr*> rights;
};

// Syntax-Trees related to types

struct Ast_PtrType : public TypeInfo
{
    Ast_PtrType()
    {
        typeId = Typeid_Ptr;
        size = align = 8;
    };
    
    TypeInfo* baseType;
    uint16 typeFlags;
};

struct Ast_ArrType : public TypeInfo
{
    Ast_ArrType() { typeId = Typeid_Arr; };
    
    TypeInfo* baseType;
    // TODO: to be replaced with constValue
    Ast_Expr* sizeExpr;
    
    // TODO: This will also be replaced with a constValue
    // Filled in later in the size stage
    uint64 sizeValue = 0;
};

struct Ast_ProcType : public TypeInfo
{
    Ast_ProcType() { typeId = Typeid_Proc; };
    
    bool isOperator;
    
    Array<Ast_Declaration*> args = { 0, 0 };
    Array<TypeInfo*> retTypes = { 0, 0 };
    
    // Codegen
    TB_FunctionPrototype* tildeProto = 0;
};

struct Ast_IdentType : public TypeInfo
{
    Ast_IdentType() { typeId = Typeid_Ident; };
    
    Atom* ident;
    
    Array<Ast_Expr*> polyParams = { 0, 0 };
    
    // Filled in by the typechecker
    Ast_StructDef* structDef = 0;
};

struct Ast_StructType : public TypeInfo
{
    Ast_StructType() { typeId = Typeid_Struct; };
    
    Array<TypeInfo*> memberTypes = { 0, 0 };
    Array<Atom*>     memberNames = { 0, 0 };
    
    // For errors?
    Array<Token*> memberNameTokens = { 0, 0 };
    
    // Filled in later in the sizing stage 
    Array<uint32> memberOffsets = { 0, 0 };
};

// Declarations

struct Ast_Declaration : public Ast_Node
{
    Ast_DeclSpec declSpecs;
    TypeInfo* type;
    Atom* name;
    
    // Codegen
    TB_Node* tildeNode;
};

struct Ast_VarDecl : public Ast_Declaration
{
    Ast_VarDecl() { kind = AstKind_VarDecl; };
    
    Ast_Expr* initExpr = 0;
    
    // This is the index in the procedure's
    // declaration array (flattened form),
    // This stays -1 if it's a global variable
    int declIdx = -1;
    
    // This is only used for global variables
    Interp_Symbol* symbol;
};

struct Ast_ProcDef : public Ast_Declaration
{
    Ast_ProcDef() { kind = AstKind_ProcDef; };
    
    Ast_Block block;
    
    // Flattened form of all declarations in the
    // procedure. Useful for codegen
    DynArray<Ast_Declaration*> declsFlat;
    
    // Codegen stuff (later some stuff will be removed)
    // This stuff could just be in a PtrMap
    TB_Function* tildeProc = 0;
    Interp_Symbol* symbol = 0;
};

struct Ast_StructDef : public Ast_Declaration
{
    Ast_StructDef() { kind = AstKind_StructDef; };
};

// Expressions
struct Ast_BinaryExpr : public Ast_Expr
{
    Ast_BinaryExpr() { kind = AstKind_BinaryExpr; };
    
    uint16 op;  // I guess this can just be the token type value for now
    Ast_Expr* lhs;
    Ast_Expr* rhs;
    
    // Filled in by the typechecker, if needed and possible
    Ast_FuncCall* overloaded = 0;
};

struct Ast_UnaryExpr : public Ast_Expr
{
    Ast_UnaryExpr() { kind = AstKind_UnaryExpr; };
    
    uint16 op;
    bool isPostfix;
    Ast_Expr* expr;
    
    // Filled in by the typechecker, if needed and possible
    Ast_FuncCall* overloaded = 0;
};

struct Ast_TernaryExpr : public Ast_Expr
{
    Ast_TernaryExpr() { kind = AstKind_TernaryExpr; };
    
    uint8 ternaryOperator;
    Ast_Expr* expr1;
    Ast_Expr* expr2;
    Ast_Expr* expr3;
    
    // Filled in by the overload resolver, if needed and possible
    Ast_FuncCall* overloaded = 0;
};

struct Ast_FuncCall : public Ast_Expr
{
    Ast_FuncCall() { kind = AstKind_FuncCall; };
    
    Ast_Expr* target;
    Array<Ast_Expr*> args = { 0, 0 };
    
    // Filled in by the typechecker (or overload resolver?)
    Ast_ProcDef* called = 0;
};

struct Ast_Subscript : public Ast_Expr
{
    Ast_Subscript() { kind = AstKind_Subscript; };
    
    Ast_Expr* target;
    Ast_Expr* idxExpr;
};

struct Ast_IdentExpr : public Ast_Expr
{
    Ast_IdentExpr() { kind = AstKind_Ident; };
    
    Atom* ident;
    
    // Filled in by the typechecker
    Ast_Declaration* declaration = 0;
};

struct Ast_MemberAccess : public Ast_Expr
{
    Ast_MemberAccess() { kind = AstKind_MemberAccess; };
    
    Ast_Expr* target;
    
    Atom* memberName;
    
    // Filled in by the typechecker
    Ast_StructType* structDecl = 0;
    uint32 memberIdx = 0;
};

struct Ast_Typecast : public Ast_Expr
{
    Ast_Typecast() { kind = AstKind_Typecast; };
    
    Ast_Expr* expr;
};

enum ConstBitfield
{
    Const_IsReady      = 1 << 0,
    Const_RunDirective = 1 << 1,
};

struct Ast_ConstValue : public Ast_Expr
{
    Ast_ConstValue() { kind = AstKind_ConstValue; };
    
    void* addr = 0;  // Size is encoded in the type
};

inline bool Ast_IsExprArithmetic(Ast_BinaryExpr* expr)
{
    if(!expr) return false;
    switch(expr->op)
    {
        default: return false;
        case '+': case '-':
        case '*': case '/':
        case '%':
        return true;
    }
    
    return false;
}

inline bool Ast_IsExprBitManipulation(Ast_BinaryExpr* expr)
{
    if(!expr) return false;
    switch(expr->op)
    {
        default: return false;
        case Tok_LShift: case Tok_RShift:
        case '&': case '|': case '^':
        case Tok_LShiftEquals: case Tok_RShiftEquals:
        case Tok_AndEquals: case Tok_OrEquals: case Tok_XorEquals:
        return true;
    }
    
    return false;
}

template<typename t>
t* Ast_MakeNode(Arena* arena, Token* token);
Ast_Node* Ast_MakeNode(Arena* arena, Token* token, Ast_NodeKind kind);
template<typename t>
t Ast_InitNode(Token* token);

// Utility functions

// A procedure definition is guaranteed (by the parser) to have an Ast_ProcType
// as the declaration type. It's only a declaration because it can be used for generic
// symbol lookup functions.
cforceinline Ast_ProcType* Ast_GetDeclProc(Ast_ProcDef* procDef)
{
    return (Ast_ProcType*)procDef->type;
}

cforceinline Ast_StructType* Ast_GetDeclStruct(Ast_StructDef* structDef)
{
    return (Ast_StructType*)structDef->type;
}

cforceinline Ast_ProcType* Ast_GetDeclCallTarget(Ast_FuncCall* call)
{
    return (Ast_ProcType*)call->target->type;
}

cforceinline Token* Ast_GetVarDeclTypeToken(Ast_VarDecl* decl)
{
    // This might be a horrible hack, but maybe not.
    // TODO: Update: it definitely is.
    return decl->where - 1;
}

cforceinline Token* Ast_GetTypecastTypeToken(Ast_Typecast* typecast)
{
    return typecast->where;
}

// It returns the token itself if it's not an assignment token.
// It returns '=' if it's just that.
cforceinline TokenType Ast_GetAssignUnderlyingOp(TokenType tokType, bool* outIsAssign)
{
    Assert(outIsAssign);
    *outIsAssign = true;
    
    switch_nocheck(tokType)
    {
        case Tok_PlusEquals:   return (TokenType)'+';
        case Tok_MinusEquals:  return (TokenType)'-';
        case Tok_MulEquals:    return (TokenType)'*';
        case Tok_DivEquals:    return (TokenType)'/';
        case Tok_ModEquals:    return (TokenType)'%';
        case Tok_LShiftEquals: return Tok_LShift;
        case Tok_RShiftEquals: return Tok_RShift;
        case Tok_AndEquals:    return (TokenType)'&';
        case Tok_OrEquals:     return (TokenType)'|';
        case Tok_XorEquals:    return (TokenType)'^';
        case '=':              return (TokenType)'=';
    } switch_nocheck_end;
    
    *outIsAssign = false;
    return tokType;
}