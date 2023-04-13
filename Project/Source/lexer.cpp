
#include "lexer.h"
#include "base.h"

// NOTE(Leo): To add new tokens that map to
// a specific string just add them to this macro.
// This is defined in such a way that it's possible
// to temporarily define a macro "X" that can do
// anything with these strings and types
#define KeywordStringTokenMapping \
X("proc", Tok_Proc) \
X("extern", Tok_Extern) \
X("return", Tok_Return) \
/*X("var", Tok_Var)*/ \
X("if", Tok_If) \
X("else", Tok_Else) \
X("for", Tok_For) \
X("while", Tok_While) \
X("const", Tok_Const) \
X("int", Tok_Int32) \
X("int32", Tok_Int32) \
X("int8", Tok_Int8) \
X("int16", Tok_Int16) \
X("int32", Tok_Int32) \
X("int64", Tok_Int64) \
X("uint8", Tok_Uint8) \
X("uint16", Tok_Uint16) \
X("uint32", Tok_Uint32) \
X("uint64", Tok_Uint64) \
X("float", Tok_Float) \
X("double", Tok_Double)

// NOTE(Leo): Order matters here (<<= before <<)
#define OperatorStringTokenMapping \
X("++", Tok_Increment) \
X("--", Tok_Decrement) \
X("<<=", Tok_LShiftEquals) \
X(">>=", Tok_RShiftEquals) \
X("<<", Tok_LShift) \
X(">>", Tok_RShift) \
X("<=", Tok_LE) \
X(">=", Tok_GE) \
X("==", Tok_EQ) \
X("!=", Tok_NEQ) \
X("&&", Tok_And) \
X("||", Tok_Or) \
X("+=", Tok_PlusEquals) \
X("-=", Tok_MinusEquals) \
X("*=", Tok_MulEquals) \
X("/=", Tok_DivEquals) \
X("%=", Tok_ModEquals) \
X("&=", Tok_AndEquals) \
X("^=", Tok_XorEquals) \
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
        else if(IsWhitespace(t->at[0]))
            ++t->at;
        else if(t->at[0] == '/' &&
                t->at[1] == '/')
        {
            // Handle comments
            while(!IsNewline(t->at[0]))
                ++t->at;
            t->startOfCurLine = t->at - t->startOfFile + 1;
            ++t->curLineNum;
            ++t->at;
        }
        // Multiline comments
        else if(t->at[0] == '/' &&
                t->at[1] == '*')
        {
            while(t->at[0] != '*' ||
                  t->at[1] != '/')
            {
                // Stop if we got to the EOF 
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

// TODO(Leo): improve the implementation
// It's assumed that the string is properly formatted
static float String2Float(char* string, int length)
{
    float result = 0.0f;
    
    // It represents the decimal position (0.1, 0.01, ...)
    float decimalMultiplier = 1.0f;
    // The position in the string where '.' is located
    int dotPos = -1;
    for(int i = 0; i < length && string[i] != 0; ++i)
    {
        if(string[i] == '.')
            dotPos = i;
        else
        {
            if(dotPos != -1)
            {
                decimalMultiplier *= 0.1f;
                result += (float)(string[i] - '0') * decimalMultiplier;
            }
            else
            {
                // Before reaching the '.', the desired
                // effect is to multiply by 10 at each step
                // (example: 53 = 0*10+3=3 -> 3*10+5=53)
                result *= 10.0f;
                result += (float)(string[i] - '0');
            }
        }
    }
    
    return result;
}

static void LexFile(Tokenizer* t)
{
    while(true)
    {
        Token tok = GetToken(t);
        Array_AddElement(&t->tokens, t->arena, tok);
        
        if(tok.type == Tok_EOF)
            break;
    }
}

static Token GetToken(Tokenizer* t)
{
    ProfileFunc();
    
    EatAllWhitespace(t);
    
    Token result;
    result.type    = Tok_Error;
    result.sl      = t->startOfCurLine;
    result.lineNum = t->curLineNum;
    result.sc      = t->at - t->startOfFile;
    result.ident   = { t->at, 1 };
    
    if(t->at[0] == 0)
    {
        result.type = Tok_EOF;
    }
    else if(IsAllowedForStartIdent(t->at[0]))
    {
        result.type = Tok_Ident;
        for(int i = 1; IsAllowedForMiddleIdent(t->at[i]); ++i)
            ++result.ident.length;
        
        for(int i = 0; i < StArraySize(keywordStrings); ++i)
        {
            if(keywordStrings[i] == result.ident)
            {
                result.type = keywordTypes[i];
                break;
            }
        }
        
        t->at += result.ident.length;
        result.ec = result.sc + result.ident.length - 1;
    }
    else if(IsNumeric(t->at[0]))
    {
        result.type = Tok_Num;
        int length = 0;
        int numPoints = 0;
        do
        {
            ++length;
            if(t->at[length] == '.')
                ++numPoints;
        }
        while((IsNumeric(t->at[length]) ||
               t->at[length] == '.') &&
              numPoints <= 1);
        
        if(numPoints > 1)
        {
            printf("Error: Unidentified token\n");
            result.type = Tok_Error;
            result.doubleValue = 0.0f;
        }
        else
            result.doubleValue = String2Float(t->at, length);
        
        t->at += length;
        result.ec = result.sc + result.ident.length - 1;
    }
    else
    {
        // TODO: fix Lexer issue
        bool found = false;
        String foundStr = { 0, 0 };
        for(int i = 0; i < StArraySize(operatorStrings); ++i)
        {
            if(String_FirstCharsMatchEntireString(result.ident.ptr, operatorStrings[i]))
            {
                result.ident.length = operatorStrings[i].length;
                result.type = operatorTypes[i];
                found = true;
                break;
            }
        }
        
        if(found)
        {
            t->at += result.ident.length;
            result.ec = result.sc + result.ident.length - 1;
        }
        else
        {
            
            result.type = (TokenType)t->at[0];
            result.ident.length = 1;
            result.ec   = result.sc;
            ++t->at;
        }
    }
    
    
    return result;
}

// TODO: annoying duplicated code here

void CompileError(Tokenizer* t, Token* token, char* message)
{
    if(t->status == CompStatus_Error)
        return;
    
    t->status = CompStatus_Error;
    
    char* fileContents = t->startOfFile;
    
    fprintf(stderr, "%s (%d,%d): %s\n", "Error", token->lineNum, token->sc - token->sl + 1, message);
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
    
    fprintf(stderr, "\n");
    
    for(int i = token->sl; i < token->sc; ++i)
    {
        if(fileContents[i] == '\t')
            fprintf(stderr, "----");
        else
            fprintf(stderr, "-");
    }
    
    fprintf(stderr, "^\n");
}

// TODO: actually finish this
// NOTE: This necessitates a variable number of char* vars
void CompileError(Tokenizer* t, Token* token, int numStrings, char* message1, ...)
{
    printf("This function is not done\n");
    
    if(t->status == CompStatus_Error)
        return;
    
    t->status = CompStatus_Error;
    
    char* fileContents = t->startOfFile;
    
    va_list args;
    va_start(args, token);
    
    fprintf(stderr, "%s (%d,%d): ", "Error", token->lineNum, token->sc - token->sl + 1);
    for(int i = 0; i < numStrings; ++i)
    {
        
    }
    
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
    
    fprintf(stderr, "\n");
    
    for(int i = token->sl; i < token->sc; ++i)
    {
        if(fileContents[i] == '\t')
            fprintf(stderr, "----");
        else
            fprintf(stderr, "-");
    }
    
    fprintf(stderr, "^\n");
    
    va_end(args);
}

char* TokTypeToString(TokenType tokType)
{
    for(int i = 0; i < StArraySize(keywordTypes); ++i)
    {
        if(tokType == keywordTypes[i])
            return keywordStrings[i].ptr;
    }
    
    for(int i = 0; i < StArraySize(operatorTypes); ++i)
    {
        if(tokType == operatorTypes[i])
            return operatorStrings[i].ptr;
    }
    
    if(tokType == Tok_Ident) return "identifier";
    
    return "";
}