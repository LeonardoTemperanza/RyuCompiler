
#pragma once

#include "base.h"
//#include "lexer.h"
#include "dependency_graph.h"
//#include "memory_management.h"
#include "atom.h"

// This stuff is @temporary
struct TB_Node;
struct TB_Function;
struct TB_FunctionPrototype;
struct Interp_Symbol;

// @cleanup Maybe the names for these should be changed...
// Perhaps Ast_PtrType and Ast_ArrayType?

struct Ast_FileScope;

struct Ast_Expr;
struct Ast_Stmt;
struct Ast_Block;

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

// Declarations
struct Ast_Declaration;
struct Ast_VarDecl;
struct Ast_ProcDef;
struct Ast_StructDef;

// Syntax-Trees related to types
struct Ast_DeclaratorPtr;
struct Ast_DeclaratorArr;
struct Ast_DeclaratorProc;
struct Ast_DeclaratorIdent;
struct Ast_DeclaratorStruct;

// Expressions
struct Ast_BinaryExpr;
struct Ast_UnaryExpr;
struct Ast_TernaryExpr;
struct Ast_FuncCall;
struct Ast_Subscript;
struct Ast_IdentExpr;
struct Ast_MemberAccess;
struct Ast_Typecast;
struct Ast_NumLiteral;

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
    AstKind_EmptyStmt,
    AstKind_StmtEnd,
    
    // Expressions
    AstKind_ExprBegin,
    AstKind_Literal,
    AstKind_NumLiteral,
    AstKind_Ident,
    AstKind_FuncCall,
    AstKind_BinaryExpr,
    AstKind_UnaryExpr,
    AstKind_TernaryExpr,
    AstKind_Typecast,
    AstKind_Subscript,
    AstKind_MemberAccess,
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
    Typeid_Bool     = Tok_Bool   - Tok_IdentBegin,
    Typeid_Char     = Tok_Char   - Tok_IdentBegin,  // Scalar values
    Typeid_Uint8    = Tok_Uint8  - Tok_IdentBegin,
    Typeid_Uint16   = Tok_Uint16 - Tok_IdentBegin,
    Typeid_Uint32   = Tok_Uint32 - Tok_IdentBegin,
    Typeid_Uint64   = Tok_Uint64 - Tok_IdentBegin,
    Typeid_Int8     = Tok_Int8   - Tok_IdentBegin,
    Typeid_Int16    = Tok_Int16  - Tok_IdentBegin,
    Typeid_Int32    = Tok_Int32  - Tok_IdentBegin,
    Typeid_Int64    = Tok_Int64  - Tok_IdentBegin,
    Typeid_Float    = Tok_Float  - Tok_IdentBegin,
    Typeid_Double   = Tok_Double - Tok_IdentBegin,  // End of scalar values
    Typeid_BasicTypesEnd = Typeid_Double,
    
    Typeid_Ptr,
    Typeid_Arr,
    Typeid_Proc,
    Typeid_Struct,
    Typeid_Ident,
    Typeid_None,  // Analogous to void
};

struct TypeInfo
{
    TypeId typeId;
    //Token* where;
};

// @temporary
// NOTE: This has to be in the exact same order as the typeid enum
TypeInfo primitiveTypes[] =
{
    { Typeid_Bool  }, { Typeid_Char   },
    { Typeid_Uint8 }, { Typeid_Uint16 }, { Typeid_Uint32 }, { Typeid_Uint64 },
    { Typeid_Int8  }, { Typeid_Int16  }, { Typeid_Int32  }, { Typeid_Int64  },
    { Typeid_Float }, { Typeid_Double }
};

// Size in bytes of all primitive types
// NOTE: This has to be in the exact same order as the typeid enum
const uint32 typeSize[] =
{
    // Bool,  Char,
    1,        1,
    // Uint8, Uint16, Uint32, Uint64,
    1,        2,      4,      8,
    // Int8,  Int16,  Int32,  Int64,
    1,        2,      4,      8,
    // Float, Double
    4,        8
};

TypeInfo noneType = { Typeid_None };

// @cleanup the same function is in typer (should just be in typer)
template<typename t>
t* Ast_MakeType(Arena* arena)
{
    // Call the constructor for the type,
    // pack the types for better cache locality
    t* result = Arena_AllocAndInitPack(arena, t);
    return result;
}

TypeInfo* TokToPrimitiveTypeid(TokenType tokType)
{
    return primitiveTypes + (tokType - Tok_IdentBegin);
}

