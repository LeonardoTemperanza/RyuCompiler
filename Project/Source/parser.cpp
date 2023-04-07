
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
    
    while(true)
    {
        Ast_TopLevel* node = 0;
        
        switch(p->at->type)
        {
            case Tok_Proc: node = CastToTopLevel(ParseFunctionDef(p)); break;
            case Tok_Struct: Assert(false); break;
            // If none of those keywords, must be
            // a global variable declaration
            default: node = CastToTopLevel(ParseDeclOrExpr(p)); break;
        }
        
        if(!node || p->error)
            break;
        
        Array_AddElement(&result, p->tempArena, node);
    }
    
    result = Array_CopyToArena(result, p->arena);
    return result;
}

Ast_FunctionDef* ParseFunctionDef(Parser* p)
{
    EatRequiredToken(p, Tok_Proc);
    if(p->error)
        return 0;
    
    auto func = Ast_MakeNode<Ast_FunctionDef>(p->arena, p->at);
    
    
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
    
    EatRequiredToken(p, ';');
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
        //case Tok_If: stmt = ParseIf(p);
        //case Tok_For: stmt = ParseFor(p);
        case '{': stmt = ParseBlock(p);
        default: stmt = CastToStmt(ParseDeclOrExpr(p));
    }
    
    return stmt;
}

Ast_If* ParseIf(Parser* p)
{
    return 0;
}

Ast_Block* ParseBlock(Parser* p)
{
    while(true)
    {
        // Logic here
        
        //if(p->tokenizer->
    }
    
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
        int curPrefixPrec = GetPrefixOpPrec(p->at->type);
        Ast_UnaryExpr* prefixExpr = 0;
        while(curPrefixPrec != -1)
        {
            auto tmp = Ast_MakeNode<Ast_UnaryExpr>(p->arena, p->at);
            tmp->unaryOperator = p->at->type;
            
            if(prefixExpr)
                prefixExpr->expr = tmp;
            
            prefixExpr = tmp;
            ++p->at;
            
            curPrefixPrec = GetPrefixOpPrec(p->at->type);
        }
        
        // Postfix operators are more complex than
        // prefix (function call, subscript, member access...)
        lhs = ParsePostfixExpression(p);
        
        // Postfix ops have precedence over prefix ops
        prefixExpr->expr = lhs;
        lhs = prefixExpr;
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
    
    
    return 0;
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

int GetPrefixOpPrec(TokenType tokType)
{
    return 0;
}

int GetPostfixOpPrec(TokenType tokType)
{
    return 0;
}

bool IsOperatorLToR(TokenType tokType)
{
    return false;
}

void EatRequiredToken(Parser* p, TokenType tokType)
{
    if(p->at->type != tokType)
    {
        CompileError(p->tokenizer, p->at, 3, "Expecting '", TokTypeToString(tokType), "'");
        p->error = true;
    }
    else
        ++p->at;
}

void EatRequiredToken(Parser* p, char tokType)
{
    EatRequiredToken(p, (TokenType)tokType);
}