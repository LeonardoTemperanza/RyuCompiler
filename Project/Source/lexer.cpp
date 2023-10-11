
#include "lexer.h"
#include "base.h"

// NOTE(Leo): To add new tokens that map to
// a specific string just add them to this macro.
// This is defined in such a way that it's possible
// to temporarily define a macro "X" that can do
// anything with these strings and types
#define KeywordStringTokenMapping     \
X("proc", Tok_Proc)               \
X("operator", Tok_Operator)       \
X("return", Tok_Return)           \
X("if", Tok_If)                   \
X("else", Tok_Else)               \
X("for", Tok_For)                 \
X("while", Tok_While)             \
X("do", Tok_Do)                   \
X("switch", Tok_Switch)           \
X("case", Tok_Case)               \
X("break", Tok_Break)             \
X("continue", Tok_Continue)       \
X("fallthrough", Tok_Fallthrough) \
X("default", Tok_Default)         \
X("defer", Tok_Defer)             \
X("const", Tok_Const)             \
X("struct", Tok_Struct)           \
X("cast", Tok_Cast)               \
/* Primitive types */             \
X("bool", Tok_Bool)               \
X("char", Tok_Char)               \
X("uint8", Tok_Uint8)             \
X("uint16", Tok_Uint16)           \
X("uint32", Tok_Uint32)           \
X("uint64", Tok_Uint64)           \
X("int8", Tok_Int8)               \
X("int16", Tok_Int16)             \
X("int32", Tok_Int32)             \
X("int", Tok_Int32)               \
X("int64", Tok_Int64)             \
X("float", Tok_Float)             \
X("double", Tok_Double)           \
X("raw", Tok_Raw)                 \
X("true", Tok_True)               \
X("false", Tok_False)             \
/* Decl specifiers */             \
X("extern", Tok_Extern)           \


// NOTE(Leo): Order matters here (<<= before <<)
#define OperatorStringTokenMapping \
X("->", Tok_Arrow)             \
X("++", Tok_Increment)         \
X("--", Tok_Decrement)         \
X("<<=", Tok_LShiftEquals)     \
X(">>=", Tok_RShiftEquals)     \
X("<<", Tok_LShift)            \
X(">>", Tok_RShift)            \
X("<=", Tok_LE)                \
X(">=", Tok_GE)                \
X("==", Tok_EQ)                \
X("!=", Tok_NEQ)               \
X("&&", Tok_LogAnd)            \
X("||", Tok_LogOr)             \
X("+=", Tok_PlusEquals)        \
X("-=", Tok_MinusEquals)       \
X("*=", Tok_MulEquals)         \
X("/=", Tok_DivEquals)         \
X("%=", Tok_ModEquals)         \
X("&=", Tok_AndEquals)         \
X("^=", Tok_XorEquals)         \
X("|=", Tok_OrEquals)

#define X(string, _) { (string), (int64)strlen(string) },
const String keywordStrings[] =
{
    KeywordStringTokenMapping
};
#undef X

#define X(_, tokenType) { (tokenType) },
const TokenType keywordTypes[] =
{
    KeywordStringTokenMapping
};
#undef X

#define X(string, _) { (string), (int64)strlen(string) },
const String operatorStrings[] =
{
    OperatorStringTokenMapping
};
#undef X

#define X(_, tokenType) { (tokenType) },
const TokenType operatorTypes[] =
{
    OperatorStringTokenMapping
};
#undef X

inline bool IsAlphabetic(char c)
{
    return ((c >= 'a') && (c <= 'z'))
        || ((c >= 'A') && (c <= 'Z'));
}

inline bool IsNumeric(char c)
{
    return c >= '0' && c <= '9';
}

inline bool IsWhitespace(char c)
{
    return (c == ' ')
        || (c == '\t')
        || (c == '\v')
        || (c == '\f')
        || (c == '\r')
        || IsNewline(c);
}

inline bool IsAllowedForStartIdent(char c)
{
    return IsAlphabetic(c)
        || c == '_';
}

inline bool IsAllowedForMiddleIdent(char c)
{
    return IsAllowedForStartIdent(c)
        || IsNumeric(c);
}

inline bool IsAllowedForMiddleNum(char c)
{
    return IsNumeric(c)
        || c == '.' || c == 'e' || c == '-';
}

inline bool IsNewline(char c)
{
    return c == '\n';
}

