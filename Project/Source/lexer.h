
#pragma once

#include "base.h"

// NOTE(Leo): ASCII characters are reserved in the enum
// space, so that they can be used as token types. For instance,
// the plus operator's token type is the ASCII code for '+'.
enum TokenType
{
    // Ascii types
    
    // Lexer message types
    TokenEOF = 256,
    TokenError,
    
    // Operator types (not single character)
    TokenLE,
    TokenGE,
    TokenEQ,
    TokenNEQ,
    TokenSubscript,   // Not an actual token, only needed for operator precedence
    
    // Keywords
    TokenProc,
    TokenExtern,
    //TokenVar,  // TODO: use later for type inference
    TokenIf,
    TokenElse,
    TokenFor,
    TokenWhile,
    TokenReturn,
    TokenArrow,
    
    // Type qualifiers
    TokenConst,
    TokenVolatile,
    
    // Generic identifier, could be a type or
    // a variable name. The program is not
    // able to figure that out until later
    // in the semantic analysis stage
    TokenIdent,
    
    // Constants
    TokenNum,
    TokenStrLiteral
};

struct Token
{
    TokenType type = TokenError;
    
    uint32 sl, sc;  // Start of line, starting character
    uint32 ec;      // end character
    uint32 lineNum;
    
    // Additional information
    union
    {
        String ident;
        uint64 intValue;
        float floatValue;
        double doubleValue;
    };
};

#define Tokenizer_MaxPeekNum 10
// TODO: there should be file name here
struct Tokenizer
{
    char* startOfFile;
    char* at;
    uint32 startOfCurLine = 0;
    uint32 curLineNum = 1;
    
    // Circular buffer containing peeked tokens
    Token peekBuffer[Tokenizer_MaxPeekNum];
    int peekLength  = 0;
    int peekStart   = 0;
    
    // NOTE(Leo): Offset from start until the EOF
    // token. If < 0 there is no eof token in the
    // buffer.
    int eofTokenOffset = -1;
};

void EatToken(Tokenizer* t);
Token* PeekToken(Tokenizer* t, int lookahead);
inline Token* PeekNextToken(Tokenizer* t) 
{
    return PeekToken(t, 1);
}

void CompilationError(Token token, char* fileContents, char* prefix, char* message);