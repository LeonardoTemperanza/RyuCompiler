
#pragma once

#include "ast.h"

struct Parser
{
    Arena* arena; // Ast storage
    Tokenizer* tokenizer;
    Token* at;  // Current analyzed token in the stream
    
    Ast_Block* scope = 0;  // Current scope
    Ast_Block* globalScope = 0;
    
    Arena* internArena;
    Array<ToIntern> internArray = { 0, 0 };
};

Ast_FileScope* ParseFile(Parser* p);
Ast_ProcDef* ParseProcDef(Parser* p);
Ast_StructDef* ParseStructDef(Parser* p);
Ast_Node* ParseDeclOrExpr(Parser* p, bool forceInit = false);
Ast_VarDecl* ParseVarDecl(Parser* p, bool forceInit = false, bool preventInit = false);
Ast_Stmt* ParseStatement(Parser* p);
Ast_If* ParseIf(Parser* p);
Ast_For* ParseFor(Parser* p);
Ast_While* ParseWhile(Parser* p);
Ast_DoWhile* ParseDoWhile(Parser* p);
Ast_Defer* ParseDefer(Parser* p);
Ast_Return* ParseReturn(Parser* p);
Ast_Break* ParseBreak(Parser* p);
Ast_Continue* ParseContinue(Parser* p);
Ast_Block* ParseBlock(Parser* p);
Ast_Block* ParseOneOrMoreStmtBlock(Parser* p);  // Used for blocks in i.e. if stmts

Ast_Expr* ParseExpression(Parser* p, int prec = INT_MAX);
Ast_Expr* ParsePostfixExpression(Parser* p);
Ast_Expr* ParsePrimaryExpression(Parser* p);
TypeInfo* ParseType(Parser* p, Token** outIdent);
// Do i even need these to be values and not pointers anymore?
Ast_DeclaratorProc ParseDeclProc(Parser* p, Token** outIdent, bool forceArgNames = true);
Ast_DeclaratorStruct ParseDeclStruct(Parser* p, Token** outIdent);

// Inserts the string and the associated atom address to patch later
// in the parser array (internArray)
void DeferStringInterning(Parser* p, String string, Atom** atom);

// Operators

// TODO: @temporary Put other declaration keywords here
inline bool IsTokStartOfDecl(TokenType tokType)
{
    return tokType == '^' || tokType == '[' || tokType == Tok_Struct || tokType == Tok_Proc;
};

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
