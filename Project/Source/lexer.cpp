
#include "lexer.h"
#include "base.h"

// NOTE(Leo): To add new tokens that map to
// a specific string just add them to this macro.
// This is defined in such a way that it's possible
// to temporarily define a macro "X" that can do
// anything with these strings and types
#define Lexer_StringTokenMapping \
X("proc", TokenProc) \
X("extern", TokenExtern) \
X("return", TokenReturn) \
/*X("var", TokenVar)*/ \
X("if", TokenIf) \
X("else", TokenElse) \
X("for", TokenFor) \
X("while", TokenWhile) \

// Counts the number of keywords
#define X(tokenString, tokenType) 1+
const int numTokenStrings = Lexer_StringTokenMapping 0;
#undef X

#define X(tokenString, tokenType) { (tokenString), (int64)strlen(tokenString) },
const String tokenStrings[numTokenStrings] =
{
    Lexer_StringTokenMapping
};
#undef X

#define X(tokenString, tokenType) { (tokenType) },
const TokenType tokenTypes[numTokenStrings] =
{
    Lexer_StringTokenMapping
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

static Token GetToken(Tokenizer* t)
{
    ProfileFunc();
    
    EatAllWhitespace(t);
    
    Token result;
    result.type    = TokenError;
    result.sl      = t->startOfCurLine;
    result.lineNum = t->curLineNum;
    result.sc      = t->at - t->startOfFile;
    result.ident   = { t->at, 1 };
    
    if(t->at[0] == 0)
    {
        result.type = TokenEOF;
    }
    else if(IsAllowedForStartIdent(t->at[0]))
    {
        result.type = TokenIdent;
        for(int i = 1; IsAllowedForMiddleIdent(t->at[i]); ++i)
            ++result.ident.length;
        
        for(int i = 0; i < numTokenStrings; ++i)
        {
            if(tokenStrings[i] == result.ident)
            {
                result.type = tokenTypes[i];
                break;
            }
        }
        
        t->at += result.ident.length;
        result.ec = result.sc + result.ident.length - 1;
    }
    else if(IsNumeric(t->at[0]))
    {
        result.type = TokenNum;
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
            result.type = TokenError;
            result.doubleValue = 0.0f;
        }
        else
            result.doubleValue = String2Float(t->at, length);
        
        t->at += length;
        result.ec = result.sc + result.ident.length - 1;
    }
    else
    {
        String tmpStr = { t->at, 2 };
        if(tmpStr == "<=")
        {
            result.type = TokenLE;
            result.ec = result.sc + 1;
            t->at += 2;
        }
        else if(tmpStr == ">=")
        {
            result.type = TokenGE;
            result.ec = result.sc + 1;
            t->at += 2;
        }
        else if(tmpStr == "==")
        {
            result.type = TokenEQ;
            result.ec = result.sc + 1;
            t->at += 2;
        }
        else if(tmpStr == "!=")
        {
            result.type = TokenNEQ;
            result.ec = result.sc + 1;
            t->at += 2;
        }
        else if(tmpStr == "->")
        {
            result.type = TokenArrow;
            result.ec = result.sc + 1;
            t->at += 2;
        }
        else
        {
            result.type = (TokenType)result.ident[0];
            result.ec   = result.sc;
            ++t->at;
        }
    }
    
    return result;
}

void EatToken(Tokenizer* t)
{
    t->peekStart = (t->peekStart + 1) % Tokenizer_MaxPeekNum;
    
    if(t->peekLength == 0)
        PeekNextToken(t);
    
    --t->peekLength;
    if(t->eofTokenOffset > 0)
        --t->eofTokenOffset;
}

// NOTE(Leo): lookahead starts from 1 (I thought it was
// the one that intuitively made the most sense)
Token* PeekToken(Tokenizer* t, int lookahead)
{
    Assert(lookahead < Tokenizer_MaxPeekNum &&
           "Exceeded max peek limit!");
    
    // If trying to peek past the EOF token, keep returning
    // that instead.
    if(t->eofTokenOffset >= 0 && t->peekLength >= t->eofTokenOffset)
    {
        int idx = (t->peekStart + t->eofTokenOffset) % Tokenizer_MaxPeekNum;
        return t->peekBuffer + idx;
    }
    
    if(lookahead > t->peekLength)
    {
        for(int i = t->peekLength; i < lookahead; ++i)
        {
            int idx = (t->peekStart + i) % Tokenizer_MaxPeekNum;
            t->peekBuffer[idx] = GetToken(t);
        }
        
        t->peekLength += (lookahead - t->peekLength);
    }
    
    int idx = (t->peekStart + lookahead - 1) % Tokenizer_MaxPeekNum;
    return t->peekBuffer + idx;
}

void CompilationError(Token token, Tokenizer* t, char* prefix, char* message)
{
    char* fileContents = t->startOfFile;
    
    fprintf(stderr, "%s (%d,%d): %s\n", prefix, token.lineNum, token.sc - token.sl + 1, message);
    bool endOfLine = false;
    int i = token.sl;
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
    
    int length = i - token.sl;
    
    fprintf(stderr, "\n");
    
    for(int i = token.sl; i < token.sc; ++i)
    {
        if(fileContents[i] == '\t')
            fprintf(stderr, "----");
        else
            fprintf(stderr, "-");
    }
    
    fprintf(stderr, "^\n");
}

void CompilationError(Tokenizer* t, char* prefix, char* message)
{
    CompilationError(*PeekNextToken(t), t->startOfFile, prefix, message);
}