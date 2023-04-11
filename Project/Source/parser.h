
#pragma once

#include "base.h"
#include "lexer.h"
#include "memory_management.h"

struct Ast_FunctionDef;

struct Ast_Declaration;
struct Ast_Enumerator;
struct Ast_Declaration;
struct Ast_Declarator;
struct Ast_DeclaratorPtr;
struct Ast_DeclaratorArr;
struct Ast_DeclaratorIdent;
struct Ast_DeclaratorFunc;
struct Ast_DeclaratorStruct;

struct Ast_If;
struct Ast_For;
struct Ast_While;
struct Ast_DoWhile;
struct Ast_Switch;

struct Ast_Literal;
struct Ast_Ident;
struct Ast_BinaryExpr;
struct Ast_TernaryExpr;
struct Ast_FuncCall;

struct TypeInfo;

// Ast node types
enum Ast_NodeKind : uint8
{
    AstKind_None,
    
    // Toplevel
    AstKind_ToplevelBegin,
    AstKind_FunctionDef,
    AstKind_ToplevelEnd,
    
    // Declarations
    AstKind_DeclBegin,
    AstKind_Declaration,
    AstKind_Enumerator,
    AstKind_Declarator,
    AstKind_DeclaratorPtr,
    AstKind_DeclaratorArr,
    AstKind_DeclaratorIdent,
    AstKind_DeclaratorFunc,
    AstKind_DeclaratorStruct,
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
    AstKind_StmtEnd,
    
    // Expressions
    AstKind_ExprBegin,
    AstKind_Literal,
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
// [3]^[4]int var;  array of pointers to static arrays
// vec3(float) v;   polymorphic compound type
// There are also going to be const qualifiers and all that 
enum TypeId
{
    Typeid_BasicTypesBegin = Tok_IdentBegin,
    Typeid_Int8     = Tok_Int8,
    Typeid_Int16    = Tok_Int16,
    Typeid_Int32    = Tok_Int32,
    Typeid_Int64    = Tok_Int64,
    Typeid_Uint8    = Tok_Uint8,
    Typeid_Uint16   = Tok_Uint16,
    Typeid_Uint32   = Tok_Uint32,
    Typeid_Uint64   = Tok_Uint64,
    Typeid_Float    = Tok_Float,
    Typeid_Double   = Tok_Double,
    Typeid_Function = Tok_IdentEnd + 1,
    Typeid_BasicTypesEnd = Typeid_Function,
    
    Typeid_Struct = Tok_Ident,
    Typeid_Ptr    = '^',
    Typeid_Array  = Tok_Subscript
};

struct Ast_Node
{
    Ast_NodeKind kind;
    Token* where;  // Primary token representing this node
};

inline bool Ast_IsExpr(Ast_Node* node)
{
    return node->kind >= AstKind_ExprBegin && node->kind <= AstKind_ExprEnd;
}

inline bool Ast_IsStmt(Ast_Node* node)
{
    return Ast_IsExpr(node)
        || (node->kind >= AstKind_StmtBegin && node->kind <= AstKind_StmtEnd)
        || (node->kind == AstKind_Declaration);
}

inline bool Ast_IsDecl(Ast_Node* node)
{
    return node->kind >= AstKind_DeclBegin && node->kind <= AstKind_DeclEnd;
}

inline bool Ast_IsTopLevel(Ast_Node* node)
{
    return node->kind == AstKind_FunctionDef || Ast_IsDecl(node);
}

// These are like typesafe typedefs (for pointers)
struct Ast_Expr : public Ast_Node {};
struct Ast_Stmt : public Ast_Node {};
struct Ast_Decl : public Ast_Node {};
struct TypeInfo : public Ast_Node {};
struct Ast_TopLevel : public Ast_Node {};

// Statements
struct Ast_If : public Ast_Stmt
{
    Ast_If() { kind = AstKind_If; };
    
    Ast_Node* condition;  // Declaration or expression
    Ast_Stmt* thenStmt;
    Ast_Stmt* elseStmt;
};

struct Ast_For : public Ast_Stmt
{
    Ast_For() { kind = AstKind_For; };
    