static void EatAllWhitespace(Tokenizer* t)
{
    bool loop = true;
    while(loop)
    {
        if(IsNewline(t->at[0]))
        {
            t->startOfCurLine = t->at - t->startOfFile + 1;
            ++t->curLineNum;
            ++t->at;
        }
        else if(IsWhitespace(t->at[0])) ++t->at;
        else if(t->at[0] == '/' && t->at[1] == '/')
        {
            // Handle comments
            while(!IsNewline(t->at[0])) ++t->at;
            
            t->startOfCurLine = t->at - t->startOfFile + 1;
            ++t->curLineNum;
            ++t->at;
        }
        // Multiline comments
        else if(t->at[0] == '/' && t->at[1] == '*')
        {
            while(t->at[0] != '*' || t->at[1] != '/')
            {
                // Stop if we got to EOF 
                if(t->at[0] == 0)
                    return;
                if(IsNewline(t->at[0]))
                {
                    t->startOfCurLine = t->at - t->startOfFile + 1;
                    ++t->curLineNum;
                }
                
                ++t->at;
            }
            
            // Eat the '*' and the '/'
            t->at += 2;
        }
        else
            loop = false;
    }
}

static void LexFile(Tokenizer* t)
{
    ProfileFunc(prof);
    
    while(true)
    {
        Token tok = GetToken(t);
        t->tokens.Append(t->arena, tok);
        
        if(tok.type == Tok_EOF || tok.type == Tok_Error)
            break;
    }
}

static Token GetToken(Tokenizer* t)
{
    EatAllWhitespace(t);
    
    Token result;
    result.type    = Tok_Error;
    result.sl      = t->startOfCurLine;
    result.lineNum = t->curLineNum;
    result.sc      = t->at - t->startOfFile;
    result.ec      = result.sc;
    result.text    = { t->at, 1 };
    
    if(t->at[0] == 0)  // String terminator
    {
        result.type = Tok_EOF;
        result.ec = result.sc;
    }
    else if(IsAllowedForStartIdent(t->at[0]))  // Identifiers
    {
        result.type = Tok_Ident;
        for(int i = 1; IsAllowedForMiddleIdent(t->at[i]); ++i)
            ++result.text.length;
        
        for(int i = 0; i < StArraySize(keywordStrings); ++i)
        {
            if(keywordStrings[i] == result.text)
            {
                result.type = keywordTypes[i];
                break;
            }
        }
        
        t->at += result.text.length;
        result.ec = result.sc + result.text.length - 1;
    }
    else if(IsNumeric(t->at[0]))// || (t->at[0] == '-' && IsNumeric(t->at[1])))  // Numbers
    {
        // Determine which type of number it is
        int length = 0;
        bool isFloatingPoint = false;
        bool isDoublePrecision = false;
        
        do
        {
            ++length;
            if(!isFloatingPoint && (t->at[length] == '.' || t->at[length] == 'e'))
                isFloatingPoint = true;
        }
        while(IsAllowedForMiddleNum(t->at[length]));
        
        
        // Use c-stdlib functions for number parsing
        // (parsing floating pointer numbers with accurate precision
        // is kinda hard)
        if(isFloatingPoint)
        {
            if(t->at[length] == 'd')
            {
                result.type = Tok_DoubleNum;
                char* endPtr;
                result.doubleValue = strtod(t->at, &endPtr);
                Assert(endPtr == t->at + length);
                
                isDoublePrecision = true;
                ++length;
            }
            else
            {
                result.type = Tok_FloatNum;
                char* endPtr;
                result.floatValue = strtof(t->at, &endPtr);
                Assert(endPtr == t->at + length);
                
                // Eat the optional 'f' at the end of single precision floating point
                if(t->at[length] == 'f')
                    ++length;
            }
        }
        else
        {
            result.type = Tok_IntNum;
            char* endPtr;
            result.intValue = strtoll(t->at, &endPtr, 10);
            if(endPtr == t->at)
            {
                SetErrorColor();
                fprintf(stderr, "Error");
                ResetColor();
                fprintf(stderr, ": Unidentified token\n");
                result.type = Tok_Error;
                result.doubleValue = 0.0f;
            }
            else
                Assert(endPtr == t->at + length);
            
        }
        
        t->at += length;
        result.text.length = length;
        result.ec = result.sc + result.text.length - 1;
    }
    /*
    else if(String_FirstCharsMatchEntireString(result.text.ptr, StrLit("true")))
    {
        String trueLit = StrLit("true");
        
    }
    else if(String_FirstCharsMatchEntireString(result.text.ptr, StrLit("false")))
    {
        String falseLit = StrLit("false");
    }*/
    else  // Anything else (operators, parentheses, etc)
    {
        
        bool found = false;
        String foundStr = { 0, 0 };
        for(int i = 0; i < StArraySize(operatorStrings); ++i)
        {
            if(String_FirstCharsMatchEntireString(result.text.ptr, operatorStrings[i]))
            {
                result.text.length = operatorStrings[i].length;
                result.type = operatorTypes[i];
                found = true;
                break;
            }
        }
        
        if(found)
        {
            t->at += result.text.length;
            result.ec = result.sc + result.text.length - 1;
        }
        else
        {
            result.type = (TokenType)t->at[0];
            result.text.length = 1;
            result.ec   = result.sc;
            ++t->at;
        }
    }
    
    return result;
}

