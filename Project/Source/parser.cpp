
#include "parser.h"

template<typename t>
t* Ast_MakeNode(Arena* arena, Token* token)
{
    // Call the constructor for the node,
    // Pack the nodes for better cache locality
    t* result = (t*)Arena_AllocAndInitAlign(arena, t, alignof(t));
    result->where = token;
    return result;
}

Array<Ast_TopLevel*> ParseFile(Parser* p)
{
    Arena_TempGuard(p->tempArena);
    Array<Ast_TopLevel*> result = { 0, 0 };
    p->at = p->tokenizer->tokens.ptr;
    
    bool quit = false;
    while(!quit)
    {
        Ast_TopLevel* node = 0;
        
        switch(p->at->type)
        {
            case Tok_Proc: node = CastToTopLevel(ParseFunctionDef(p)); break;
            case Tok_Struct: Assert(false); break;
            case Tok_Error:
            case Tok_EOF: quit = true; break;
            // If none of those keywords, must be
            // a global variable declaration
            default: node = CastToTopLevel(ParseDeclOrExpr(p)); break;
        }
        
        if(!quit)
            Array_AddElement(&result, p->tempArena, node);
    }
    
    result = Array_CopyToArena(result, p->arena);
    return result;
}

Ast_FunctionDef* ParseFunctionDef(Parser* p)
{
    EatRequiredToken(p, Tok_Proc);
    
    auto func = Ast_MakeNode<Ast_FunctionDef>(p->arena, p->at);
    
    // Parse function declarator
    
    EatRequiredToken(p, '{');
    
    while(true)
    {
        
    }
    
    EatRequiredToken(p, '}');
    return 0;
}

Ast_Node* ParseDeclOrExpr(Parser* p)
{
    // Determine if it's a declaration or an expression first
    bool isDecl = false;
    bool foundIdent = false;
    for(Token* token = p->at; token->type != Tok_EOF; ++token)
    {
        if(token->type == Tok_Ident && foundIdent)
        {
            isDecl = true;
            break;
        }
        else if(token->type == Tok_Ident)
            foundIdent = true;
        
        if(token->type == '^' || (!foundIdent && token->type == '['))
        {
            isDecl = true;
            break;
        }
    }
    
    Ast_Node* result;
    if(isDecl) result = ParseDeclaration(p);
    else       result = ParseExpression(p);
    
    return result;
}

Ast_Declaration* ParseDeclaration(Parser* p)
{
    auto decl = Ast_MakeNode<Ast_Declaration>(p->arena, p->at);
    //decl->...
    
    ParseType(p);
    
    return 0;
}

Ast_Stmt* ParseStatement(Parser* p)
{
    Ast_Stmt* stmt;
    switch(p->at->type)
    {
        case Tok_If: stmt = ParseIf(p);
        case Tok_For: stmt = ParseFor(p);
        case '{': stmt = ParseBlock(p); break;
        case Tok_Error: return 0;
        default: stmt = CastToStmt(ParseDeclOrExpr(p));
    }
    
    return stmt;
}

Ast_If* ParseIf(Parser* p)
{
    return 0;
}

Ast_For* ParseFor(Parser* p)
{
    return 0;
}

Ast_While* ParseWhile(Parser* p)
{
    return 0;
}

Ast_Defer* ParseDefer(Parser* p)
{
    return 0;
}

Ast_Block* ParseBlock(Parser* p)
{
    EatRequiredToken(p, '{');
    
    while(true)
    {
        // Logic here
        
        //if(p->tokenizer->
    }
    
    EatRequiredToken(p, '}');
    
    return 0;
}