    Ast_Node* initialization;  // Declaration or expression
    Ast_Node* condition;  // Declaration or expression
    Ast_Expr* update;
    Ast_Stmt* body;
};

struct Ast_While : public Ast_Stmt
{
    Ast_While() { kind = AstKind_While; };
    
    Ast_Expr* condition;
    Ast_Stmt* doStmt;
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
    Array<Ast_Expr*> cases;
};

struct Ast_Defer : public Ast_Stmt
{
    Ast_Defer() { kind = AstKind_Defer; };
    
    Ast_Stmt* stmt;
};

struct Ast_Block : public Ast_Stmt
{
    Ast_Block() { kind = AstKind_Block; };
    
    Array<Ast_Stmt*> stmts;
    
    // Scope info???
};

struct Ast_Declaration : public Ast_Node
{
    Ast_Declaration() { kind = AstKind_Declaration; };
    
    TypeInfo* typeInfo; // TODO: should this be here then?
    Array<Ast_Declarator*> declarators;
    String name;
};

// Declarators

struct Ast_DeclaratorPtr : public TypeInfo
{
    Ast_DeclaratorPtr() { kind = AstKind_DeclaratorPtr; };
    
    uint16 typeFlags;
};

struct Ast_DeclaratorArr : public TypeInfo
{
    Ast_DeclaratorArr() { kind = AstKind_DeclaratorArr; };
    
    Ast_Expr* sizeExpr;
    
    // Possibly filled in later after the run stage?
    // Need this for the sizing stage
    uint64 sizeValue = 0;
};

struct Ast_DeclaratorFunc : public TypeInfo
{
    Ast_DeclaratorFunc() { kind = AstKind_DeclaratorFunc; };
    
    Array<TypeInfo*> argTypes;
    Array<TypeInfo*> retTypes;
};

struct Ast_DeclaratorIdent : public TypeInfo
{
    Ast_DeclaratorIdent() { kind = AstKind_DeclaratorIdent; };
    
    String ident;
    Array<Ast_Expr*> polyParams;
};

struct Ast_DeclaratorStruct : public TypeInfo
{
    Ast_DeclaratorStruct() { kind = AstKind_DeclaratorStruct; };
};

struct Ast_FunctionDef : public Ast_Declaration
{
    Ast_FunctionDef() { kind = AstKind_FunctionDef; };
    
    Ast_DeclaratorFunc decl;
    Ast_Block block;
};

// Expressions
struct Ast_BinaryExpr : public Ast_Expr
{
    Ast_BinaryExpr() { kind = AstKind_BinaryExpr; };
    
    uint8 binaryOperator;
    Ast_Expr* lhs;
    Ast_Expr* rhs;
    
    // Filled in by the typechecker
    TypeInfo* typeInfo;
    
    // Filled in by the overload resolver (if needed/possible)
    Ast_FuncCall* overloaded = 0;
};

struct Ast_UnaryExpr : public Ast_Expr
{
    Ast_UnaryExpr() { kind = AstKind_UnaryExpr; };
    
    uint8 unaryOperator;
    Ast_Expr* expr;
    
    // Filled in by the typechecker
    TypeInfo* typeInfo;
    
    // Filled in by the overload resolver (if needed/possible)
    Ast_FuncCall* overloaded = 0;
};

struct Ast_TernaryExpr : public Ast_Expr
{
    Ast_TernaryExpr() { kind = AstKind_TernaryExpr; };
    
    uint8 ternaryOperator;
    Ast_Expr* expr1;
    Ast_Expr* expr2;
    Ast_Expr* expr3;
    
    // Filled in by the typechecker
    TypeInfo* typeInfo;
    
    // Filled in by the overload resolver (if needed/possible)
    Ast_FuncCall* overloaded = 0;
};

// This could also be a function pointer though...
struct Ast_FuncCall : public Ast_Expr
{
    Ast_FuncCall() { kind = AstKind_UnaryExpr; };
    
    // You can call a function pointer like:
    // funcPtrs[2](3);
    Ast_Expr* func;
    Array<Ast_Expr*> args;
    
