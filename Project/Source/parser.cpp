
#include "parser.h"

// Returns value of operator precedence (1 is the lowest),
// and returns -1 if the supplied token is not an operator
static int GetOperatorPrecedence(Token* token);

static Ast_ExprNode* ParsePrimaryExpression(Parser* p);
static Ast_ExprNode* ParseExpression(Parser* p,
                                     int prec = INT_MIN);
static Ast_ExprNode* ParseExpression(Parser* p,
                                     Ast_ExprNode* left,
                                     int prec = INT_MIN);
static Array<Ast_ExprNode*> ParseExpressionArray(Parser* p, bool* err);

struct TryParse_Ret
{
    Ast_ExprNode* expr;
    bool error;
};
static TryParse_Ret TryParseExpression(Parser* p,
                                       TokenType expected);
static Ast_Function* ParseDefinition(Parser* p);
static Ast_Prototype* ParseExtern(Parser* p);

static Ast_Stmt* ParseStatement(Parser* p);
static Ast_If* ParseIfStatement(Parser* p);
static Ast_For* ParseForStatement(Parser* p);
// TODO: this will probably replace the previous
// "ParseExpressionOrDeclaration"
static Ast_Stmt* ParseDeclOrExpr(Parser* p,
                                 bool forceInitialization);
static Ast_ExprOrDecl* ParseExpressionOrDeclaration(Parser* p,
                                                    bool forceInitialization);
struct TryParseDecl_Ret
{
    Ast_ExprOrDecl* node;
    bool error;
};
static TryParseDecl_Ret TryParseExpressionOrDeclaration(Parser* p,
                                                        bool forceInitialization,
                                                        TokenType expected);
static Ast_While* ParseWhileStatement(Parser* p);
static Ast_ReturnStmt* ParseReturnStatement(Parser* p);
static Ast_DeclStmt* ParseDeclarationStatement(Parser* p,
                                               bool forceInitialization);
static Ast_BlockStmt* ParseBlockStatement(Parser* p);
static Ast_Type ParseType(Parser* p, bool* err);

static void SyntaxError(Token token, Parser* tokenizer, char* message);
static void SyntaxError(Parser* parser, char* message);

static void SyntaxError(Token token, Parser* parser, char* message)
{
    CompilationError(token, parser->tokenizer, "Syntax error", message);
}

static void SyntaxError(Parser* parser, char* message)
{
    CompilationError(parser->tokenizer, "Syntax error", message);
}

inline int GetOperatorPrecedence(Token* token)
{
    switch(token->type)
    {
        default: return -1;
        case TokenLE:
        case TokenGE:
        case TokenEQ:
        case TokenNEQ:
        case '<':
        case '>': return 10;
        case '+': return 20;
        case '-': return 20;
        case '/':
        case '*': return 40;
        case '=': return 5;
    }
}

inline bool IsUnaryOperator(Token* token)
{
    switch(token->type)
    {
        default:  return false;
        case '*':
        case '-':
        case '+': return true;
    }
}

inline bool IsOperatorLeftToRightAssociative(Token* token)
{
    if(token->type == '=')
        return false;
    return true;
}

inline Ast_ExprType TokenToExprType(Token* token)
{
    switch(token->type)
    {
        default:         return IdentNode;
        case TokenIdent: return IdentNode;
        case TokenNum:   return NumNode;
    }
}

