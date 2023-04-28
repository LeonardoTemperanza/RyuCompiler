
#include "parser.h"

// TODO: the operator type is still not being inserted in the expression node

template<typename t>
t* Ast_MakeNode(Arena* arena, Token* token)
{
    // Call the constructor for the node,
    // Pack the nodes for better cache locality
    t* result = Arena_AllocAndInitPack(arena, t);
    result->where = token;
    return result;
}

Ast_Node* Ast_MakeNode(Arena* arena, Token* token, Ast_NodeKind kind)
{
    // Pack the nodes for better cache locality
    auto result = Arena_AllocAndInitPack(arena, Ast_Node);
    result->kind  = kind;
    result->where = token;
    //result->tokenHotData = token->tokenHotData;
    
    return 0;
}

template<typename t>
t Ast_InitNode(Token* token)
{
    t result;
    result.where = token;
    return result;
}

Ast_Block* ParseFile(Parser* p)
{
    ScratchArena scratch;
    
    auto root = Ast_MakeNode<Ast_Block>(p->arena, p->at);
    root->enclosing = p->globalScope;
    p->scope = root;
    defer(p->scope = p->scope->enclosing);
    
    Array<Ast_Node*> result = { 0, 0 };
    p->at = p->tokenizer->tokens.ptr;
    
    bool quit = false;
    while(!quit)
    {
        Ast_Node* node = 0;
        
        switch(p->at->type)
        {
            case ';':        ++p->at; break;
            case Tok_Proc:   node = ParseFunctionDef(p); break;
            case Tok_Struct: node = ParseStructDef(p); break;
            case Tok_EOF:    quit = true; break;
            // If none of those keywords, must be
            // a global variable declaration
            default:
            {
                auto decl = ParseDeclaration(p);
                node = Arena_FromStackPack(p->arena, decl);
                EatRequiredToken(p, ';');
                break;
            }
        }
        
        Assert(Ast_IsTopLevel(node));
        
        if(!quit && node)
            result.AddElement(scratch.arena, node);
    }
    
    result = result.CopyToArena(p->arena);
    
    root->stmts = result;
    return root;
}

Ast_FunctionDef* ParseFunctionDef(Parser* p)
{
    ScratchArena scratch;
    
    auto func = Ast_MakeNode<Ast_FunctionDef>(p->arena, p->at);
    func->decl = ParseDeclaration(p, false, true);
    
    Ast_Block block;
    block.stmts = { 0, 0 };
    block.enclosing = p->scope;
    p->scope = &func->block;
    defer(p->scope = p->scope->enclosing);  // Revert to original scope
    
    EatRequiredToken(p, '{');
    
    while(p->at->type != '}' && p->at->type != Tok_EOF)
    {
        Ast_Stmt* stmt = ParseStatement(p);
        block.stmts.AddElement(scratch.arena, (Ast_Node*)stmt);
    }
    
    block.stmts = block.stmts.CopyToArena(p->arena);
    
    EatRequiredToken(p, '}');
    
    func->block = block;
    return func;
}

Ast_StructDef* ParseStructDef(Parser* p)
{
    auto structDef = Ast_MakeNode<Ast_StructDef>(p->arena, p->at);
    
    Token* ident = 0;
    auto decl = ParseDeclStruct(p, &ident);
    if(!IsTokIdent(ident->type))
        ExpectedTokenError(p, ident, Tok_Ident);
    
    structDef->decl = decl;
    structDef->name = ident->ident;
    
    return structDef;
}

Ast_Node* ParseDeclOrExpr(Parser* p, bool forceInit)
{
    // Determine if it's a declaration or an expression first
    bool isDecl = false;
    
    if(IsTokStartOfDecl(p->at->type))
        isDecl = true;
    else
    {
        bool foundIdent = false;
        for(Token* token = p->at; token->type != Tok_EOF; ++token)
        {
            if(foundIdent && IsTokIdent(token->type))
            {
                isDecl = true;
                break;
            }
            else if(IsTokIdent(token->type))
            {
                foundIdent = true;
                // There could be polymorphic parameters here.
                token = SkipParens(token + 1, '(', ')') - 1;
            }
            else break;
        }
    }
    
    Ast_Node* result;
    if(isDecl)
    {
        auto decl = ParseDeclaration(p, forceInit);
        result = Arena_FromStackPack(p->arena, decl);
    }
    else
        result = ParseExpression(p);
    
    return result;
}