TokenType PrimitiveTypeidToTokType(TypeId typeId)
{
    return (TokenType)(typeId + Tok_IdentBegin);
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

// These are like typesafe typedefs (for pointers)
struct Ast_Expr : public Ast_Node
{
    TypeInfo* type;
    TypeInfo* castType;  // Do I actually need this?
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
};

struct Ast_Defer : public Ast_Stmt
{
    Ast_Defer() { kind = AstKind_Defer; };
    
    Ast_Stmt* stmt;
};

struct Ast_Return : public Ast_Stmt
{
    Ast_Return() { kind = AstKind_Return; };
    
    Ast_Expr* expr = 0;
};

struct Ast_Break : public Ast_Stmt
{
    Ast_Break() { kind = AstKind_Break; };
};

struct Ast_Continue : public Ast_Stmt
{
    Ast_Continue() { kind = AstKind_Continue; };
};

// Syntax-Trees related to types

struct Ast_DeclaratorPtr : public TypeInfo
{
    Ast_DeclaratorPtr() { typeId = Typeid_Ptr; };
    
    TypeInfo* baseType;
    uint16 typeFlags;
};

struct Ast_DeclaratorArr : public TypeInfo
{
    Ast_DeclaratorArr() { typeId = Typeid_Arr; };
    
    TypeInfo* baseType;
    Ast_Expr* sizeExpr;
    
    // Possibly filled in later after the run stage?
    // Need this for the sizing stage
    uint64 sizeValue = 0;
};

struct Ast_DeclaratorProc : public TypeInfo
{
    Ast_DeclaratorProc() { typeId = Typeid_Proc; };
    
    bool isOperator;
    
    Array<Ast_Declaration*> args = { 0, 0 };
    Array<TypeInfo*> retTypes = { 0, 0 };
    
    // Codegen
    TB_FunctionPrototype* tildeProto = 0;
};

struct Ast_DeclaratorIdent : public TypeInfo
{
    Ast_DeclaratorIdent() { typeId = Typeid_Ident; };
    
    //String ident;
    
    // Later this will be used
    Atom* ident;
    
    Array<Ast_Expr*> polyParams = { 0, 0 };
    
    // Filled in by the typechecker
    Ast_StructDef* structDef = 0;
};

struct Ast_DeclaratorStruct : public TypeInfo
{
    Ast_DeclaratorStruct() { typeId = Typeid_Struct; };
    
    Array<TypeInfo*> memberTypes = { 0, 0 };
    Array<Atom*>     memberNames = { 0, 0 };
    
    // For errors?
    Array<Token*> memberNameTokens = { 0, 0 };
    //Array<Ast_Declaration*> decls;
    
    //uint32 size = -1;
};

// Declarations

struct Ast_Declaration : public Ast_Node
{
    TypeInfo* type;
    //String name;
    
    // In the future this will be used
    Atom* name;
    
    
    Ast_Expr* initExpr = 0;
    
    // Codegen
    TB_Node* tildeNode;
    uint16 interpReg = 0;
};

struct Ast_VarDecl : public Ast_Declaration
{
    Ast_VarDecl() { kind = AstKind_VarDecl; };
    
    Ast_Expr* initExpr = 0;
};

struct Ast_ProcDef : public Ast_Declaration
{
    Ast_ProcDef() { kind = AstKind_ProcDef; };
    
    Ast_Block block;
    
    // Codegen stuff (later some stuff will be removed)
    TB_Function* tildeProc = 0;
    Interp_Symbol* symbol = 0;
};

struct Ast_StructDef : public Ast_Declaration
{
    Ast_StructDef() { kind = AstKind_StructDef; };
    
    // Filled in later in the sizing stage 
    uint32 size = -1;
    uint32 align = -1;
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
    
    // Filled in by the overload resolver (if needed/possible)
    Ast_FuncCall* overloaded = 0;
};

struct Ast_FuncCall : public Ast_Expr
{
    Ast_FuncCall() { kind = AstKind_FuncCall; };
    
    // You can call a function pointer like:
    // funcPtrs[2](3);
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
    
    //String ident;
    
    // Later will be
    Atom* ident;
    
    // Filled in by the typechecker
    Ast_Declaration* declaration = 0;
};

struct Ast_MemberAccess : public Ast_Expr
{
    Ast_MemberAccess() { kind = AstKind_MemberAccess; };
    
    Ast_Expr* target;
    //String memberName;
    
    // Later will be
    Atom* memberName;
    
    // Filled in by the typechecker
    Ast_StructDef* structDef = 0;
    uint32 memberIdx = 0;
};

struct Ast_Typecast : public Ast_Expr
{
    Ast_Typecast() { kind = AstKind_Typecast; };
    
    Ast_Expr* expr;
};

struct Ast_NumLiteral : public Ast_Expr
{
    Ast_NumLiteral() { kind = AstKind_NumLiteral; };
};

inline bool Ast_IsExprArithmetic(Ast_BinaryExpr* expr)
{
    if(!expr)
        return false;
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

template<typename t>
t* Ast_MakeNode(Arena* arena, Token* token);
Ast_Node* Ast_MakeNode(Arena* arena, Token* token, Ast_NodeKind kind);
template<typename t>
t Ast_InitNode(Token* token);

// Utility functions

// A procedure definition is guaranteed (by the parser) to have an Ast_DeclaratorProc
// as the declaration type. It's only a declaration because it can be used for generic
// symbol lookup functions.
cforceinline Ast_DeclaratorProc* Ast_GetDeclProc(Ast_ProcDef* procDef)
{
    return (Ast_DeclaratorProc*)procDef->type;
}

cforceinline Ast_DeclaratorStruct* Ast_GetDeclStruct(Ast_StructDef* structDef)
{
    return (Ast_DeclaratorStruct*)structDef->type;
}

cforceinline Ast_DeclaratorProc* Ast_GetDeclCallTarget(Ast_FuncCall* call)
{
    return (Ast_DeclaratorProc*)call->target->type;
}

cforceinline Token* Ast_GetVarDeclTypeToken(Ast_VarDecl* decl)
{
    // This might be a horrible hack, but maybe not.
    return decl->where - 1;
}

cforceinline Token* Ast_GetTypecastTypeToken(Ast_Typecast* typecast)
{
    return typecast->where;
}