// TODO: This function should be refactored
static Ast_ExprNode* ParsePrimaryExpression(Parser* p)
{
    ProfileFunc();
    
    Ast_ExprNode* node = 0;
    Token* token = PeekNextToken(p->tokenizer);
    
    // It's a function call
    if(token->type == TokenIdent &&
       PeekToken(p->tokenizer, 2)->type == '(')
    {
        auto callNode = Arena_AllocAndInit(p->arena, Ast_CallExprNode);
        callNode->token       = *token;
        callNode->args.ptr    = 0;
        callNode->args.length = 0;
        
        EatToken(p->tokenizer);  // Eat identifier
        EatToken(p->tokenizer);  // Eat '('
        
        TempArenaMemory savepoint = Arena_TempBegin(p->tempArena);
        defer(Arena_TempEnd(savepoint));
        
        Array<Ast_ExprNode*> argsArray = { 0, 0 };
        Token* token = PeekNextToken(p->tokenizer);
        if(token->type != ')')
        {
            int curArg = 0;
            while(true)
            {
                Ast_ExprNode* tmp = ParseExpression(p);
                if(!tmp)
                    return 0;
                
                Array_AddElement(&argsArray, p->tempArena, tmp);
                
                token = PeekNextToken(p->tokenizer);
                if(token->type == ')')
                    break;
                
                if(token->type != ',')
                {
                    SyntaxError(p, "Expected ','");
                    return 0;
                }
                
                EatToken(p->tokenizer);  // Eat ','
            }
            
            callNode->args = Array_CopyToArena(argsArray, p->arena);
        }
        
        node = callNode;
        EatToken(p->tokenizer);  // Eat ')'
    }
    // It's a subscript expression
    else if(token->type == TokenIdent &&
            PeekToken(p->tokenizer, 2)->type == '[')
    {
        Token ident = *token;
        EatToken(p->tokenizer);  // Eat ident
        EatToken(p->tokenizer);  // Eat '['
        
        auto subscript = Arena_AllocAndInit(p->arena, Ast_SubscriptExprNode);
        subscript->token = ident;
        subscript->indexExpr = ParseExpression(p);
        if(!subscript->indexExpr)
            return 0;
        
        if(PeekNextToken(p->tokenizer)->type != ']')
        {
            SyntaxError(p, "Expected ']'");
            return 0;
        }
        
        EatToken(p->tokenizer);  // Eat ']'
        node = subscript;
    }
    // It's an identifier/literal
    else if(token->type == TokenIdent || token->type == TokenNum)
    {
        node = Arena_AllocAndInit(p->arena, Ast_ExprNode);
        node->token = *token;
        node->type = TokenToExprType(token);
        
        EatToken(p->tokenizer);  // Eat ident/num
    }
    // Parenthesis expression
    else if(token->type == '(')
    {
        EatToken(p->tokenizer);  // Eat '('
        node = ParseExpression(p);
        if(PeekNextToken(p->tokenizer)->type == ')')
            EatToken(p->tokenizer);  // Eat ')'
        else
        {
            SyntaxError(p, "Expected ')' at the end of expression");
            return 0;
        }
    }
    // Unary expression
    // You could accomplish the same thing by using
    // a different precedence for the unary operator - or something.
    // I wonder if that would be better
    else if(IsUnaryOperator(token))
    {
        Token unary = *token;
        EatToken(p->tokenizer);  // Eat unary
        
        auto unaryNode = Arena_AllocAndInit(p->arena, Ast_UnaryExprNode);
        unaryNode->token = unary;
        unaryNode->expr = ParsePrimaryExpression(p);
        node = unaryNode;
    }
    // Array expression
    else if(token->type == '{')
    {
        Token bracket = *token;
        EatToken(p->tokenizer);  // Eat '}'
        
        auto arrayNode = Arena_AllocAndInit(p->arena, Ast_ArrayInitNode);
        arrayNode->token = bracket;
        bool err = false;
        arrayNode->expressions = ParseExpressionArray(p, &err);
        if(err)
            return 0;
        
        node = arrayNode;
        EatToken(p->tokenizer);
    }
    else if(token->type != TokenError)
    {
        //SyntaxError(*token, p->tokenizer, "Expected expression");
        //return 0;
    }
    return node;
}

static Ast_ExprNode* ParseExpression(Parser* p,
                                     int prec)
{
    ProfileFunc();
    
    Ast_ExprNode* left = ParsePrimaryExpression(p);
    if(!left)
        return 0;
    
    while(true)
    {
        Token* token = PeekNextToken(p->tokenizer);
        int tokenPrec = GetOperatorPrecedence(token);
        // Stop the recursion
        if(tokenPrec == prec)
        {
            if(IsOperatorLeftToRightAssociative(token))
                return left;
        }
        else if(tokenPrec < prec || tokenPrec == -1)
            return left;
        
        // This is a binary operation
        
        auto node = Arena_AllocAndInit(p->arena, Ast_BinExprNode);
        node->token = *token;
        node->left  = left;
        EatToken(p->tokenizer);  // Eat operator
        node->right = ParseExpression(p, tokenPrec);
        
        if(!node->right)
            return 0;
        
        left = node;
    }
}