Ast_Declaration ParseDeclaration(Parser* p, bool forceInit, bool preventInit)
{
    Token* t = 0;
    auto typeInfo = ParseType(p, &t);
    if(!IsTokIdent(t->type))
        ExpectedTokenError(p, t, Tok_Ident);
    
    auto decl     = Ast_InitNode<Ast_Declaration>(t);
    decl.name     = t->ident;
    decl.typeInfo = typeInfo;
    
    if(!preventInit)
    {
        if(forceInit || p->at->type == '=')
        {
            EatRequiredToken(p, '=');
            decl.initExpr = ParseExpression(p);
        }
    }
    
    return decl;
}

Ast_Stmt* ParseStatement(Parser* p)
{
    Ast_Node* stmt = 0;
    switch(p->at->type)
    {
        case Tok_EOF:    return 0;
        case Tok_If:     stmt = ParseIf(p); break;
        case Tok_For:    stmt = ParseFor(p); break;
        case Tok_While:  stmt = ParseWhile(p); break;
        case Tok_Do:     stmt = ParseDoWhile(p); break;
        case Tok_Defer:  stmt = ParseDefer(p); break;
        case Tok_Return: stmt = ParseReturn(p); break;
        case '{':        stmt = ParseBlock(p); break;
        case ';':
        {
            stmt = Ast_MakeNode(p->arena, p->at, AstKind_EmptyStmt);
            ++p->at;
            break;
        }
        default:
        {
            stmt = ParseDeclOrExpr(p);
            EatRequiredToken(p, ';');
            break;
        }
    }
    
    Assert(Ast_IsStmt(stmt));
    return (Ast_Stmt*)stmt;
}

Ast_If* ParseIf(Parser* p)
{
    auto stmt = Ast_MakeNode<Ast_If>(p->arena, p->at);
    
    EatRequiredToken(p, Tok_If);
    
    EatRequiredToken(p, '(');
    stmt->condition = ParseDeclOrExpr(p);
    EatRequiredToken(p, ')');
    
    stmt->thenStmt = ParseStatement(p);
    
    if(p->at->type == Tok_Else)
    {
        ++p->at;
        stmt->elseStmt = ParseStatement(p);
    }
    
    return stmt;
}

Ast_For* ParseFor(Parser* p)
{
    auto stmt = Ast_MakeNode<Ast_For>(p->arena, p->at);
    EatRequiredToken(p, Tok_For);
    
    EatRequiredToken(p, '(');
    if(p->at->type != ';')
        stmt->initialization = ParseDeclOrExpr(p);
    
    EatRequiredToken(p, ';');
    
    if(p->at->type != ';')
        stmt->condition = ParseDeclOrExpr(p, true);
    
    EatRequiredToken(p, ';');
    
    if(p->at->type != ')')
        stmt->update = ParseExpression(p);
    
    EatRequiredToken(p, ')');
    
    stmt->body = ParseStatement(p);
    return stmt;
}

Ast_While* ParseWhile(Parser* p)
{
    auto stmt = Ast_MakeNode<Ast_While>(p->arena, p->at);
    
    EatRequiredToken(p, Tok_While);
    
    EatRequiredToken(p, '(');
    stmt->condition = ParseDeclOrExpr(p);
    EatRequiredToken(p, ')');
    
    stmt->doStmt = ParseStatement(p);
    return stmt;
}

Ast_DoWhile* ParseDoWhile(Parser* p)
{
    auto stmt = Ast_MakeNode<Ast_DoWhile>(p->arena, p->at);
    
    EatRequiredToken(p, Tok_Do);
    stmt->doStmt = ParseStatement(p);
    
    EatRequiredToken(p, Tok_While);
    EatRequiredToken(p, '(');
    stmt->condition = ParseExpression(p);
    EatRequiredToken(p, ')');
    
    EatRequiredToken(p, ';');
    return stmt;
}

