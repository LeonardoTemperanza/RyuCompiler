
#include "base.h"
#include "lexer.h"
#include "memory_management.h"
#include "parser.h"

// @architecture Do we want to do a conversion between syntactic declarators
// and typeinfo? You need AST nodes for declarators etc., but you also need that
// for the typeinfo stuff. I can imagine in some cases a reallocation might be
// beneficial, i'm just not sure if I should do it or not. The overall structure
// seems the same to me for both syntax and semantics. Let's just put these in 2 different
// places, in the ast arena initially and then in the 

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
    
    return result;
}

template<typename t>
t Ast_InitNode(Token* token)
{
    t result;
    result.where = token;
    return result;
}

Ast_FileScope* ParseFile(Parser* p)
{
    ProfileFunc(prof);
    ScratchArena scratch;
    
    auto root = Arena_AllocAndInitPack(p->arena, Ast_FileScope);
    //auto root = Ast_MakeNode<Ast_FileScope>(p->arena, p->at);
    root->scope.enclosing = p->globalScope;
    p->scope = &root->scope;
    defer(p->scope = p->scope->enclosing);
    
    Array<Ast_Node*> result = { 0, 0 };
    p->at = p->tokenizer->tokens.ptr;
    
    bool quit = false;
    while(!quit)
    {
        Ast_Node* node = 0;
        
        switch_nocheck(p->at->type)
        {
            case ';':          ++p->at; break;
            case Tok_Proc:     node = ParseProcDef(p); break;
            case Tok_Operator: node = ParseProcDef(p); break;
            case Tok_Struct:   node = ParseStructDef(p); break;
            case Tok_EOF:      quit = true; break;
            // If none of those keywords, must be
            // a global variable declaration
            default:
            {
                node = ParseVarDecl(p);
                EatRequiredToken(p, ';');
                break;
            }
        } switch_nocheck_end;
        
        if(!quit && node)
            result.Append(scratch.arena(), node);
    }
    
    result = result.CopyToArena(p->arena);
    
    root->scope.stmts = result;
    return root;
}

Ast_ProcDef* ParseProcDef(Parser* p)
{
    ProfileFunc(prof);
    ScratchArena scratch;
    
    bool isOperator = p->at->type == Tok_Operator;
    
    if(p->at->type != Tok_Proc && p->at->type != Tok_Operator)
        ExpectedTokenError(p, p->at, Tok_Proc);
    
    auto func = Ast_MakeNode<Ast_ProcDef>(p->arena, p->at);
    
    Token* ident = 0;
    TypeInfo* typeInfo = ParseType(p, &ident);
    if(!isOperator && !IsTokIdent(ident->type))
        ExpectedTokenError(p, ident, Tok_Ident);
    else if(isOperator && !IsTokOperator(ident->type))
        ParseError(p, ident, StrLit("Expecting operator"));
    
    func->type = typeInfo;
    DeferStringInterning(p, ident->ident, &func->name);
    
    Ast_Block block;
    block.stmts = { 0, 0 };
    block.enclosing = p->scope;
    p->scope = &func->block;
    defer(p->scope = p->scope->enclosing);  // Revert to original scope
    
    EatRequiredToken(p, '{');
    
    while(p->at->type != '}' && p->at->type != Tok_EOF)
    {
        Ast_Stmt* stmt = ParseStatement(p);
        block.stmts.Append(scratch.arena(), (Ast_Node*)stmt);
    }
    
    block.stmts = block.stmts.CopyToArena(p->arena);
    
    EatRequiredToken(p, '}');
    
    func->block = block;
    return func;
}

// TODO: Refactor the ast for the declarations.
// Inheritance is not needed and actually poses a problem
Ast_StructDef* ParseStructDef(Parser* p)
{
    ProfileFunc(prof);
    
    if(p->at->type != Tok_Struct)
        ExpectedTokenError(p, p->at, Tok_Struct);
    
    auto structDef = Ast_MakeNode<Ast_StructDef>(p->arena, p->at);
    
    Token* ident = 0;
    TypeInfo* typeInfo = ParseType(p, &ident);
    if(!IsTokIdent(ident->type))
        ExpectedTokenError(p, ident, Tok_Ident);
    
    structDef->type = typeInfo;
    DeferStringInterning(p, ident->ident, &structDef->name);
    return structDef;
}

Ast_Node* ParseDeclOrExpr(Parser* p, bool forceInit)
{
    ProfileFunc(prof);
    
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
        result = ParseVarDecl(p, forceInit);
    else
        result = ParseExpression(p);
    
    return result;
}