static Ast_ExprNode* ParseExpression(Parser* p,
                                     Ast_ExprNode* left,
                                     int prec)
{
    ProfileFunc();
    
    while(true)
    {
        Token* token = PeekNextToken(p->tokenizer);
        int tokenPrec = GetOperatorPrecedence(token);
        // Stop the recursion
        if(tokenPrec == prec)
        {
            if(IsOperatorLeftToRightAssociative(token))
                return left;
        }
        else if(tokenPrec < prec || tokenPrec == -1)
            return left;
        
        // This is a binary operation
        
        auto node = Arena_AllocAndInit(p->arena, Ast_BinExprNode);
        node->token = *token;
        node->left  = left;
        EatToken(p->tokenizer);  // Eat operator
        node->right = ParseExpression(p, tokenPrec);
        
        if(!node->right)
            return 0;
        
        left = node;
    }
}

// TODO: I think this could be refactored
static TryParse_Ret TryParseExpression(Parser* p, TokenType expected)
{
    ProfileFunc();
    
    Ast_ExprNode* node = 0;
    
    if(PeekNextToken(p->tokenizer)->type != expected)
    {
        node = ParseExpression(p, false);
        if(PeekNextToken(p->tokenizer)->type != expected)
        {
            // TODO: Print expected token here
            SyntaxError(p, "Unexpected token");
            TryParse_Ret ret { 0, true };
            return ret;
        }
    }
    
    EatToken(p->tokenizer);   // Eat 'expected'
    TryParse_Ret ret { node, false };
    return ret;
}

static Ast_Prototype* ParsePrototype(Parser* p)
{
    ProfileFunc();
    
    Token token = *PeekNextToken(p->tokenizer);
    
    if(token.type != TokenIdent)
    {
        SyntaxError(p, "Expected function name in prototype");
        return 0;
    }
    
    String ident = token.ident;
    EatToken(p->tokenizer);   // Eat identifier
    
    if(PeekNextToken(p->tokenizer)->type != '(')
    {
        SyntaxError(p, "Expected '(' in prototype");
        return 0;
    }
    
    EatToken(p->tokenizer);   // Eat (
    
    auto node = Arena_AllocAndInit(p->arena, Ast_Prototype);
    node->token       = token;
    node->args.ptr    = 0;
    node->args.length = 0;
    
    Token currentToken = *PeekNextToken(p->tokenizer);
    bool continueLoop = currentToken.type == TokenIdent;
    while(continueLoop)
    {
        if(currentToken.type != TokenIdent)
        {
            SyntaxError(p, "Expected identifier");
            return 0;
        }
        
        EatToken(p->tokenizer);  // Eat ident
        
        bool isPtr = false;
        if(PeekNextToken(p->tokenizer)->type == '*')
        {
            isPtr = true;
            EatToken(p->tokenizer);  // Eat '*'
        }
        
        Ast_ProtoArg arg = { currentToken, isPtr };
        Array_AddElement(&node->args, p->arena, arg);
        
        if(PeekNextToken(p->tokenizer)->type == ',')
            EatToken(p->tokenizer);  // Eat ','
        else
            continueLoop = false;
        
        currentToken = *PeekNextToken(p->tokenizer);
    }
    
    if(PeekNextToken(p->tokenizer)->type != ')')
    {
        SyntaxError(p, "Expected ')' at end of prototype");
        EatToken(p->tokenizer);
        return 0;
    }
    
    EatToken(p->tokenizer);   // Eat ')'
    
    // If there are return values
    if(PeekNextToken(p->tokenizer)->type == TokenArrow)
    {
        EatToken(p->tokenizer);  // Eat "->"
        
        Token token = *PeekNextToken(p->tokenizer);
        if(token.type != TokenIdent)
        {
            SyntaxError(p, "Expecting return type(s) after '->'");
            return 0;
        }
        
        auto savepoint = Arena_TempBegin(p->tempArena);
        defer(Arena_TempEnd(savepoint));
        
        // NOTE(Leo): Ignoring return names, for now.
        Array<Ast_Type> types = { 0, 0 };
        while(true)
        {
            bool err = false;
            Ast_Type type = ParseType(p, &err);
            if(err)
                return 0;
            
            Array_AddElement(&types, p->tempArena, type);
            
            if(PeekNextToken(p->tokenizer)->type == TokenIdent)
                EatToken(p->tokenizer);  // Eat ident
            
            Token token = *PeekNextToken(p->tokenizer);
            if(token.type != ',')
                break;
            
            EatToken(p->tokenizer);  // Eat ','
        }
        
        types = Array_CopyToArena(types, p->arena);
        
        node->retTypes = types;
    }
    
    return node;
}

