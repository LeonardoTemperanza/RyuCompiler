
#pragma once

#include "base.h"
#include "lexer.h"
#include "memory_management.h"

struct Ast_Block;

struct Ast_FunctionDef;
struct Ast_StructDef;

struct Ast_Declaration;
struct Ast_Enumerator;
struct Ast_Declarator;
struct Ast_DeclaratorPtr;
struct Ast_DeclaratorArr;
struct Ast_DeclaratorIdent;
struct Ast_DeclaratorProc;
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
    AstKind_Root,
    
    // Toplevel
    AstKind_ToplevelBegin,
    AstKind_FunctionDef,
    AstKind_StructDef,
    AstKind_ToplevelEnd,
    
    // Declarators
    AstKind_DeclBegin,
    AstKind_Declaration,
    AstKind_Enumerator,
    AstKind_Declarator,
    AstKind_DeclaratorPtr,
    AstKind_DeclaratorArr,
    AstKind_DeclaratorIdent,
    AstKind_DeclaratorProc,
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
    AstKind_Return,
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
// [3]^[4]int var;  array of pointers to static arrays
// vec3(float) v;   polymorphic compound type
// There are also going to be const qualifiers and all that 
// NOTE(Leo): Bigger value means more accurate (/bigger) type
enum TypeId
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
    Typeid_Double   = Tok_Double - Tok_IdentBegin,
    Typeid_BasicTypesEnd = Typeid_Double,
    
    Typeid_Ptr,
    Typeid_Arr,
    Typeid_Proc,
    Typeid_Struct,
    Typeid_Ident
};

struct TypeInfo
{
    TypeId typeId;
    Token* where;
};

// @temporary
// This is the exact same order as the typeid enum
TypeInfo primitiveTypes[] =
{
    { Typeid_Bool },  { Typeid_Char },
    { Typeid_Uint8 }, { Typeid_Uint16 }, { Typeid_Uint32 }, { Typeid_Uint64 },
    { Typeid_Int8 },  { Typeid_Int16 },  { Typeid_Int32 },  { Typeid_Int64 },
    { Typeid_Float }, { Typeid_Double }
};

TypeInfo* TokToPrimitiveTypeid(TokenType tokType)
{
    return primitiveTypes + (tokType - Tok_IdentBegin);
}

TokenType PrimitiveTypeidToTokType(TypeId typeId)
{
    return (TokenType)(typeId + Tok_IdentBegin);
}

// Some of the token data is used frequently (like the token string)
// so it's beneficial to store it directly here
struct Ast_Node
{
    Ast_NodeKind kind;
    Token* where;  // Primary token representing this node
    union // Token hot data, the name or value
    {
        TokenHotData tokenHotData;
        union
        {
            String ident;
            uint64 uintValue;
            int64 intValue;
            float floatValue;
            double doubleValue;
        };
    };
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
        || (node->kind == AstKind_Declaration);
}

inline bool Ast_IsDecl(Ast_Node* node)
{
    if(!node)
        return true;
    
    return node->kind >= AstKind_DeclBegin && node->kind <= AstKind_DeclEnd;
}

inline bool Ast_IsTopLevel(Ast_Node* node)
{
    if(!node)
        return true;
    
    return (node->kind >= AstKind_ToplevelBegin && node->kind <= AstKind_ToplevelEnd) ||
        node->kind == AstKind_FunctionDef || Ast_IsDecl(node);
}

// These are like typesafe typedefs (for pointers)
// @temporary
struct Ast_Expr : public Ast_Node
{
    TypeInfo* type;
    TypeInfo* castType;  // Do I actually need this?
};
struct Ast_Stmt : public Ast_Node {};
struct Ast_TopLevel : public Ast_Node {};

struct Ast_Block : public Ast_Stmt
{
    Ast_Block() { kind = AstKind_Block; };
    
    DynArray<Ast_Declaration*> decls;
    
    Ast_Block* enclosing = 0;
    Ast_For* inForStmt = 0;
    Ast_While* inWhileStmt = 0;
    Ast_DoWhile* inDoWhileStmt = 0;
    Ast_Switch* inSwitchStmt = 0;
    Ast_FunctionDef* inFunction = 0;
    
    Array<Ast_Node*> stmts = { 0, 0 };
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

struct Ast_Declaration : public Ast_TopLevel
{
    Ast_Declaration() { kind = AstKind_Declaration; };
    
    TypeInfo* type;
    String name;
    
    Ast_Expr* initExpr = 0;
};

// Declarators

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
    
    String name = { 0, 0 };
    Array<TypeInfo*> argTypes = { 0, 0 };
    Array<Token*> argNames = { 0, 0 };
    Array<TypeInfo*> retTypes = { 0, 0 };
};

struct Ast_DeclaratorIdent : public TypeInfo
{
    Ast_DeclaratorIdent() { typeId = Typeid_Ident; };
    
    String ident;
    Array<Ast_Expr*> polyParams = { 0, 0 };
    
    // Typeid here? Filled in later by the checker
};

struct Ast_DeclaratorStruct : public TypeInfo
{
    Ast_DeclaratorStruct() { typeId = Typeid_Struct; };
    
    Array<TypeInfo*> memberTypes = { 0, 0 };
    Array<Token*> memberNames = { 0, 0 };
};

