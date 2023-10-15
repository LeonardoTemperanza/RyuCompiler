
#pragma once

#include "ast.h"
#include "dependency_graph.h"

struct Parser
{
    Arena* arena; // Ast storage
    Tokenizer* tokenizer;
    Token* at;  // Current analyzed token in the stream
    
    Ast_Block* scope = 0;  // Current scope
    Ast_Block* fileScope = 0;
    Ast_ProcDef* curProc = 0;
    
    Arena* entityArena;
    Slice<Dg_Entity> entities;
    
    Arena* internArena;
    Slice<ToIntern> internArray = { 0, 0 };
    
    // False if in error mode
    bool status = true;
};

template<typename t>
t* Ast_MakeEntityNode(Parser* p, Token* token);

void AddDeclToScope(Ast_Block* block, Ast_Declaration* decl);

Ast_FileScope* ParseFile(Parser* p);
Ast_ProcDecl* ParseProc(Parser* p, Ast_DeclSpec specs);
Ast_StructDef* ParseStructDef(Parser* p, Ast_DeclSpec specs);
Ast_Node* ParseDeclOrExpr(Parser* p, Ast_DeclSpec specs, bool forceInit = false, bool ignoreInit = false);
Ast_VarDecl* ParseVarDecl(Parser* p, Ast_DeclSpec specs, bool forceInit = false, bool ignoreInit = false);
Ast_Stmt* ParseStatement(Parser* p);
Ast_If* ParseIf(Parser* p);
Ast_For* ParseFor(Parser* p);
Ast_While* ParseWhile(Parser* p);
Ast_DoWhile* ParseDoWhile(Parser* p);
Ast_Switch* ParseSwitch(Parser* p);
Ast_Defer* ParseDefer(Parser* p);
Ast_Return* ParseReturn(Parser* p);
template<typename t> Ast_Stmt* ParseJump(Parser* p);  // Used for break, continue, and fallthrough
Ast_Block* ParseBlock(Parser* p);
Ast_Block* ParseOneOrMoreStmtBlock(Parser* p);  // Used for blocks in e.g. if stmts

Ast_Expr* ParseExpression(Parser* p, int prec = INT_MAX, bool ignoreEqual = false);
Ast_Expr* ParsePostfixExpression(Parser* p);
Ast_Expr* ParsePrimaryExpression(Parser* p);
TypeInfo* ParseType(Parser* p, Token** outIdent);
// Do I even need these to be values and not pointers anymore?
Ast_ProcType ParseProcType(Parser* p, Token** outIdent, bool forceArgNames = true);
Ast_StructType ParseStructType(Parser* p, Token** outIdent);
Ast_DeclSpec ParseDeclSpecs(Parser* p);
void CheckDeclSpecs(Parser* p, Ast_DeclSpec specs, Ast_DeclSpec allowedSpecs);

// Inserts the string and the associated atom address to patch later
// in the parser array (internArray)
#if 0
void DeferStringInterning(Parser* p, String string, Atom** atom);
#endif

// Operators

// NOTE: Declaration keywords need to be inserted here
inline bool IsTokStartOfDecl(TokenType tokType)
{
    return tokType == '^' || tokType == '[' || tokType == Tok_Struct || tokType == Tok_Proc
        || tokType == Tok_Extern || tokType == Tok_Var;
}

int GetOperatorPrec(TokenType tokType);
bool IsOpPrefix(TokenType tokType);
bool IsOpPostfix(TokenType tokType);
bool IsOperatorLToR(TokenType tokType);
bool IsTokOperator(TokenType tokType);

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
