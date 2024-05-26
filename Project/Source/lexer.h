
#pragma once

#include "base.h"

// NOTE(Leo): ASCII characters are reserved in the enum
// space, so that they can be used as token types. For instance,
// the plus operator's token type is the ASCII code for '+'.
// NOTE(Leo): This and TypeId depend on each other (fix??)
enum TokenKind
{
    // Ascii types
    
    // Operator types (not single character)
    Tok_OperatorBegin = 257,
    Tok_LE = Tok_OperatorBegin,
    Tok_GE,
    Tok_EQ,
    Tok_NEQ,
    Tok_LogAnd,
    Tok_LogOr,
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
    Tok_Raw,
    Tok_PrimitiveTypesEnd = Tok_Raw,
    Tok_Ident,
    Tok_IdentEnd = Tok_Ident,
    
    // Keywords
    Tok_Proc,
    Tok_Operator,
    Tok_Struct,
    Tok_Extern,
    Tok_Var,
    Tok_If,
    Tok_Else,
    Tok_For,
    Tok_While,
    Tok_Do,
    Tok_Switch,
    Tok_Case,
    Tok_Break,
    Tok_Continue,
    Tok_Fallthrough,
    Tok_Default,
    Tok_Defer,
    Tok_Return,
    Tok_Cast,
    Tok_Arrow,  // For function return types
    
    // Type qualifiers
    Tok_Const,
    Tok_Volatile,
    
    // Literals
    Tok_IntNum,
    Tok_FloatNum,
    Tok_DoubleNum,
    Tok_StrLiteral,
    Tok_True,
    Tok_False,
    
    // Lexer message types
    Tok_EOF,
    Tok_Error,
};

struct Token
{
    TokenKind kind = Tok_Error;
    
    uint32 sl, sc;  // Start of line, starting character
    uint32 ec;      // end character
    uint32 lineNum;
    
    union
    {
        String text;
        HashedString ident;
    };
    
    // Additional information
    union
    {
        int64 intValue;
        float floatValue;
        double doubleValue;
    };
};

struct Tokenizer
{
    String path = { 0, 0 };
    
    char* fileContents;
    char* at;
    uint32 startOfCurLine = 0;
    uint32 curLineNum = 1;
    int commentNestLevel = 0;
    
    Arena* arena;
    Slice<Token> tokens = { 0, 0 };
    
    // Used for the not very good "Continue" functions
    bool compileErrorPrinted = false;
    int lastCompileErrorNumChars = 0;
};

Tokenizer InitTokenizer(Arena* arena, char* fileContents, String path);

static Token GetToken(Tokenizer* t);

inline bool IsAlphabetic(char c);
inline bool IsNumeric(char c);
inline bool IsWhitespace(char c);
inline bool IsAllowedForStartIdent(char c);
inline bool IsAllowedForMiddleIdent(char c);
inline bool IsAllowedForMiddleNum(char c);
inline bool IsNewline(char c);
static void EatAllWhitespace(Tokenizer* t);
static bool FindStringInStream(char* stream, char* string);
TokenKind MatchKeywords(Tokenizer* t, int tokenLength);
bool MatchAlpha(char* stream, char* str, int tokenLength);

static void LexFile(Tokenizer* t);

inline bool IsTokIdent(TokenKind tokType) { return tokType >= Tok_IdentBegin && tokType <= Tok_IdentEnd; }

void EatToken(Tokenizer* t);
Token* PeekToken(Tokenizer* t, int lookahead);
inline Token* PeekNextToken(Tokenizer* t) { return PeekToken(t, 1); }

void CompileError(Tokenizer* t, Token* token, String message);
void CompileErrorContinue(Tokenizer* t, Token* token, String message);