static Ast_Function* ParseDefinition(Parser* p)
{
    ProfileFunc();
    
    EatToken(p->tokenizer);   // Eat def
    
    auto node = Arena_AllocAndInit(p->arena, Ast_Function);
    node->prototype = ParsePrototype(p);
    if(!node->prototype)
        return 0;
    node->body = ParseBlockStatement(p);
    if(!node->body)
        return 0;
    
    return node;
}

static Ast_Prototype* ParseExtern(Parser* p)
{
    ProfileFunc();
    
    EatToken(p->tokenizer);   // Eat extern
    
    Ast_Prototype* proto = ParsePrototype(p);
    
    Token next = *PeekNextToken(p->tokenizer);
    if(next.type != ';')
    {
        SyntaxError(p, "Expected ';'");
        return 0;
    }
    
    return proto;
}

static Ast_Stmt* ParseStatement(Parser* p)
{
    ProfileFunc();
    
    Ast_Stmt* result = 0;
    bool expectingSemicolon = true;
    switch (PeekNextToken(p->tokenizer)->type)
    {
        // Empty statement
        case ';':
        {
            EatToken(p->tokenizer);   // Eat ';'
            expectingSemicolon = false;
            result = Arena_AllocAndInit(p->arena, Ast_Stmt);
            result->type = EmptyStmt;
        } break;
        
        case TokenReturn:
        {
            result = ParseReturnStatement(p);
        }
        break;
        
        case '{':
        {
            result = ParseBlockStatement(p);
            expectingSemicolon = false;
        }
        break;
        
        case TokenIf:
        {
            result = ParseIfStatement(p);
            expectingSemicolon = false;
        }
        break;
        
        case TokenFor:
        {
            result = ParseForStatement(p);
            expectingSemicolon = false;
        }
        break;
        
        case TokenWhile:
        {
            result = ParseWhileStatement(p);
            expectingSemicolon = false;
        }
        break;
        
        default:
        {
            // If it's none of the keywords,
            // then it could either be a declaration or
            // an expression.
            result = ParseDeclOrExpr(p, false);
        }
        break;
    }
    
    if(result && expectingSemicolon)
    {
        if(PeekNextToken(p->tokenizer)->type != ';')
        {
            SyntaxError(p, "Expected ';' at the end of statement");
            return 0;
        }
        
        EatToken(p->tokenizer);
    }
    
    return result;
}

static Ast_If* ParseIfStatement(Parser* p)
{
    ProfileFunc();
    
    if(PeekNextToken(p->tokenizer)->type != TokenIf)
    {
        SyntaxError(p, "Expected if statement");
        return 0;
    }
    
    if(PeekToken(p->tokenizer, 2)->type != '(')
    {
        SyntaxError(p, "Expected '('");
        return 0;
    }
    
    EatToken(p->tokenizer);   // Eat token 'if'
    EatToken(p->tokenizer);   // Eat token '('
    
    auto ifStmt  = Arena_AllocAndInit(p->arena, Ast_If);
    ifStmt->cond = ParseExpressionOrDeclaration(p, true);
    if(!ifStmt->cond)
        return 0;
    
    if(PeekNextToken(p->tokenizer)->type != ')')
    {
        SyntaxError(p, "Expected ')' at the end of condition");
        return 0;
    }
    
    EatToken(p->tokenizer);   // Eat token ')'
    
    ifStmt->trueStmt = ParseStatement(p);
    if(!ifStmt->trueStmt)
        return 0;
    if(PeekNextToken(p->tokenizer)->type == TokenElse)
    {
        EatToken(p->tokenizer);   // Eat 'else'
        ifStmt->falseStmt = ParseStatement(p);
        if(!ifStmt->falseStmt)
            return 0;
    }
    
    return ifStmt;
}