Ast_VarDecl* ParseVarDecl(Parser* p, bool forceInit, bool preventInit)
{
    ProfileFunc(prof);
    
    Token* t = 0;
    auto type = ParseType(p, &t);
    if(!IsTokIdent(t->type))
        ExpectedTokenError(p, t, Tok_Ident);
    
    auto decl  = Ast_MakeNode<Ast_VarDecl>(p->arena, t);
    decl->type = type;
    DeferStringInterning(p, t->ident, &decl->name);
    
    if(!preventInit)
    {
        if(forceInit || p->at->type == '=')
        {
            EatRequiredToken(p, '=');
            decl->initExpr = ParseExpression(p);
        }
    }
    
    return decl;
}

Ast_Stmt* ParseStatement(Parser* p)
{
    ProfileFunc(prof);
    
    Ast_Node* stmt = 0;
    switch_nocheck(p->at->type)
    {
        case Tok_EOF:      return 0;
        case Tok_If:       stmt = ParseIf(p); break;
        case Tok_For:      stmt = ParseFor(p); break;
        case Tok_While:    stmt = ParseWhile(p); break;
        case Tok_Do:       stmt = ParseDoWhile(p); break;
        case Tok_Defer:    stmt = ParseDefer(p); break;
        case Tok_Return:   stmt = ParseReturn(p); break;
        case Tok_Break:    stmt = ParseBreak(p); break;
        case Tok_Continue: stmt = ParseContinue(p); break;
        case '{':          stmt = ParseBlock(p); break;
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
    } switch_nocheck_end;
    
    Assert(Ast_IsStmt(stmt));
    return (Ast_Stmt*)stmt;
}

Ast_If* ParseIf(Parser* p)
{
    ProfileFunc(prof);
    
    auto stmt = Ast_MakeNode<Ast_If>(p->arena, p->at);
    
    EatRequiredToken(p, Tok_If);
    
    EatRequiredToken(p, '(');
    stmt->condition = ParseDeclOrExpr(p);
    EatRequiredToken(p, ')');
    
    stmt->thenBlock = ParseOneOrMoreStmtBlock(p);
    
    if(p->at->type == Tok_Else)
    {
        ++p->at;
        stmt->elseStmt = ParseStatement(p);
    }
    
    return stmt;
}

Ast_For* ParseFor(Parser* p)
{
    ProfileFunc(prof);
    
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
    
    stmt->body = ParseOneOrMoreStmtBlock(p);
    return stmt;
}

Ast_While* ParseWhile(Parser* p)
{
    ProfileFunc(prof);
    
    auto stmt = Ast_MakeNode<Ast_While>(p->arena, p->at);
    
    EatRequiredToken(p, Tok_While);
    
    EatRequiredToken(p, '(');
    stmt->condition = ParseDeclOrExpr(p);
    EatRequiredToken(p, ')');
    
    stmt->doBlock = ParseOneOrMoreStmtBlock(p);
    return stmt;
}

Ast_DoWhile* ParseDoWhile(Parser* p)
{
    ProfileFunc(prof);
    
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
    ProfileFunc(prof);
    
    auto stmt = Ast_MakeNode<Ast_Defer>(p->arena, p->at);
    EatRequiredToken(p, Tok_Defer);
    
    stmt->stmt = ParseStatement(p);
    return stmt;
}

Ast_Return* ParseReturn(Parser* p)
{
    ProfileFunc(prof);
    
    auto stmt = Ast_MakeNode<Ast_Return>(p->arena, p->at);
    EatRequiredToken(p, Tok_Return);
    
    if(p->at->type != ';')
        stmt->expr = ParseExpression(p);
    EatRequiredToken(p, ';');
    return stmt;
}

Ast_Break* ParseBreak(Parser* p)
{
    ProfileFunc(prof);
    
    auto stmt = Ast_MakeNode<Ast_Break>(p->arena, p->at);
    EatRequiredToken(p, Tok_Break);
    EatRequiredToken(p, ';');
    return stmt;
}

Ast_Continue* ParseContinue(Parser* p)
{
    ProfileFunc(prof);
    
    auto stmt = Ast_MakeNode<Ast_Continue>(p->arena, p->at);
    EatRequiredToken(p, Tok_Continue);
    EatRequiredToken(p, ';');
    return stmt;
}

