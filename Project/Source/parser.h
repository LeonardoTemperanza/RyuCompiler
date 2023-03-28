
#pragma once

#include "base.h"
#include "lexer.h"
#include "memory_management.h"

enum Ast_TopLevelStmtType 
{
    Function, 
    Prototype
};

struct Ast_TopLevelStmt
{
    Ast_TopLevelStmtType type;
};


struct Ast_Root
{
    Array<Ast_TopLevelStmt*> topStmts = { 0, 0 };
};

struct Ast_ExprNode;

struct Ast_Type
{
    Token ident;
    
    // Useful for accurate error messages
    Array<Token> qualifiers = { 0, 0 };
    // Useful for quickly determining if a declaration has
    // a certain qualifier or not.
    // TODO: Currently, there is no way to declare an array.
    // Arrays should work like this: [3][4]^int matrixPtr3x4;
    // But they could also be like this: [3]^[4]int arrayOfPtrsToArrays;
    // In general an array of tokens is probably needed, not only that but
    // also the order in which the tokens appear is important, and also
    // the array size could be an expression i suppose.
    bool isConst = 0;
    uint8 numPointers = 0;
    
    // Used for parametric polymorphism
    Array<Ast_ExprNode*> params = { 0, 0 };
};

enum Ast_ExprType
{
    IdentNode,
    NumNode,
    BinNode,
    UnaryNode,
    CallNode,
    ArrayInitNode,
    SubscriptNode
};

struct Ast_ExprNode
{
    Ast_ExprType type;
    Token token;
    
    // Any expression could have a cast
    // Associated to them
    bool hasCast = false;
    Ast_Type castTo;
    
    // Only used during IR generation.
    bool isLeftValue = false;
};

struct Ast_BinExprNode : public Ast_ExprNode
{
    Ast_BinExprNode() { type = BinNode; }
    
    Ast_ExprNode* left;
    Ast_ExprNode* right;
};

struct Ast_UnaryExprNode : public Ast_ExprNode
{
    Ast_UnaryExprNode() { type = UnaryNode; }
    
    Ast_ExprNode* expr;
};

struct Ast_CallExprNode : public Ast_ExprNode
{
    Ast_CallExprNode() { type = CallNode; }
    
    Array<Ast_ExprNode*> args = { 0, 0 };
};

struct Ast_ArrayInitNode : public Ast_ExprNode
{
    Ast_ArrayInitNode() { type = ArrayInitNode; }
    
    Array<Ast_ExprNode*> expressions = { 0, 0 };
};

// This needs to be an operator
struct Ast_SubscriptExprNode : public Ast_ExprNode
{
    Ast_SubscriptExprNode() { type = SubscriptNode; }
    
    Token ident;
    Ast_ExprNode* indexExpr;
};

enum Ast_StmtType
{
    ErrorStmt,
    EmptyStmt,
    IfStmt,
    ForStmt,
    WhileStmt,
    BlockStmt,
    DeclStmt,
    ExprStmt,
    ReturnStmt,
    BreakStmt,
    ContinueStmt,
};

struct Ast_Stmt
{
    Ast_StmtType type;
};

// @outdated
struct Ast_DeclStmt : public Ast_Stmt
{
    Ast_DeclStmt() { type = DeclStmt; };
    
    // true when used for arrays
    bool isPointer = false;
    
    Token id;
    Token equal;
    Ast_ExprNode* expr = 0;
};

struct Ast_Declaration : public Ast_Stmt
{
    Ast_Declaration() { type = DeclStmt; };
    
    Ast_Type typeInfo;
    Token nameIdent;
    
    Ast_BinExprNode* initialization = 0;
};

struct Ast_ReturnStmt : public Ast_Stmt
{
    Ast_ReturnStmt() { type = ReturnStmt; };
    
    Ast_ExprNode* returnValue = 0;
};

// NOTE(Leo): Used for if, for and while statements
// (declarations + initializations are also
// allowed in conditions, for example). It
// can be used in both cases (initialization
// is mandatory or not), in the latter case
// declCond should be null and isDecl = true

struct Ast_ExprOrDecl
{
    bool isDecl;
    union
    {
        Ast_DeclStmt* decl;
        Ast_ExprNode* expr;
    };
};

struct Ast_If : public Ast_Stmt
{
    Ast_If() { type = IfStmt; };
    
    Ast_ExprOrDecl* cond = 0;
    Ast_Stmt* trueStmt   = 0;
    Ast_Stmt* falseStmt  = 0;
};

struct Ast_For : public Ast_Stmt
{
    Ast_For() { type = ForStmt; };
    
    Ast_ExprOrDecl* init    = 0;
    Ast_ExprOrDecl* cond    = 0;
    Ast_ExprNode* iteration = 0;
    
    Ast_Stmt* execStmt;
};

struct Ast_While : public Ast_Stmt
{
    Ast_While() { type = WhileStmt; };
    
    Ast_ExprOrDecl* cond;
    Ast_Stmt* execStmt;
};

struct Ast_BlockNode
{
    Ast_Stmt* stmt;
    Ast_BlockNode* next = 0;
};

struct Ast_BlockStmt : public Ast_Stmt
{
    Ast_BlockStmt() { type = BlockStmt; };
    
    Ast_BlockStmt* enclosingBlock;
    Ast_BlockNode* first = 0;
};

struct Ast_ExprStmt : public Ast_Stmt
{
    Ast_ExprStmt() { type = ExprStmt; };
    
    Ast_ExprNode* expr;
};

struct Ast_ProtoArg
{
    Token id;
    bool isPointer;
};

struct Ast_Prototype : public Ast_TopLevelStmt
{
    Ast_Prototype() { type = Prototype; }
    Token token;
    
    Array<Ast_Type> argTypes = { 0, 0 };
    Array<Token> argNames = { 0, 0 };
    
    Array<Ast_Type> retTypes = { 0, 0 };
    // NOTE: For now return names are just
    // a documentation feature
    // Array<Token> retNames;
    
    Array<Ast_ProtoArg> args = { 0, 0 };
};

struct Ast_Function : public Ast_TopLevelStmt
{
    Ast_Function() { type = Function; }
    
    // Useful for IR generation because
    // all Allocas need to be at the beginning
    // for optimization purposes
    // TODO: Also this should probably be in the scope or something
    Array<Ast_Declaration*> declarations = { 0, 0 };
    
    // TODO: these can also not be pointers
    Ast_Prototype* prototype;
    Ast_BlockStmt* body;
};

struct Parser
{
    Arena* tempArena;
    
    // Ast storage
    Arena* arena;
    
    Tokenizer* tokenizer;
    
    Ast_Root root;
};

bool ParseRootNode(Parser *parser, Tokenizer *tokenizer);