static Ast_For* ParseForStatement(Parser* p)
{
    ProfileFunc();
    
    if(PeekNextToken(p->tokenizer)->type != TokenFor)
    {
        SyntaxError(p, "Expected for statement");
        return 0;
    }
    
    if(PeekToken(p->tokenizer, 2)->type != '(')
    {
        SyntaxError(p, "Expected '('");
        return 0;
    }
    
    EatToken(p->tokenizer);   // Eat 'for'
    EatToken(p->tokenizer);   // Eat '('
    
    auto forStmt  = Arena_AllocAndInit(p->arena, Ast_For);
    
    // Try to parse the decl/expr only if it's not empty
    TryParseDecl_Ret result;
    result = TryParseExpressionOrDeclaration(p, false, (TokenType)';');
    forStmt->init = result.node;
    if(result.error)
        return 0;
    result = TryParseExpressionOrDeclaration(p, true, (TokenType)';');
    forStmt->cond = result.node;
    if(result.error)
        return 0;
    TryParse_Ret exprResult;
    exprResult = TryParseExpression(p, (TokenType)')');
    forStmt->iteration = exprResult.expr;
    if(exprResult.error)
        return 0;
    forStmt->execStmt = ParseStatement(p);
    if(!forStmt->execStmt)
        return 0;
    
    return forStmt;
}

static Ast_While* ParseWhileStatement(Parser* p)
{
    ProfileFunc();
    
    if(PeekNextToken(p->tokenizer)->type != TokenWhile)
    {
        SyntaxError(p, "Expected while statement");
        return 0;
    }
    
    if(PeekToken(p->tokenizer, 2)->type != '(')
    {
        SyntaxError(p, "Expected '('");
        return 0;
    }
    
    EatToken(p->tokenizer);   // Eat 'while'
    EatToken(p->tokenizer);   // Eat '('
    
    auto whileStmt = Arena_AllocAndInit(p->arena, Ast_While);
    whileStmt->cond = ParseExpressionOrDeclaration(p, true);
    if(!whileStmt->cond)
        return 0;
    
    if(PeekNextToken(p->tokenizer)->type != ')')
    {
        SyntaxError(p, "Expected ')' at the end of condition");
        return 0;
    }
    
    EatToken(p->tokenizer);   // Eat ')'
    
    whileStmt->execStmt = ParseStatement(p);
    if(!whileStmt->execStmt)
        return 0;
    
    return whileStmt;
}

static Ast_ExprOrDecl* ParseExpressionOrDeclaration(Parser* p,
                                                    bool forceInitialization)
{
    ProfileFunc();
    
    auto exprOrDecl = Arena_AllocAndInit(p->arena, Ast_ExprOrDecl);
    /*if(PeekNextToken(p->tokenizer)->type == TokenVar)
    {
        exprOrDecl->isDecl = true;
        exprOrDecl->decl   = ParseDeclarationStatement(p, forceInitialization);
        if(!exprOrDecl->decl)
            return 0;
    }
    else
    {
        exprOrDecl->isDecl = false;
        exprOrDecl->expr   = ParseExpression(p);
        if(!exprOrDecl->expr)
            return 0;
    }
    */
    return exprOrDecl;
}

// Expressions separated with ','
static Array<Ast_ExprNode*> ParseExpressionArray(Parser* p, bool* err)
{
    Array<Ast_ExprNode*> result = { 0, 0 };
    *err = false;
    
    auto expr = ParseExpression(p);
    while(expr)
    {
        Array_AddElement(&result, p->tempArena, expr);
        
        if(PeekNextToken(p->tokenizer)->type != ',')
        {
            *err = true;
            return result;
        }
        
        EatToken(p->tokenizer);  // Eat ','
        auto expr = ParseExpression(p);
    }
    
    result = Array_CopyToArena(result, p->arena);
    return result;
};