    // Filled in by the typechecker (or overload resolver?)
    Ast_FunctionDef* called = 0;
    TypeInfo* typeInfo = 0;
};

struct Ast_Subscript : public Ast_Expr
{
    Ast_Subscript() { kind = AstKind_Subscript; };
    
    Ast_Expr* array;
    Ast_Expr* idxExpr;
    
    // Filled in by the typechecker
    TypeInfo* typeInfo = 0;
};

struct Ast_IdentExpr : public Ast_Expr
{
    Ast_IdentExpr() { kind = AstKind_Ident; };
    
    String ident;
    
    // Filled in by the typechecker
    Ast_Declaration* declaration = 0;
};

struct Ast_Typecast : public Ast_Expr
{
    Ast_Typecast() { kind = AstKind_Typecast; };
    
    TypeInfo* typeInfo;
    Ast_Expr* expr;
};

// Runtime typechecking for dynamic casts (only if in debug build)
#ifdef Debug
inline Ast_Expr* CastToExpr(Ast_Node* node) { Assert(Ast_IsExpr(node)); return (Ast_Expr*)node; }
inline Ast_Stmt* CastToStmt(Ast_Node* node) { Assert(Ast_IsStmt(node)); return (Ast_Stmt*)node; }
inline Ast_Decl* CastToDecl(Ast_Node* node) { Assert(Ast_IsDecl(node)); return (Ast_Decl*)node; }
inline Ast_TopLevel* CastToTopLevel(Ast_Node* node) { Assert(Ast_IsTopLevel(node)); return (Ast_TopLevel*)node; }
#else
#define CastToExpr(ptr) (Ast_Expr*)(ptr)
#define CastToStmt(ptr) (Ast_Expr*)(ptr)
#define CastToDecl(ptr) (Ast_Decl*)(ptr)
#define CastToTopLevel(ptr) (Ast_TopLevel*)(ptr);
#endif

#define GenericDynCast(ptr, asttype, astkind) (Assert((ptr)->kind == (astkind)), (asttype*)(ptr));
#define CastToIf(ptr) GenericDynCast(ptr, Ast_If, AstKind_If)
#define CastToFor(ptr) GenericDynCast(ptr, Ast_For, AstKind_For)
#define CastToWhile(ptr) GenericDynCast(ptr, Ast_While, AstKind_While);
#define CastToDoWhile(ptr) GenericDynCast(ptr, Ast_DoWhile, AstKind_DoWhile);
// Do this for all node types i guess
#undef GenericDynCast

struct Parser
{
    Arena* tempArena;
    
    // Ast storage
    Arena* arena;
    
    Tokenizer* tokenizer;
    Token* at;
    
    Array<Ast_Node*> nodes;
};

template<typename t>
t* Ast_MakeNode(Arena* arena, Token* token);

Array<Ast_TopLevel*> ParseFile(Parser* p);
Ast_FunctionDef* ParseFunctionDef(Parser* p);
Ast_Node* ParseDeclOrExpr(Parser* p);
Ast_Declaration* ParseDeclaration(Parser* p);
Ast_Stmt* ParseStatement(Parser* p);
Ast_If* ParseIf(Parser* p);
Ast_For* ParseFor(Parser* p);
Ast_While* ParseWhile(Parser* p);
Ast_Defer* ParseDefer(Parser* p);
Ast_Block* ParseBlock(Parser* p);

Ast_Expr* ParseExpression(Parser* p, int prec = INT_MIN);
Ast_Expr* ParsePostfixExpression(Parser* p);
Ast_Expr* ParsePrimaryExpression(Parser* p);
TypeInfo* ParseType(Parser* p);

int GetOperatorPrec(TokenType tokType);
// TODO: These two should not exist, unary operators do not have precedence in general.
bool IsOpPrefix(TokenType tokType);
bool IsOpPostfix(TokenType tokType);
bool IsOperatorLToR(TokenType tokType);

inline Token* EatRequiredToken(Parser* p, TokenType tokType);
inline Token* EatRequiredToken(Parser* p, char tokType);

inline void ExpectedTokenError(Parser* p, TokenType tokType);
inline void ExpectedTokenError(Parser* p, char tokType);