struct Ast_FunctionDef : public Ast_Node
{
    Ast_FunctionDef() { kind = AstKind_FunctionDef; };
    
    Ast_Declaration decl;
    Ast_Block block;
};

struct Ast_StructDef : public Ast_TopLevel
{
    Ast_StructDef() { kind = AstKind_StructDef; };
    
    Ast_DeclaratorStruct decl;
    String name = { 0, 0 };
};

// Expressions
struct Ast_BinaryExpr : public Ast_Expr
{
    Ast_BinaryExpr() { kind = AstKind_BinaryExpr; };
    
    uint16 op;  // I guess this can just be the token type value for now
    Ast_Expr* lhs;
    Ast_Expr* rhs;
    
    // Filled in by the overload resolver (if needed/possible)
    Ast_FuncCall* overloaded = 0;
};

struct Ast_UnaryExpr : public Ast_Expr
{
    Ast_UnaryExpr() { kind = AstKind_UnaryExpr; };
    
    uint16 op;
    Ast_Expr* expr;
    
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
    
    // Filled in by the overload resolver (if needed/possible)
    Ast_FuncCall* overloaded = 0;
};

struct Ast_FuncCall : public Ast_Expr
{
    Ast_FuncCall() { kind = AstKind_UnaryExpr; };
    
    // You can call a function pointer like:
    // funcPtrs[2](3);
    Ast_Expr* func;
    Array<Ast_Expr*> args = { 0, 0 };
    
    // Filled in by the typechecker (or overload resolver?)
    Ast_FunctionDef* called = 0;
};

struct Ast_Subscript : public Ast_Expr
{
    Ast_Subscript() { kind = AstKind_Subscript; };
    
    Ast_Expr* array;
    Ast_Expr* idxExpr;
};

struct Ast_IdentExpr : public Ast_Expr
{
    Ast_IdentExpr() { kind = AstKind_Ident; };
    
    String ident;
    
    // Filled in by the typechecker
    Ast_Declaration* declaration = 0;
};

struct Ast_MemberAccess : public Ast_Expr
{
    Ast_MemberAccess() { kind = AstKind_MemberAccess; };
    
    Ast_Expr* toAccess;
    String memberName;
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

struct OverloadSet
{
    String funcName;
    DynArray<Ast_DeclaratorProc*> functionTypes;
};

struct Parser
{
    Arena* arena; // Ast storage
    Tokenizer* tokenizer;
    Token* at;  // Current analyzed token in the stream
    
    Ast_Block* scope = 0;  // Current scope
    Ast_Block* globalScope = 0;
};

template<typename t>
t* Ast_MakeNode(Arena* arena, Token* token);
Ast_Node* Ast_MakeNode(Arena* arena, Token* token, Ast_NodeKind kind);
template<typename t>
t Ast_InitNode(Token* token);

Ast_Block* ParseFile(Parser* p);
Ast_FunctionDef* ParseFunctionDef(Parser* p);
Ast_StructDef* ParseStructDef(Parser* p);
Ast_Node* ParseDeclOrExpr(Parser* p, bool forceInit = false);
Ast_Declaration ParseDeclaration(Parser* p, bool forceInit = false, bool preventInit = false);
Ast_Stmt* ParseStatement(Parser* p);
Ast_If* ParseIf(Parser* p);
Ast_For* ParseFor(Parser* p);
Ast_While* ParseWhile(Parser* p);
Ast_DoWhile* ParseDoWhile(Parser* p);
Ast_Defer* ParseDefer(Parser* p);
Ast_Return* ParseReturn(Parser* p);
Ast_Block* ParseBlock(Parser* p);
Ast_Block* ParseOneOrMoreStmtBlock(Parser* p);

Ast_Expr* ParseExpression(Parser* p, int prec = INT_MIN);
Ast_Expr* ParsePostfixExpression(Parser* p);
Ast_Expr* ParsePrimaryExpression(Parser* p);
TypeInfo* ParseType(Parser* p, Token** outIdent);
Ast_DeclaratorProc ParseDeclProc(Parser* p, Token** outIdent, bool forceArgNames = true);
Ast_DeclaratorStruct ParseDeclStruct(Parser* p, Token** outIdent);

// TODO: @temporary Put other declaration keywords here
inline bool IsTokStartOfDecl(TokenType tokType)
{
    return tokType == '^' || tokType == '[' || tokType == Tok_Struct || tokType == Tok_Proc;
};

int GetOperatorPrec(TokenType tokType);
bool IsOpPrefix(TokenType tokType);
bool IsOpPostfix(TokenType tokType);
bool IsOperatorLToR(TokenType tokType);

// Also skips nested parentheses
Token* SkipParens(Token* start, char openParen, char closeParen);

inline void ParseError(Parser* p, Token* token, String message);
inline void ParseError(Parser* p, Token* token, int numStrings, char* message1, ...);
inline void ParseError(Parser* p, Token* token, char* message);

inline Token* EatRequiredToken(Parser* p, TokenType tokType);
inline Token* EatRequiredToken(Parser* p, char tokType);

inline void ExpectedTokenError(Parser* p, TokenType tokType);
inline void ExpectedTokenError(Parser* p, char tokType);
inline void ExpectedTokenError(Parser* p, Token* tok, TokenType tokType);
inline void ExpectedTokenError(Parser* p, Token* tok, char tokType);