static Ast_Type ParseType(Parser* p, bool* err)
{
    Ast_Type result;
    
    Token t = *PeekNextToken(p->tokenizer);
    uint8 numPointers = 0;
    while(t.type == '*')
    {
        EatToken(p->tokenizer);
        t = *PeekNextToken(p->tokenizer);
        
        //++result.numPointers;
    }
    
    t = *PeekNextToken(p->tokenizer);
    if(t.type != TokenIdent)
    {
        SyntaxError(p, "Expecting identifier here");
        *err = true;
        return result;
    }
    
    EatToken(p->tokenizer);  // Eat ident
    
    result.baseType = t;
    if(PeekNextToken(p->tokenizer)->type == '(')
    {
        EatToken(p->tokenizer);  // Eat '('
        result.params = ParseExpressionArray(p, err);
        if(*err)
            return result;
        
        if(PeekNextToken(p->tokenizer)->type != ')')
        {
            SyntaxError(p, "Expecting ')'");
            *err = true;
            return result;
        }
        
        EatToken(p->tokenizer);  // Eat ')'
    }
    
    return result;
}

// This is pretty messy... Would just be better to it the other way.
static Ast_Stmt* ParseDeclOrExpr(Parser* p,
                                 bool forceInitialization)
{
    Ast_Stmt* result = 0;
    if(PeekNextToken(p->tokenizer)->type == ';')
    {
        EatToken(p->tokenizer);  // Eat ';'
        return result;
    }
    
    auto savepoint = Arena_TempBegin(p->tempArena);
    defer(Arena_TempEnd(savepoint));
    
    Token t = *PeekNextToken(p->tokenizer);
    
    Array<Ast_TypeDescriptor> typeDesc = { 0, 0 };
    // If determined to be a declaration
    bool isDecl = false;
    // TODO: Other keywords might be here
    while(t.type == '^' || t.type == '[')
    {
        isDecl = true;
        Ast_ExprNode* arrSize = 0;
        if(t.type == '[')
        {
            t.type = TokenSubscript;
            arrSize = ParseExpression(p);
        }
        
        Array_AddElement(&typeDesc, p->tempArena, { t, arrSize } );
        EatToken(p->tokenizer);
        t = *PeekNextToken(p->tokenizer);
    }
    
    typeDesc = Array_CopyToArena(typeDesc, p->arena);
    
    Array<Ast_ExprNode*> exprArr = { 0, 0 };
    Token ident;  // Either for function call or for
    
    // NOTE: there has to be a single element in here,
    // if it was a polymorphic struct 
    if(PeekToken(p->tokenizer, 1)->type == TokenIdent &&
       PeekToken(p->tokenizer, 2)->type == '(')
    {
        ident = *PeekNextToken(p->tokenizer);
        
        EatToken(p->tokenizer);
        EatToken(p->tokenizer);
        bool err = false;
        exprArr = ParseExpressionArray(p, &err);
        if(err)
            return 0;
        
        if(PeekNextToken(p->tokenizer)->type != ')')
        {
            SyntaxError(p, "Expecting ')'");
            return 0;
        }
        
        EatToken(p->tokenizer);  // Eat ')'
        
        if(PeekNextToken(p->tokenizer)->type == TokenIdent)
        {
            isDecl = true;
            EatToken(p->tokenizer);
        }
    }
    else if(PeekToken(p->tokenizer, 1)->type == TokenIdent &&
            PeekToken(p->tokenizer, 2)->type == TokenIdent)
    {
        isDecl = true;
        EatToken(p->tokenizer);
    }
    
    if(isDecl)
    {
        auto decl = Arena_AllocAndInit(p->arena, Ast_Declaration);
        decl->typeInfo.typeDesc = typeDesc;
        decl->typeInfo.baseType = ident;
        decl->nameIdent = *PeekNextToken(p->tokenizer);
        if(decl->nameIdent.type != TokenIdent)
        {
            SyntaxError(p, "Unexpected token");
            return 0;
        }
        
        EatToken(p->tokenizer);
        
        // Don't forget to do the initialization stuff here
        
        result = decl;
    }
    else
    {
        // If we set the expression previously,
        // AND it's not a declaration then this
        // has to be a function call
        if(exprArr.ptr)
        {
            auto call = Arena_AllocAndInit(p->arena, Ast_CallExprNode);
            call->token = ident;
            call->args  = exprArr;
            
            auto exprStmt = Arena_AllocAndInit(p->arena, Ast_ExprStmt);
            exprStmt->expr = ParseExpression(p, call);
            
            result = exprStmt;
        }
        else
        {
            auto exprStmt = Arena_AllocAndInit(p->arena, Ast_ExprStmt);
            exprStmt->expr = ParseExpression(p);
            
            result = exprStmt;
        }
    }
    
    Assert(result->type == DeclStmt || result->type == ExprStmt);
    return result;
    
#if 0
    // TODO: this doesn't handle any type specifier yet.
    
    Ast_ExprNode* maybeExpr = ParseExpression(p);
    if(!maybeExpr)
        return 0;
    
    Token type;
    Array<Ast_ExprNode*> typeParams;
    Token ident;
    bool isExpr = false;
    uint8 numPointers = 0;
    // If two identifiers are back to back, then
    // it's immediately clear that it's a declaration
    // *int intPtr = 0;
    if(PeekNextToken(p->tokenizer)->type == TokenIdent)
    {
        ident = *PeekNextToken(p->tokenizer);
        
        // This might be a declaration
        auto tmp = maybeExpr;
        while(tmp)
        {
            // Reached the end of the '*' chain
            // (Could be a polymorphic type)
            if(tmp->type == IdentNode || tmp->type == CallNode)
            {
                type = tmp->token;
                if(tmp->type == CallNode)
                    typeParams = ((Ast_CallExprNode*)tmp)->args;
                
                isExpr = false;
                break;
            }
            
            if(tmp->type != UnaryNode || tmp->token.type != '*')
            {
                isExpr = true;
                break;
            }
            
            // It's a unary post-fix '*', so it could be part
            // of the declaration
            ++numPointers;
            auto unaryTmp = (Ast_UnaryExprNode*)tmp;
            tmp = unaryTmp->expr;
        }
    }
    else
        isExpr = true;
    
    if(isExpr)
    {
        result = Arena_AllocAndInit(p->arena, Ast_ExprStmt);
        auto exprRes = (Ast_ExprStmt*)result;
        exprRes->expr = maybeExpr;
    }
    else
    {
        EatToken(p->tokenizer);  // Eat the var identifier
        
        result = Arena_AllocAndInit(p->arena, Ast_Declaration);
        auto declRes = (Ast_Declaration*)result;
        declRes->typeInfo.ident  = type;
        declRes->nameIdent       = ident;
        declRes->typeInfo.params = typeParams;
        declRes->typeInfo.numPointers = numPointers;
        
        Token maybeEqual = *PeekNextToken(p->tokenizer);
        if(maybeEqual.type == '=')
        {
            EatToken(p->tokenizer);
            
            auto binOp = Arena_AllocAndInit(p->arena, Ast_BinExprNode);
            
            auto binLhs   = Arena_AllocAndInit(p->arena, Ast_ExprNode);
            binLhs->type  = IdentNode;
            binLhs->token = ident;
            
            binOp->token = maybeEqual;
            binOp->left  = binLhs;
            binOp->right = ParseExpression(p);
            
            declRes->initialization = binOp;
        }
    }
    
    Assert(result->type == DeclStmt || result->type == ExprStmt);
    return result;
#endif
}