Ast_Defer* ParseDefer(Parser* p)
{
    auto stmt = Ast_MakeNode<Ast_Defer>(p->arena, p->at);
    EatRequiredToken(p, Tok_Defer);
    
    stmt->stmt = ParseStatement(p);
    return stmt;
}

Ast_Return* ParseReturn(Parser* p)
{
    auto stmt = Ast_MakeNode<Ast_Return>(p->arena, p->at);
    EatRequiredToken(p, Tok_Return);
    
    stmt->expr = ParseExpression(p);
    EatRequiredToken(p, ';');
    return stmt;
}

Ast_Block* ParseBlock(Parser* p)
{
    ScratchArena scratch;
    
    auto block = Ast_MakeNode<Ast_Block>(p->arena, p->at);
    block->enclosing = p->scope;
    p->scope = block;
    defer(p->scope = p->scope->enclosing);
    
    EatRequiredToken(p, '{');
    
    while(p->at->type != '}' && p->at->type != Tok_EOF)
    {
        Ast_Node* stmt = ParseStatement(p);
        block->stmts.AddElement(scratch.arena, stmt);
    }
    
    block->stmts = block->stmts.CopyToArena(p->arena);
    
    EatRequiredToken(p, '}');
    return block;
}

// NOTE: All unary operators always have precedence over
// binary operators. If this is not the case anymore, change
// the implementation of this procedure a bit.
Ast_Expr* ParseExpression(Parser* p, int prec)
{
    Ast_Expr* lhs = 0;
    
    if(p->at->type == '(')  // Parenthesis
    {
        ++p->at;
        lhs = ParseExpression(p);
        EatRequiredToken(p, ')');
    }
    else
    {
        // Prefix operators (basic linked list construction)
        Ast_Expr* prefixExpr = 0;
        Ast_Expr** baseExpr = &prefixExpr;
        while(IsOpPrefix(p->at->type))
        {
            if(p->at->type == Tok_Cast)  // Typecast
            {
                auto tmp = Ast_MakeNode<Ast_Typecast>(p->arena, p->at);
                ++p->at;
                
                EatRequiredToken(p, '(');
                Token* ident = 0;
                tmp->typeInfo = ParseType(p, &ident);
                EatRequiredToken(p, ')');
                
                *baseExpr = tmp;
                baseExpr = &tmp->expr;
            }
            else  // Other prefix operators
            {
                auto tmp = Ast_MakeNode<Ast_UnaryExpr>(p->arena, p->at);
                tmp->unaryOperator = p->at->type;
                
                *baseExpr = tmp;
                baseExpr = &tmp->expr;
                
                ++p->at;
            }
        }
        
        // Postfix operators are more complex than
        // prefix (function call, subscript, member access...)
        lhs = ParsePostfixExpression(p);
        
        // Postfix ops have precedence over prefix ops
        if(prefixExpr)
        {
            *baseExpr = lhs;
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
    Ast_Expr* curExpr = ParsePrimaryExpression(p);
    
    ScratchArena scratch;
    while(IsOpPostfix(p->at->type))
    {
        scratch.Reset();
        switch(p->at->type)
        {
            case '.':  // Member access
            {
                auto memberAccess = Ast_MakeNode<Ast_MemberAccess>(p->arena, p->at);
                ++p->at;
                
                memberAccess->memberName = EatRequiredToken(p, Tok_Ident)->ident;
                memberAccess->toAccess   = curExpr;
                curExpr = memberAccess;
                
                break;
            }
            case '(':  // Function call
            {
                auto call = Ast_MakeNode<Ast_FuncCall>(p->arena, p->at);
                ++p->at;
                
                if(p->at->type != ')')
                {
                    // Parse arguments
                    while(true)
                    {
                        auto argExpr = ParseExpression(p);
                        call->args.AddElement(scratch.arena, argExpr);
                        
                        if(p->at->type == ',') ++p->at;
                        else break;
                    }
                }
                
                call->args = call->args.CopyToArena(p->arena);
                EatRequiredToken(p, ')');
                
                call->func = curExpr;
                curExpr = call;
                
                break;
            }
            case '[':  // Subscript operator
            {
                auto subscript = Ast_MakeNode<Ast_Subscript>(p->arena, p->at);
                ++p->at;
                
                subscript->idxExpr = ParseExpression(p);
                subscript->array = curExpr;
                curExpr = subscript;
                
                EatRequiredToken(p, ']');
                
                break;
            }
            default:  // The rest of the post-fix unary operators
            {
                auto unary = Ast_MakeNode<Ast_UnaryExpr>(p->arena, p->at);
                // TODO: Fill in the unary operator!!
                
                unary->expr = curExpr;
                curExpr = unary;
                ++p->at;
                
                break;
            }
        }
    }
    
    return curExpr;
}

Ast_Expr* ParsePrimaryExpression(Parser* p)
{
    ProfileFunc(prof);
    
    if(IsTokIdent(p->at->type))
    {
        auto ident = Ast_MakeNode<Ast_IdentExpr>(p->arena, p->at);
        ++p->at;
        return ident;
    }
    else if(p->at->type == Tok_Num)
    {
        auto literalExpr = Ast_MakeNode<Ast_NumLiteral>(p->arena, p->at);
        literalExpr->typeInfo = &primitiveTypes[Typeid_Float];
        ++p->at;
        return literalExpr;
    }
    
    ScratchArena scratch;
    String tokType = TokTypeToString(p->at->type, scratch.arena);
    
    StringBuilder strBuilder;
    
    strBuilder.Append(StrLit("Unexpected '"), scratch.arena);
    strBuilder.Append(tokType, scratch.arena);
    strBuilder.Append(StrLit("', expecting identifier or constant literal"), scratch.arena);
    ParseError(p, p->at, strBuilder.string);
    
    return 0;
}

TypeInfo* ParseType(Parser* p, Token** outIdent)
{
    ProfileFunc(prof);
    Assert(outIdent);
    *outIdent = 0;
    
    ScratchArena scratch;
    
    TypeInfo* typeInfo = 0;
    TypeInfo** baseType = &typeInfo;
    
    bool loop = true;
    while(loop)
    {
        if(IsTokIdent(p->at->type))
        {
            if(p->at->type == Tok_Ident)  // Compound type
            {
                auto tmp = Ast_MakeNode<Ast_DeclaratorIdent>(p->arena, p->at);
                tmp->ident = p->at->ident;
                *baseType = tmp;
                
                // TODO: polymorphic parameters
            }
            else  // Primitive type
            {
                *baseType = primitiveTypes + (p->at->type - Tok_IdentBegin);
            }
            
            ++p->at;
            
            // Ident should be the last type declarator
            loop = false;
        }
        else switch(p->at->type)
        {
            case '^':
            {
                auto tmp = Ast_MakeNode<Ast_DeclaratorPtr>(p->arena, p->at);
                ++p->at;
                
                *baseType = tmp;
                baseType = &tmp->baseType;
                break;
            }
            case '[':
            {
                auto tmp = Ast_MakeNode<Ast_DeclaratorArr>(p->arena, p->at);
                ++p->at;
                
                *baseType = tmp;
                baseType = &tmp->baseType;
                
                tmp->sizeExpr = ParseExpression(p);
                
                EatRequiredToken(p, ']');
                break;
            }
            case Tok_Proc:
            {
                Token* ident = 0;
                Ast_DeclaratorProc decl = ParseDeclProc(p, &ident, false);
                auto tmp = Arena_FromStackPack(p->arena, decl);
                if(ident)
                    *outIdent = ident;
                
                *baseType = tmp;
                
                // Proc should be the last type declarator
                loop = false;
                break;
            }
            case Tok_Struct:
            {
                Token* ident = 0;
                Ast_DeclaratorStruct decl = ParseDeclStruct(p, &ident);
                auto tmp = Arena_FromStackPack(p->arena, decl);
                
                *baseType = tmp;
                
                // Struct should be the last type declarator
                loop = false;
                break;
            }
            default: loop = false; break;
        }
    }
    
    // The identifier is only located after
    // the type if it's not a procedure/procptr
    if(!(*outIdent))
        *outIdent = p->at;
    
    if(IsTokIdent(p->at->type)) ++p->at;
    
    return typeInfo;
}

Ast_DeclaratorProc ParseDeclProc(Parser* p, Token** outIdent, bool forceArgNames)
{
    Assert(outIdent);
    *outIdent = 0;
    
    EatRequiredToken(p, Tok_Proc);
    
    // Parse function declaration
    *outIdent = p->at;
    if(IsTokIdent(p->at->type)) ++p->at;
    
    EatRequiredToken(p, '(');
    
    ScratchArena typeScratch;
    ScratchArena nameScratch(typeScratch);
    Ast_DeclaratorProc decl = Ast_InitNode<Ast_DeclaratorProc>(p->at);
    decl.name     = { 0, 0 };
    decl.argTypes = { 0, 0 };
    decl.argNames = { 0, 0 };
    decl.retTypes = { 0, 0 };
    
    // Parse arguments
    if(p->at->type != ')')
    {
        Array<TypeInfo*> types = { 0, 0 };
        Array<Token*> names = { 0, 0 };
        while(true)
        {
            Token* argName = 0;
            TypeInfo* type = ParseType(p, &argName);
            if(forceArgNames && !IsTokIdent(argName->type))
                ExpectedTokenError(p, argName, Tok_Ident);
            
            types.AddElement(typeScratch, type);
            
            if(IsTokIdent(argName->type))
                names.AddElement(nameScratch, argName);
            else
                names.AddElement(nameScratch, 0);
            
            if(p->at->type == ',') ++p->at;
            else break;
        }
        
        types = types.CopyToArena(p->arena);
        names = names.CopyToArena(p->arena);
    }
    
    typeScratch.Reset();
    EatRequiredToken(p, ')');
    
    // Parse return types
    if(p->at->type == Tok_Arrow)  // There is at least one return type
    {
        ++p->at;
        bool multipleRetValues = false;
        if(p->at->type != '(')
        {
            Token* token = 0;
            TypeInfo* type = ParseType(p, &token);
            
            decl.retTypes.length = 1;
            decl.retTypes.ptr = Arena_FromStackPack(p->arena, type);
        }
        else
        {
            ++p->at;
            
            while(true)
            {
                Token* token = 0;
                TypeInfo* type = ParseType(p, &token);
                
                decl.retTypes.AddElement(typeScratch, type);
                
                if(p->at->type == ',') ++p->at;
                else break;
            }
            
            decl.retTypes = decl.retTypes.CopyToArena(p->arena);
            EatRequiredToken(p, ')');
        }
    }
    
    return decl;
}

Ast_DeclaratorStruct ParseDeclStruct(Parser* p, Token** outIdent)
{
    ScratchArena scratch;
    
    *outIdent = 0;
    
    EatRequiredToken(p, Tok_Struct);
    
    Ast_DeclaratorStruct decl = Ast_InitNode<Ast_DeclaratorStruct>(p->at);
    
    *outIdent = p->at;
    if(IsTokIdent(p->at->type)) ++p->at;
    
    EatRequiredToken(p, '{');
    
    if(p->at->type == '}')
    {
        ++p->at;
        return decl;
    }
    
    // Parse arguments
    struct Member
    {
        TypeInfo* type;
        Token* name;
    };
    Array<Member> members = { 0, 0 };
    while(p->at->type != '}' && p->at->type != Tok_EOF)
    {
        if(p->at->type == ';')
        {
            ++p->at;
            continue;
        }
        
        Token* argName = 0;
        TypeInfo* type = ParseType(p, &argName);
        if(!IsTokIdent(argName->type))
            ExpectedTokenError(p, argName, Tok_Ident);
        
        Member member = { type, argName };
        members.AddElement(scratch.arena, member);
        
        EatRequiredToken(p, ';');
    }
    
    for(int i = 0; i < members.length; ++i)
        decl.memberTypes.AddElement(p->arena, members[i].type);
    for(int i = 0; i < members.length; ++i)
        decl.memberNames.AddElement(p->arena, members[i].name);
    
    EatRequiredToken(p, '}');
    return decl;
}

// These are binary operators only
int GetOperatorPrec(TokenType tokType)
{
    switch(tokType)
    {
        case '*': case '/':
        case '%':
        return 3;
        
        case '+': case '-':
        return 4;
        
        case Tok_LShift:
        case Tok_RShift:
        return 5;
        
        case '<': case '>':
        case Tok_LE:
        case Tok_GE:
        return 6;
        
        case Tok_EQ:
        case Tok_NEQ:
        return 7;
        
        case '&':
        return 8;
        
        case '^':
        return 9;
        
        case '|':
        return 10;
        
        case Tok_And:
        return 11;
        
        case Tok_Or:
        return 12;
        
        // ? : Ternary operator here? How to do that?
        
        case '=':
        case Tok_PlusEquals:
        case Tok_MinusEquals:
        case Tok_MulEquals:
        case Tok_DivEquals:
        case Tok_ModEquals:
        case Tok_LShiftEquals:
        case Tok_RShiftEquals: 
        case Tok_AndEquals:
        case Tok_OrEquals:
        case Tok_XorEquals:
        return 14;
        
        default: return -1;
    }
    
    Assert(false && "Unreachable code");
    return -1;
}

bool IsOpPrefix(TokenType tokType)
{
    switch(tokType)
    {
        default: return false;
        
        case Tok_Increment:
        case Tok_Decrement:
        case '+': case '-':
        case '!': case '~':
        case '*': case '&':
        case Tok_Cast:
        return true;
    }
    
    Assert(false && "Unreachable code");
    return false;
}

// TODO: remove '(' and other stuff?
bool IsOpPostfix(TokenType tokType)
{
    switch(tokType)
    {
        default: return false;
        
        case Tok_Increment:
        case Tok_Decrement:
        case '(':
        case '[':
        case '.':
        return true;
    }
    
    Assert(false && "Unreachable code");
    return false;
}

bool IsOperatorLToR(TokenType tokType)
{
    switch(tokType)
    {
        default: return true;
        
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
    
    Assert(false && "Unreachable code");
    return true;
}

// TODO: this is pretty bad, if closeParen is missing then the
// whole file is scanned.
Token* SkipParens(Token* start, char openParen, char closeParen)
{
    int parenLevel = 0;
    if(start->type == openParen)
        ++parenLevel;
    
    Token* result = start;
    while(parenLevel > 0 && result->type != Tok_EOF)
    {
        ++result;
        
        if(result->type == openParen)
            ++parenLevel;
        else if(result->type == closeParen)
            --parenLevel;
    }
    
    return result;
}

inline void ParseError(Parser* p, Token* token, String message)
{
    CompileError(p->tokenizer, token, message);
    
    p->at->type = Tok_EOF;
}

inline Token* EatRequiredToken(Parser* p, TokenType tokType)
{
    if(p->at->type != tokType && !(IsTokIdent(p->at->type) && IsTokIdent(tokType)))
    {
        ExpectedTokenError(p, tokType);
        return p->at;
    }
    
    return p->at++;
}

inline Token* EatRequiredToken(Parser* p, char tokType)
{
    return EatRequiredToken(p, (TokenType)tokType);
}

inline void ExpectedTokenError(Parser* p, Token* tok, TokenType tokType)
{
    if(IsTokIdent(tokType))
        ParseError(p, tok, StrLit("Expecting identifier"));
    else
    {
        ScratchArena scratch;
        String tokTypeStr = TokTypeToString(tokType, scratch.arena);
        
        StringBuilder strBuilder;
        strBuilder.Append(StrLit("Expecting '"), scratch);
        strBuilder.Append(tokTypeStr, scratch);
        strBuilder.Append(StrLit("'"), scratch);
        ParseError(p, tok, strBuilder.string);
    }
}

inline void ExpectedTokenError(Parser* p, TokenType tokType)
{
    ExpectedTokenError(p, p->at, tokType);
}

inline void ExpectedTokenError(Parser* p, Token* tok, char tokType)
{
    ExpectedTokenError(p, tok, (TokenType)tokType);
}

inline void ExpectedTokenError(Parser* p, char tokType)
{
    ExpectedTokenError(p, tokType);
}