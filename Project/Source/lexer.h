
#pragma once

#include "base.h"

// NOTE(Leo): ASCII characters are reserved in the enum
// space, so that they can be used as token types. For instance,
// the plus operator's token type is the ASCII code for '+'.
enum TokenType
{
    // Ascii types
    
    // Lexer message types
    Tok_EOF = 257,
    Tok_Error,
    
    // Operator types (not single character)
    Tok_OperatorBegin,
    Tok_LE = Tok_OperatorBegin,
    Tok_GE,
    Tok_EQ,
    Tok_NEQ,
    Tok_And,
    Tok_Or,
    Tok_Increment,
    Tok_Decrement,
    Tok_PlusEquals,
    Tok_MinusEquals,
    Tok_MulEquals,
    Tok_DivEquals,
    Tok_ModEquals,
    Tok_LShiftEquals,
    Tok_RShiftEquals,
    Tok_AndEquals,
    Tok_OrEquals,
    Tok_XorEquals,
    Tok_LShift,
    Tok_RShift,
    Tok_Subscript,   // Not an actual token, only needed for operator precedence
    Tok_OperatorEnd = Tok_Subscript,
    
    // Keywords
    Tok_Proc,
    Tok_Struct,
    Tok_Extern,
    //TokenVar,  // TODO: use later for type inference
    Tok_If,
    Tok_Else,
    Tok_For,
    Tok_While,
    Tok_Return,
    Tok_Arrow,  // For function return types
    
    // Type qualifiers
    Tok_Const,
    Tok_Volatile,
    
    // Generic identifier, could be a type or
    // a variable name. The program is not
    // able to figure that out until later
    // in the semantic analysis stage
    // TokenTypes after TokenIdentBegin are considered
    // identifiers by the parser
    // These are all considered "identifiers" by
    // the parser.
    Tok_IdentBegin,
    Tok_Int8 = Tok_IdentBegin,
    Tok_Int16,
    Tok_Int32,
    Tok_Int64,
    Tok_Uint8,
    Tok_Uint16,
    Tok_Uint32,
    Tok_Uint64,
    Tok_Float,
    Tok_Double,
    Tok_Ident,
    Tok_IdentEnd = Tok_Ident,
    
    // Constants
    Tok_Num,
    Tok_StrLiteral
};

struct Token
{
    TokenType type = Tok_Error;
    
    uint32 sl, sc;  // Start of line, starting character
    uint32 ec;      // end character
    uint32 lineNum;
    
    // Additional information
    union
    {
        String ident;
        uint64 uintValue;
        int64 intValue;
        float floatValue;
        double doubleValue;
    };
};

enum ParseStatus
{
    CompStatus_Error = 1,
    CompStatus_Success = 0
};

// TODO: there should be the file name here
struct Tokenizer
{
    char* startOfFile;
    char* at;
    uint32 startOfCurLine = 0;
    uint32 curLineNum = 1;
    
    Arena* arena;
    // Stream of tokens which compose the entire file
    Array<Token> tokens = { 0, 0 };
    
    ParseStatus status = CompStatus_Success;
};

void EatToken(Tokenizer* t);
Token* PeekToken(Tokenizer* t, int lookahead);
inline Token* PeekNextToken(Tokenizer* t) 
{
    return PeekToken(t, 1);
}

void CompileError(Tokenizer* t, Token* token, char* message);
void CompileError(Tokenizer* t, Token* token, int numStrings, char* message1, ...);