static TryParseDecl_Ret TryParseExpressionOrDeclaration(Parser* p,
                                                        bool forceInitialization,
                                                        TokenType expected)
{
    ProfileFunc();
    
    Ast_ExprOrDecl* exprOrDecl = 0;
    if(PeekNextToken(p->tokenizer)->type != expected)
    {
        exprOrDecl = ParseExpressionOrDeclaration(p, forceInitialization);
        if(PeekNextToken(p->tokenizer)->type != expected)
        {
            // TODO(Leo): print expected token
            SyntaxError(p, "Unexpected token");
            TryParseDecl_Ret ret = { 0, true };
        }
    }
    
    EatToken(p->tokenizer);   // Eat 'expected'
    return { exprOrDecl, false };
}

static Ast_DeclStmt* ParseDeclarationStatement(Parser* p,
                                               bool forceInitialization)
{
    ProfileFunc();
    
    EatToken(p->tokenizer);  // Eat 'var'
    
    auto block = Arena_AllocAndInit(p->arena, Ast_DeclStmt);
    block->expr = 0;
    
    if(PeekNextToken(p->tokenizer)->type == '*')
    {
        block->isPointer = true;
        EatToken(p->tokenizer);  // Eat '*'
    }
    else
        block->isPointer = false;
    
    Token identifier = *PeekNextToken(p->tokenizer);
    if (identifier.type != TokenIdent)
    {
        SyntaxError(p, "Expected identifier");
        return 0;
    }
    
    block->id = identifier;
    
    EatToken(p->tokenizer);
    Token equal = *PeekNextToken(p->tokenizer);
    
    if (equal.type == '=')
    {
        block->equal = equal;
        EatToken(p->tokenizer);
        block->expr = ParseExpression(p);
        if (!block->expr)
            return 0;
    }
    else if(forceInitialization)
    {
        SyntaxError(p, "Expected '=', variable has to be initialized");
        return 0;
    }
    
    return block;
}