Ast_Block* ParseBlock(Parser* p)
{
    ProfileFunc(prof);
    
    ScratchArena scratch;
    
    auto block = Ast_MakeNode<Ast_Block>(p->arena, p->at);
    block->enclosing = p->scope;
    p->scope = block;
    defer(p->scope = p->scope->enclosing);
    
    EatRequiredToken(p, '{');
    
    while(p->at->type != '}' && p->at->type != Tok_EOF)
    {
        Ast_Node* stmt = ParseStatement(p);
        block->stmts.Append(scratch.arena(), stmt);
    }
    
    block->stmts = block->stmts.CopyToArena(p->arena);
    
    EatRequiredToken(p, '}');
    return block;
}

Ast_Block* ParseOneOrMoreStmtBlock(Parser* p)
{
    ProfileFunc(prof);
    
    auto thenBlock = Ast_MakeNode<Ast_Block>(p->arena, p->at);
    thenBlock->enclosing = p->scope;
    p->scope = thenBlock;
    defer(p->scope = p->scope->enclosing);
    
    if(p->at->type == '{')  // Multiple statements
    {
        ++p->at;
        
        ScratchArena scratch;
        while(p->at->type != '}' && p->at->type != Tok_EOF)
        {
            Ast_Node* stmt = ParseStatement(p);
            thenBlock->stmts.Append(scratch.arena(), stmt);
        }
        
        thenBlock->stmts = thenBlock->stmts.CopyToArena(p->arena);
        
        EatRequiredToken(p, '}');
    }
    else  // Only one statement
    {
        thenBlock->stmts = { 0, 1 };
        auto stmtPtr = Arena_AllocVarPack(p->arena, Ast_Node*);
        *stmtPtr = ParseStatement(p);
        thenBlock->stmts.ptr = stmtPtr;
    }
    
    return thenBlock;
}

// NOTE: All unary operators always have precedence over
// binary operators. If this is not the case anymore, change
// the implementation of this procedure a bit.
Ast_Expr* ParseExpression(Parser* p, int prec)
{
    ProfileFunc(prof);
    
    Ast_Expr* lhs = 0;
    
    // Prefix operators (basic linked-list construction)
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
            tmp->type = ParseType(p, &ident);
            EatRequiredToken(p, ')');
            
            *baseExpr = tmp;
            baseExpr = &tmp->expr;
        }
        else  // Other prefix operators
        {
            auto tmp = Ast_MakeNode<Ast_UnaryExpr>(p->arena, p->at);
            tmp->op = p->at->type;
            tmp->isPostfix = false;
            
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
    
    while(true)
    {
        int opPrec = GetOperatorPrec(p->at->type);
        
        bool undoRecurse = false;
        undoRecurse |= (opPrec == -1);
        undoRecurse |= (opPrec > prec);  // Is it's less important (=greater priority) don't recurse
        undoRecurse |= (opPrec == prec && IsOperatorLToR(p->at->type));
        if(undoRecurse)
        {
            /* TODO: test this
            if(p->at->type == '?')  // Ternary operator(s)
            {
                ++p->at;
                
                auto ternary = Ast_MakeNode<Ast_TernaryExpr>(p->arena, p->at);
                ternary->expr1 = lhs;
                ternary->expr2 = ParseExpression(p);
                EatRequiredToken(p, ':');
                ternary->expr3 = ParseExpression(p);
                
                return ternary;
            }
            */
            
            return lhs;
        }
        
        // Recurse
        auto binOp = Ast_MakeNode<Ast_BinaryExpr>(p->arena, p->at);
        binOp->op = p->at->type;
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
    ProfileFunc(prof);
    
    Ast_Expr* curExpr = ParsePrimaryExpression(p);
    
    ScratchArena scratch;
    while(IsOpPostfix(p->at->type))
    {
        scratch.Reset();
        switch_nocheck(p->at->type)
        {
            case '.':  // Member access
            {
                auto memberAccess = Ast_MakeNode<Ast_MemberAccess>(p->arena, p->at);
                ++p->at;
                
                String ident = EatRequiredToken(p, Tok_Ident)->ident;
                DeferStringInterning(p, ident, &memberAccess->memberName);
                memberAccess->target = curExpr;
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
                        call->args.Append(scratch.arena(), argExpr);
                        
                        if(p->at->type == ',') ++p->at;
                        else break;
                    }
                }
                
                call->args = call->args.CopyToArena(p->arena);
                EatRequiredToken(p, ')');
                
                call->target = curExpr;
                curExpr = call;
                
                break;
            }
            case '[':  // Subscript operator
            {
                auto subscript = Ast_MakeNode<Ast_Subscript>(p->arena, p->at);
                ++p->at;
                
                subscript->idxExpr = ParseExpression(p);
                subscript->target = curExpr;
                curExpr = subscript;
                
                EatRequiredToken(p, ']');
                break;
            }
            default:  // The rest of the post-fix unary operators
            {
                auto unary = Ast_MakeNode<Ast_UnaryExpr>(p->arena, p->at);
                unary->op = p->at->type;
                unary->isPostfix = true;
                
                unary->expr = curExpr;
                curExpr = unary;
                ++p->at;
                
                break;
            }
        } switch_nocheck_end;
    }
    
    return curExpr;
}