void PrintFileLine(Token* token, char* fileContents)
{
    fprintf(stderr, "    > ");
    bool endOfLine = false;
    int i = token->sl;
    while(!endOfLine)
    {
        if(fileContents[i] == '\t')
            fprintf(stderr, "    ");
        else
            fprintf(stderr, "%c", fileContents[i]);
        
        ++i;
        endOfLine = fileContents[i] == '\r' ||
            fileContents[i] == '\n' ||
            fileContents[i] == 0;
    }
    
    int length = i - token->sl;
    
    fprintf(stderr, "\n    > ");
    
    for(int i = token->sl; i < token->sc; ++i)
    {
        if(fileContents[i] == '\t')
            fprintf(stderr, "    ");
        else
            fprintf(stderr, " ");
    }
    
    fprintf(stderr, "^");
    
    for(int i = token->sc; i < token->ec - 1; ++i)
    {
        if(fileContents[i] == '\t')
            fprintf(stderr, "----");
        else
            fprintf(stderr, "-");
    }
    
    if(token->sc != token->ec)
        fprintf(stderr, "^\n");
    else
        fprintf(stderr, "\n");
}

void CompileError(Tokenizer* t, Token* token, String message)
{
    if(token->type == Tok_EOF || token->type == Tok_Error)
    {
        printf("Reached unexpected EOF\n");
        return;
    }
    
    char* fileContents = t->startOfFile;
    
    SetErrorColor();
    t->lastCompileErrorNumChars = fprintf(stderr, "Error ");
    ResetColor();
    fprintf(stderr, "(%d,%d): ",
            token->lineNum,
            token->sc - token->sl + 1);
    
    fprintf(stderr, "%.*s\n", (int)message.length, message.ptr);
    
    PrintFileLine(token, fileContents);
    
    t->compileErrorPrinted = true;
}

void CompileErrorContinue(Tokenizer* t, Token* token, String message)
{
    if(!t->compileErrorPrinted)
        return;
    t->compileErrorPrinted = false;
    
    if(token->type == Tok_EOF || token->type == Tok_Error)
    {
        printf("Reached unexpected EOF\n");
        return;
    }
    
    char* fileContents = t->startOfFile;
    
    for(int i = 0; i < t->lastCompileErrorNumChars; ++i)
        fprintf(stderr, " ");
    
    fprintf(stderr, "(%d,%d): ",
            token->lineNum,
            token->sc - token->sl + 1);
    
    fprintf(stderr, "%.*s\n", (int)message.length, message.ptr);
    
    PrintFileLine(token, fileContents);
}

String TokTypeToString(TokenType tokType, Arena* dest)
{
    // No need to allocate in these cases
    for(int i = 0; i < StArraySize(keywordTypes); ++i)
    {
        if(tokType == keywordTypes[i])
            return keywordStrings[i];
    }
    
    for(int i = 0; i < StArraySize(operatorTypes); ++i)
    {
        if(tokType == operatorTypes[i])
            return operatorStrings[i];
    }
    
    if(tokType == Tok_Ident) return StrLit("identifier");
    if(tokType == Tok_EOF)   return StrLit("end of file");
    if(tokType == Tok_Error) return StrLit("error");
    
    // If it's just the ASCII code, then an arena
    // allocation is needed as tokType is stack allocated
    String result { 0, 1 };
    result.ptr = Arena_FromStackPack(dest, (char)tokType);
    return result;
}