static Ast_ReturnStmt* ParseReturnStatement(Parser* p) 
{
    ProfileFunc();
    
    if (PeekNextToken(p->tokenizer)->type != TokenReturn)
    {
        SyntaxError(p, "Expected 'return'");
        return 0;
    }
    
    EatToken(p->tokenizer);
    
    auto stmt = Arena_AllocAndInit(p->arena, Ast_ReturnStmt);
    
    if (PeekNextToken(p->tokenizer)->type != ';')
        stmt->returnValue = ParseExpression(p);
    else
    {
        SyntaxError(p, "Expected return value");
        return 0;
    }
    
    return stmt;
}

static Ast_BlockStmt* ParseBlockStatement(Parser* p)
{
    ProfileFunc();
    
    if(PeekNextToken(p->tokenizer)->type != '{')
    {
        SyntaxError(p, "Expected '{' for the beginning of the block");
        return 0;
    }
    
    EatToken(p->tokenizer);   // Eat '{'
    
    auto block = Arena_AllocAndInit(p->arena, Ast_BlockStmt);
    block->first = 0;
    
    Ast_BlockNode* prevBlockNode = 0;
    while(PeekNextToken(p->tokenizer)->type != '}')
    {
        auto blockNode = Arena_AllocAndInit(p->arena, Ast_BlockNode);
        blockNode->stmt = ParseStatement(p);
        if(!blockNode->stmt)
            return 0;
        
        if(block->first)
            prevBlockNode->next = blockNode;
        else
            block->first = blockNode;
        
        prevBlockNode = blockNode;
    }
    
    if(PeekNextToken(p->tokenizer)->type != '}')
    {
        SyntaxError(p, "Expected '}' for block ending");
        return 0;
    }
    
    EatToken(p->tokenizer);   // Eat '}'
    
    return block;
}

bool ParseRootNode(Parser *parser, Tokenizer *tokenizer)
{
    ProfileFunc();
    
    auto savepoint = Arena_TempBegin(parser->tempArena);
    defer(Arena_TempEnd(savepoint));
    
    Token* token = PeekNextToken(tokenizer);
    bool endOfFile = false;
    // Top level statements
    while(true)
    {
        bool skipToNext = false;
        bool parseSuccess = false;
        Ast_TopLevelStmt *topLevel = nullptr;
        switch(token->type)
        {
            // Ignore top-level semicolons
            case ';':
            {
                EatToken(tokenizer);
                token = PeekNextToken(tokenizer);
                skipToNext = true;
            }
            break;
            case TokenProc:
            {
                topLevel = ParseDefinition(parser);
                if(topLevel)
                    parseSuccess = true;
            }
            break;
            case TokenExtern:
            {
                topLevel = ParseExtern(parser);
                if(topLevel)
                    parseSuccess = true;
            }
            break;
            case TokenError: { parseSuccess = false; } break;
            case TokenEOF: { endOfFile = true; } break;
            default:
            {
                SyntaxError(*token, parser, "Unexpected token");
                parseSuccess = false;
            } break;
        }
        
        if(skipToNext)
            continue;
        if(endOfFile)
            break;
        if(!parseSuccess || !topLevel)
            return false;
        
        Array_AddElement(&parser->root.topStmts, parser->tempArena, topLevel);
        token = PeekNextToken(tokenizer);    
    }
    
    parser->root.topStmts = Array_CopyToArena(parser->root.topStmts, parser->arena);
    return true;
}