Ast_Expr* ParsePrimaryExpression(Parser* p)
{
    ProfileFunc(prof);
    ScratchArena scratch;
    
    if(p->at->type == '(')  // Parenthesis expression
    {
        ++p->at;
        auto primary = ParseExpression(p);
        EatRequiredToken(p, ')');
        return primary;
    }
    else if(IsTokIdent(p->at->type))
    {
        auto ident = Ast_MakeNode<Ast_IdentExpr>(p->arena, p->at);
        DeferStringInterning(p, p->at->ident, &ident->ident);
        ++p->at;
        return ident;
    }
    else if(p->at->type == Tok_Num)
    {
        auto literalExpr = Ast_MakeNode<Ast_NumLiteral>(p->arena, p->at);
        literalExpr->type = &primitiveTypes[Typeid_Float];
        ++p->at;
        return literalExpr;
    }
    
    String tokType = TokTypeToString(p->at->type, scratch.arena());
    
    StringBuilder strBuilder;
    
    strBuilder.Append(StrLit("Unexpected '"), scratch.arena());
    strBuilder.Append(tokType, scratch.arena());
    strBuilder.Append(StrLit("', expecting identifier, constant literal or parenthesis expression"), scratch.arena());
    ParseError(p, p->at, strBuilder.string);
    
    return 0;
}

TypeInfo* ParseType(Parser* p, Token** outIdent)
{
    ProfileFunc(prof);
    ScratchArena scratch;
    
    Assert(outIdent);
    *outIdent = 0;
    
    TypeInfo* type = 0;
    TypeInfo** baseType = &type;
    
    bool loop = true;
    while(loop)
    {
        if(IsTokIdent(p->at->type))
        {
            if(p->at->type == Tok_Ident)  // Compound type
            {
                auto tmp = Ast_MakeType<Ast_DeclaratorIdent>(p->arena);
                DeferStringInterning(p, p->at->ident, &tmp->ident);
                
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
        else switch_nocheck(p->at->type)
        {
            case '^':
            {
                auto tmp = Ast_MakeType<Ast_DeclaratorPtr>(p->arena);
                ++p->at;
                
                *baseType = tmp;
                baseType = &tmp->baseType;
                break;
            }
            case '[':
            {
                auto tmp = Ast_MakeType<Ast_DeclaratorArr>(p->arena);
                ++p->at;
                
                *baseType = tmp;
                baseType = &tmp->baseType;
                
                tmp->sizeExpr = ParseExpression(p);
                
                EatRequiredToken(p, ']');
                break;
            }
            case Tok_Proc:
            case Tok_Operator:
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
                if(ident)
                    *outIdent = ident;
                
                *baseType = tmp;
                
                // Struct should be the last type declarator
                loop = false;
                break;
            }
            default: loop = false; break;
        } switch_nocheck_end;
    }
    
    // The identifier is only located after
    // the type if it's not a procedure/procptr/struct
    if(!(*outIdent))
    {
        *outIdent = p->at;
        if(IsTokIdent(p->at->type)) ++p->at;
    }
    
    return type;
}

Ast_DeclaratorProc ParseDeclProc(Parser* p, Token** outIdent, bool forceArgNames)
{
    ProfileFunc(prof);
    
    ScratchArena scratch;
    
    Assert(outIdent);
    *outIdent = 0;
    
    // It's a proc or an operator
    bool isOperator = p->at->type == Tok_Operator;
    
    if(p->at->type == Tok_Proc || p->at->type == Tok_Operator)
        ++p->at;
    else  // This in practice never happens
    {
        String tokType = TokTypeToString(p->at->type, scratch.arena());
        
        StringBuilder strBuilder;
        
        strBuilder.Append(StrLit("Unexpected '"), scratch.arena());
        strBuilder.Append(tokType, scratch.arena());
        strBuilder.Append(StrLit("', expecting 'proc', or 'operator'"), scratch.arena());
        ParseError(p, p->at, strBuilder.string);
    }
    
    // Parse function declaration
    
    *outIdent = p->at;  // It's set to the identifier or the operator if it's an operator declaration
    
    if(!isOperator && IsTokIdent(p->at->type))
        ++p->at;
    else if(isOperator && IsTokOperator(p->at->type))
        ++p->at;
    
    if(IsTokIdent(p->at->type)) ++p->at;
    
    EatRequiredToken(p, '(');
    
    Ast_DeclaratorProc decl;
    decl.isOperator = isOperator;
    
    // Parse arguments
    if(p->at->type != ')')
    {
        Array<Ast_Declaration*> args = { 0, 0 };
        while(true)
        {
            auto argDecl = ParseVarDecl(p, false, true);
            args.Append(scratch, argDecl);
            
            if(p->at->type == ',') ++p->at;
            else break;
        }
        
        decl.args = args.CopyToArena(p->arena);
    }
    
    scratch.Reset();
    EatRequiredToken(p, ')');
    
    // Parse return types
    if(p->at->type == Tok_Arrow)  // There is at least one return type
    {
        ++p->at;
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
                
                decl.retTypes.Append(scratch.arena(), type);
                
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
    ProfileFunc(prof);
    
    Assert(outIdent);
    *outIdent = 0;
    
    EatRequiredToken(p, Tok_Struct);
    
    Ast_DeclaratorStruct decl;
    
    *outIdent = p->at;
    if(IsTokIdent(p->at->type)) ++p->at;
    
    EatRequiredToken(p, '{');
    
    if(p->at->type == '}')
    {
        ++p->at;
        return decl;
    }
    
    // Parse arguments
    ScratchArena typeScratch(0);
    ScratchArena tokenScratch(1);
    
    Array<TypeInfo*> types = { 0, 0 };
    Array<Token*> tokens = { 0, 0 };
    
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
        
        types.Append(typeScratch.arena(), type);
        tokens.Append(tokenScratch.arena(), argName);
        
        EatRequiredToken(p, ';');
    }
    
    decl.memberTypes      = types.CopyToArena(p->arena);
    decl.memberNameTokens = tokens.CopyToArena(p->arena);
    
    decl.memberNames.ptr = Arena_AllocArrayPack(p->arena, tokens.length, Atom*);
    decl.memberNames.length = tokens.length;
    
    // Keep track of atom pointers so that they can later be patched
    // with the correct value (during string interning, which is done
    // on one thread for simplicity)
    for_array(i, decl.memberNames)
        DeferStringInterning(p, decl.memberNameTokens[i]->ident, &decl.memberNames[i]);
    
    EatRequiredToken(p, '}');
    return decl;
}

void DeferStringInterning(Parser* p, String string, Atom** atom)
{
    ToIntern toIntern = { string, atom };
    p->internArray.Append(p->internArena, toIntern);
}

// A table could be used for this information
// These are binary operators only
int GetOperatorPrec(TokenType tokType)
{
    ProfileFunc(prof);
    
    switch_nocheck(tokType)
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
    } switch_nocheck_end;
    
    Assert(false && "Unreachable code");
    return -1;
}

