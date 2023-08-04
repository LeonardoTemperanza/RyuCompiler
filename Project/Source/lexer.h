
#pragma once

#include "base.h"

// NOTE(Leo): ASCII characters are reserved in the enum
// space, so that they can be used as token types. For instance,
// the plus operator's token type is the ASCII code for '+'.
// NOTE(Leo): This and TypeId depend on each other (fix??)
enum TokenType
{
    // Ascii types
    
    // Operator types (not single character)
    Tok_OperatorBegin = 257,
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
    
    // Primitive types should be considered
    // as identifiers by the parser.
    Tok_IdentBegin,
    // Primitive types
    Tok_PrimitiveTypesBegin = Tok_IdentBegin,
    Tok_Bool = Tok_PrimitiveTypesBegin,
    Tok_Char,
    Tok_Uint8,
    Tok_Uint16,
    Tok_Uint32,
    Tok_Uint64,
    Tok_Int8,
    Tok_Int16,
    Tok_Int32,
    Tok_Int64,
    Tok_Float,
    Tok_Double,
    Tok_PrimitiveTypesEnd = Tok_Double,
    Tok_Ident,
    Tok_IdentEnd = Tok_Ident,
    
    // Keywords
    Tok_Proc,
    Tok_Operator,
    Tok_Struct,
    //Tok_Extern,
    //TokenVar,  // TODO: use later for type inference
    Tok_If,
    Tok_Else,
    Tok_For,
    Tok_While,
    Tok_Do,
    Tok_Break,
    Tok_Continue,
    Tok_Defer,
    Tok_Return,
    Tok_Cast,
    Tok_Arrow,  // For function return types
    
    // Type qualifiers
    Tok_Const,
    Tok_Volatile,
    
    // Literals
    Tok_Num,
    Tok_StrLiteral,
    
    // Lexer message types
    Tok_EOF,
    Tok_Error,
};

struct TokenHotData
{
    union
    {
        String ident;
        uint64 uintValue;
        int64 intValue;
        float floatValue;
        double doubleValue;
    };
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
        // Use token hot data either anonymously or not
        //TokenHotData tokenHotData;
        //union
        //{
        String ident;
        uint64 uintValue;
        int64 intValue;
        float floatValue;
        double doubleValue;
        //};
    };
};

enum ParseStatus
{
    CompStatus_Error = 1,
    CompStatus_Success = 0
};

// TODO: Implement file paths
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
    
    String fullFilePath = { 0, 0 };
    bool compileErrorPrinted = false;
    int lastCompileErrorNumChars = 0;
};

static Token GetToken(Tokenizer* t);

inline bool IsAlphabetic(char c);
inline bool IsNumeric(char c);
inline bool IsWhitespace(char c);
inline bool IsAllowedForStartIdent(char c);
inline bool IsAllowedForMiddleIdent(char c);
inline bool IsNewline(char c);
static void EatAllWhitespace(Tokenizer* t);
static bool FindStringInStream(char* stream, char* string);
static float String2Float(char* string, int length);

static void LexFile(Tokenizer* t);

inline bool IsTokIdent(TokenType tokType) { return tokType >= Tok_IdentBegin && tokType <= Tok_IdentEnd; }

void EatToken(Tokenizer* t);
Token* PeekToken(Tokenizer* t, int lookahead);
inline Token* PeekNextToken(Tokenizer* t) { return PeekToken(t, 1); }

void CompileError(Tokenizer* t, Token* token, String message);
void CompileErrorContinue(Tokenizer* t, Token* token, String message);