// NOTE: All unary operators always have precedence over
// binary operators. If this is not the case anymore, change
// the implementation of this function a bit.
Ast_Expr* ParseExpression(Parser* p, int prec)
{
    Ast_Expr* lhs = 0;
    
    if(p->at->type == '(')  // Paren/Typecast
    {
        ++p->at;
        
        Token* closeParen = 0;
        for(Token* token = p->at;
            token->type != Tok_EOF && !closeParen;
            ++token)
        {
            if(token->type == ')')
                closeParen = token;
        }
        
        if(closeParen && closeParen[1].type == Tok_Ident)  // Typecast
        {
            // TODO: Parse the type, then parse the next expression
            // and put all that stuff in the "typecast" expression
            p->at = closeParen + 1;
            
            // Allocate typecast node
            
            // ParseType
            
            EatRequiredToken(p, ')');
            
            // ParseExpression
            
        }
        else  // Parenthesis
        {
            lhs = ParseExpression(p);
            EatRequiredToken(p, ')');
        }
    }
    else
    {
        // Prefix operators
        Ast_UnaryExpr* prefixExpr = 0;
        while(IsOpPrefix(p->at->type))
        {
            auto tmp = Ast_MakeNode<Ast_UnaryExpr>(p->arena, p->at);
            tmp->unaryOperator = p->at->type;
            
            if(prefixExpr)
                prefixExpr->expr = tmp;
            
            prefixExpr = tmp;
            ++p->at;
        }
        
        // Postfix operators are more complex than
        // prefix (function call, subscript, member access...)
        lhs = ParsePostfixExpression(p);
        
        // Postfix ops have precedence over prefix ops
        if(prefixExpr)
        {
            prefixExpr->expr = lhs;
            lhs = prefixExpr;
        }
    }
    
    while(true)
    {
        int opPrec = GetOperatorPrec(p->at->type);
        
        bool undoRecurse = false;
        undoRecurse |= opPrec == -1;
        undoRecurse |= opPrec < prec;
        undoRecurse |= (opPrec == prec && IsOperatorLToR(p->at->type));
        if(undoRecurse)
            return lhs;
        
        // Recurse
        auto binOp = Ast_MakeNode<Ast_BinaryExpr>(p->arena, p->at);
        binOp->binaryOperator = p->at->type;
        binOp->lhs = lhs;
        ++p->at;
        
        binOp->rhs = ParseExpression(p, opPrec);
        lhs = binOp;
    }
    
    Assert(false && "Unreachable code");
    return 0;
}

Ast_Expr* ParsePostfixExpression(Parser* p)
{
    Token* ident = EatRequiredToken(p, Tok_Ident);
    Ast_Expr* result = 0;
    
    while(true)
    {
        if(IsOpPostfix(p->at->type))  // Postfix operators
        {
            Token* lastPostfix = p->at;
            for(Token* token = p->at+1; token->type != Tok_EOF; ++token)
            {
                if(!IsOpPostfix(token->type))
                {
                    lastPostfix = token;
                    break;
                }
            }
            
            // Construct the postfix unary expression list
            // You have to look at the operators in reverse order
            Ast_UnaryExpr* unary = Ast_MakeNode<Ast_UnaryExpr>(p->arena, lastPostfix);
            // unary->...
            for(Token* token = lastPostfix; token >= p->at; --token)
            {
                
            }
            
            // Link it to previous expression
            
            p->at = lastPostfix + 1;
        }
        else if(p->at->type == '(')  // Function call
        {
            Ast_FuncCall* call = Ast_MakeNode<Ast_FuncCall>(p->arena, ident);
            if(result)
                call->func = result;
        }
        else if(p->at->type == '[')  // Subscript
        {
            ++p->at;
            
            // TODO: @incomplete
            ParseExpression(p);
            
            EatRequiredToken(p, ']');
        }
        else if(p->at->type == '.')
        {
            ++p->at;
            
            Token* ident = EatRequiredToken(p, Tok_Ident);
            
        }
        // else if...
        // else if...
        else break;
    }
    
    return result;
}

Ast_Expr* ParsePrimaryExpression(Parser* p)
{
    return 0;
}

TypeInfo* ParseType(Parser* p)
{
    return 0;
}

int GetOperatorPrec(TokenType tokType)
{
    return 0;
}

bool IsOpPrefix(TokenType tokType)
{
    return false;
}

bool IsOpPostfix(TokenType tokType)
{
    return false;
}

bool IsOperatorLToR(TokenType tokType)
{
    switch(tokType)
    {
        default: return false;
        
        case '=':
        case Tok_PlusEquals:
        case Tok_MinusEquals:
        case Tok_MulEquals:
        case Tok_DivEquals:
        case Tok_ModEquals:
        case Tok_LShiftEquals:
        case Tok_RShiftEquals:
        case Tok_AndEquals:
        case Tok_XorEquals:
        case Tok_OrEquals:
        return false;
    }
    
    return true;
}

inline Token* EatRequiredToken(Parser* p, TokenType tokType)
{
    if(p->at->type != tokType)
    {
        ExpectedTokenError(p, tokType);
        
        // Change current token to Tok_Error
        // and don't go to the next one
        p->at->type = Tok_Error;
        return p->at;
    }
    
    return p->at++;
}

inline Token* EatRequiredToken(Parser* p, char tokType)
{
    return EatRequiredToken(p, (TokenType)tokType);
}

inline void ExpectedTokenError(Parser* p, TokenType tokType)
{
    CompileError(p->tokenizer, p->at, 3, "Expecting '", TokTypeToString(tokType), "'");
}

inline void ExpectedTokenError(Parser* p, char tokType)
{
    ExpectedTokenError(p, tokType);
}