bool IsOpPrefix(TokenType tokType)
{
    ProfileFunc(prof);
    
    switch_nocheck(tokType)
    {
        default: return false;
        
        case Tok_Increment:
        case Tok_Decrement:
        case '+': case '-':
        case '!': case '~':
        case '*': case '&':
        case Tok_Cast:
        return true;
    } switch_nocheck_end;
    
    Assert(false && "Unreachable code");
    return false;
}

// TODO: remove '(' and other stuff?
inline bool IsOpPostfix(TokenType tokType)
{
    ProfileFunc(prof);
    
    switch_nocheck(tokType)
    {
        default: return false;
        
        case Tok_Increment:
        case Tok_Decrement:
        case '(':
        case '[':
        case '.':
        return true;
    } switch_nocheck_end;
    
    Assert(false && "Unreachable code");
    return false;
}

inline bool IsTokOperator(TokenType tokType)
{
    return GetOperatorPrec(tokType) != -1 || IsOpPostfix(tokType) || IsOpPrefix(tokType);
}

bool IsOperatorLToR(TokenType tokType)
{
    ProfileFunc(prof);
    
    switch_nocheck(tokType)
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
    } switch_nocheck_end;
    
    Assert(false && "Unreachable code");
    return true;
}

// TODO: This is pretty bad, if closeParen is missing then the
// whole file is scanned. (not that big of a deal for now, compilation
// will be way faster with syntax errors anyway)
Token* SkipParens(Token* start, char openParen, char closeParen)
{
    ProfileFunc(prof);
    
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
        String tokTypeStr = TokTypeToString(tokType, scratch.arena());
        
        StringBuilder strBuilder;
        strBuilder.Append(scratch.arena(), 3, StrLit("Expecting '"), tokTypeStr, StrLit("'"));
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
