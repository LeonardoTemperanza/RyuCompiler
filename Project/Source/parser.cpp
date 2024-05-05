
#include "base.h"
#include "lexer.h"
#include "memory_management.h"
#include "dependency_graph.h"
#include "parser.h"

// @architecture Do we want to do a conversion between syntactic declarators
// and typeinfo?
// update: it seems that it is not needed, the structure is pretty much the same
// anyways

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
    auto result   = Arena_AllocAndInitPack(arena, Ast_Node);
    result->kind  = kind;
    result->where = token;
    
    return result;
}

template<typename t>
t* Ast_MakeEntityNode(Parser* p, Token* token)
{
    auto result   = Arena_AllocAndInitPack(p->arena, t);
    result->where = token;
    
    Dg_NewNode(result, p->entityArena, &p->entities);
    return result;
}

template<typename t>
t Ast_InitNode(Token* token)
{
    t result;
    result.where = token;
    return result;
}

void AddDeclToScope(Ast_Block* block, Ast_Declaration* decl)
{
    if(block->decls.length + 1 >= Ast_Block::maxArraySize && !(block->flags & Block_UseHashTable))
    {
        block->flags |= Block_UseHashTable;
        
        // maxArraySize * 2 would cause it to immediately reach max load factor so
        // it's multiplied by 4
        block->declsTable.Init(Ast_Block::maxArraySize * 4);
        
        for_array(i, block->decls)
        {
            block->declsTable.Add(block->decls[i]->name.hash, block->decls[i]);
        }
        
        block->declsTable.Add(decl->name.hash, decl);
    }
    else if(block->flags & Block_UseHashTable)
    {
        block->declsTable.Add(decl->name.hash, decl);
    }
    
    block->decls.Append(decl);
}

Ast_FileScope* ParseFile(Parser* p)
{
    ProfileFunc(prof);
    ScratchArena scratch;
    
    auto root = Arena_AllocAndInitPack(p->arena, Ast_FileScope);
    root->scope.enclosing = 0;
    p->fileScope = &root->scope;
    p->scope = p->fileScope;
    defer(p->scope = p->scope->enclosing);
    
    Slice<Ast_Node*> result = { 0, 0 };
    p->at = p->tokenizer->tokens.ptr;
    
    bool quit = false;
    while(!quit)
    {
        Ast_Node* node = 0;
        
        Ast_DeclSpec declSpecs = ParseDeclSpecs(p);
        
        switch_nocheck(p->at->kind)
        {
            case ';':          ++p->at; break;
            case Tok_Proc:     node = ParseProc(p, declSpecs); break;
            case Tok_Operator: node = ParseProc(p, declSpecs); break;
            case Tok_Struct:   node = ParseStructDef(p, declSpecs); break;
            case Tok_EOF:      quit = true; break;
            // If none of those keywords, must be
            // a global variable declaration
            default:
            {
                node = ParseVarDecl(p, declSpecs);
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

Ast_ProcDecl* ParseProc(Parser* p, Ast_DeclSpec specs)
{
    ProfileFunc(prof);
    ScratchArena scratch;
    
    bool isOperator = p->at->kind == Tok_Operator;
    
    if(p->at->kind != Tok_Proc && p->at->kind != Tok_Operator)
        ExpectedTokenError(p, p->at, Tok_Proc);
    
    auto procDecl = Ast_MakeEntityNode<Ast_ProcDecl>(p, p->at);
    procDecl->declSpecs = specs;
    
    Token* ident = 0;
    TypeInfo* typeInfo = ParseType(p, &ident);
    Assert(typeInfo->typeId == Typeid_Proc);
    if(!isOperator && !IsTokIdent(ident->kind))
        ExpectedTokenError(p, ident, Tok_Ident);
    else if(isOperator && !IsTokOperator(ident->kind))
        ParseError(p, ident, StrLit("Expecting operator"));
    
    procDecl->where = ident;
    procDecl->type = typeInfo;
    procDecl->name = procDecl->where->ident;
    AddDeclToScope(p->scope, procDecl);
    
    if(p->at->kind == ';')
    {
        procDecl->declSpecs |= Decl_Extern;
        ++p->at;
    }
    else
    {
        auto procDef = Ast_MakeEntityNode<Ast_ProcDef>(p, procDecl->where);
        procDef->decl = procDecl;
        auto procType = (Ast_ProcType*)typeInfo;
        
        // Special error for this token
        bool hasMultipleReturns = ((Ast_ProcType*)typeInfo)->retTypes.length > 1;
        if(!hasMultipleReturns && p->at->kind == ',')
            ParseError(p, p->at, StrLit("Multiple return values should be wrapped with '()' parentheses."));
        else
            EatRequiredToken(p, '{');
        
        // Set scope
        Ast_Block& block = procDef->block;
        block.stmts = { 0, 0 };
        block.enclosing = p->scope;
        p->scope = &procDef->block;
        defer(p->scope = p->scope->enclosing);  // Revert to original scope
        
        // NOTE(Leo): Adding arguments as declarations in the procedure body
        for_array(i, procType->args)
        {
            AddDeclToScope(&procDef->block, procType->args[i]);
            procDef->declsFlat.Append(procType->args[i]);
        }
        
        // Set current procedure
        auto prevProc = p->curProc;
        p->curProc = procDef;
        defer(p->curProc = prevProc);
        
        // Disable extern if there is a body.
        // TODO: ideally an error should be printed for this.
        procDecl->declSpecs &= ~Decl_Extern;
        
        while(p->at->kind != '}' && p->at->kind != Tok_EOF)
        {
            Ast_Stmt* stmt = ParseStatement(p);
            block.stmts.Append(scratch.arena(), (Ast_Node*)stmt);
        }
        
        block.stmts = block.stmts.CopyToArena(p->arena);
        
        EatRequiredToken(p, '}');
    }
    
    return procDecl;
}

Ast_StructDef* ParseStructDef(Parser* p, Ast_DeclSpec specs)
{
    ProfileFunc(prof);
    
    if(p->at->kind != Tok_Struct)
        ExpectedTokenError(p, p->at, Tok_Struct);
    
    auto structDef = Ast_MakeEntityNode<Ast_StructDef>(p, p->at);
    structDef->declSpecs = specs;
    
    Token* ident = 0;
    TypeInfo* typeInfo = ParseType(p, &ident);
    if(!IsTokIdent(ident->kind))
        ExpectedTokenError(p, ident, Tok_Ident);
    
    structDef->where = ident;
    structDef->type = typeInfo;
    structDef->name = ident->ident;
    AddDeclToScope(p->scope, structDef);
    return structDef;
}

bool IsNextDecl(Parser* p)
{
    bool isDecl = false;
    
    if(IsTokStartOfDecl(p->at->kind))
        isDecl = true;
    else
    {
        bool foundIdent = false;
        for(Token* token = p->at; token->kind != Tok_EOF; ++token)
        {
            if(foundIdent && IsTokIdent(token->kind))
            {
                isDecl = true;
                break;
            }
            else if(IsTokIdent(token->kind))
            {
                foundIdent = true;
                // There could be polymorphic parameters here.
                token = SkipParens(token + 1, '(', ')') - 1;
            }
            else break;
        }
    }
    
    return isDecl;
}

Ast_Node* ParseDeclOrExpr(Parser* p, Ast_DeclSpec specs, bool forceInit, bool ignoreInit)
{
    ProfileFunc(prof);
    
    Token* start = p->at;
    
    // Determine if it's a declaration or an expression first
    // If there were some declaration specifiers then it's a
    // declaration for sure.
    bool isDecl = specs || IsNextDecl(p);
    
    Ast_Node* result;
    if(isDecl)
        result = ParseVarDecl(p, specs, forceInit, ignoreInit);
    else
        result = ParseExpression(p);
    
    return result;
}

Ast_VarDecl* ParseVarDecl(Parser* p, Ast_DeclSpec specs, bool forceInit, bool ignoreInit)
{
    ProfileFunc(prof);
    
    Token* typeTok = p->at;
    
    Token* t = 0;
    auto type = ParseType(p, &t);
    if(!IsTokIdent(t->kind))
        ExpectedTokenError(p, t, Tok_Ident);
    
    // A global variable should be its own separate entity
    bool isGlobal = !p->curProc;
    auto decl  = isGlobal? Ast_MakeEntityNode<Ast_VarDecl>(p, t) : Ast_MakeNode<Ast_VarDecl>(p->arena, t);
    decl->type = type;
    decl->typeTok = typeTok;
    decl->declSpecs = specs;
    decl->name = t->ident;
    
    AddDeclToScope(p->scope, decl);
    
    if(isGlobal)
        decl->declIdx = -1;
    else
    {
        Assert(p->curProc);
        p->curProc->declsFlat.Append(decl);
        decl->declIdx = p->curProc->declsFlat.length - 1;
    }
    
    if(!ignoreInit)
    {
        if(forceInit || p->at->kind == '=')
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
    
    Ast_DeclSpec declSpecs = ParseDeclSpecs(p);
    
    switch_nocheck(p->at->kind)
    {
        case Tok_EOF:         return 0;
        case Tok_If:          stmt = ParseIf(p); break;
        case Tok_For:         stmt = ParseFor(p); break;
        case Tok_While:       stmt = ParseWhile(p); break;
        case Tok_Do:          stmt = ParseDoWhile(p); break;
        case Tok_Switch:      stmt = ParseSwitch(p); break;
        case Tok_Defer:       stmt = ParseDefer(p); break;
        case Tok_Return:      stmt = ParseReturn(p); break;
        case Tok_Break:       stmt = ParseJump<Ast_Break>(p); break;
        case Tok_Continue:    stmt = ParseJump<Ast_Continue>(p); break;
        case Tok_Fallthrough: stmt = ParseJump<Ast_Fallthrough>(p); break;
        case '{':             stmt = ParseBlock(p); break;
        case ';':
        {
            stmt = Ast_MakeNode(p->arena, p->at, AstKind_EmptyStmt);
            ++p->at;
            break;
        }
        default:
        {
            Token* start = p->at;
            stmt = ParseDeclOrExpr(p, declSpecs, false, true);
            
            if(p->at->kind == ',')  // Multi assign
            {
                ++p->at;
                
                auto multiAssign = Ast_MakeNode<Ast_MultiAssign>(p->arena, start);
                
                ScratchArena scratch;
                Slice<Ast_Node*> lefts = { 0, 0 };
                Slice<Ast_Expr*> rights = { 0, 0 };
                
                lefts.Append(scratch, stmt);
                stmt = multiAssign;
                
                while(true)
                {
                    // Parse declaration
                    Ast_Node* toAppend = 0;
                    if(declSpecs || IsNextDecl(p)) toAppend = ParseVarDecl(p, declSpecs, false, true);
                    else toAppend = ParseExpression(p, INT_MAX, true);
                    
                    lefts.Append(scratch, toAppend);
                    
                    if(p->at->kind == ',') ++p->at;
                    else break;
                }
                
                multiAssign->lefts = lefts.CopyToArena(p->arena);
                
                if(p->at->kind == '=')
                {
                    multiAssign->where = p->at;
                    ++p->at;
                    
                    while(true)
                    {
                        rights.Append(scratch, ParseExpression(p));
                        
                        if(p->at->kind == ',') ++p->at;
                        else break;
                    }
                    
                    multiAssign->rights = rights.CopyToArena(p->arena);
                }
                
            }
            else if(stmt->kind == AstKind_VarDecl && p->at->kind == '=')  // Single initialization
            {
                ++p->at;
                auto varDecl = (Ast_VarDecl*)stmt;
                varDecl->initExpr = ParseExpression(p);
            }
            
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
    
    stmt->thenBlock = Ast_MakeNode<Ast_Block>(p->arena, p->at);
    stmt->thenBlock->enclosing = p->scope;
    p->scope = stmt->thenBlock;
    defer(p->scope = p->scope->enclosing);
    
    EatRequiredToken(p, '(');
    auto declSpecs = ParseDeclSpecs(p);
    stmt->condition = ParseDeclOrExpr(p, declSpecs);
    EatRequiredToken(p, ')');
    
    ParseOneOrMoreStmtBlock(p, stmt->thenBlock);
    
    if(p->at->kind == Tok_Else)
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
    
    stmt->body = Ast_MakeNode<Ast_Block>(p->arena, p->at);
    stmt->body->enclosing = p->scope;
    p->scope = stmt->body;
    defer(p->scope = p->scope->enclosing);
    
    EatRequiredToken(p, '(');
    if(p->at->kind != ';')
    {
        auto declSpecs = ParseDeclSpecs(p);
        stmt->initialization = ParseDeclOrExpr(p, declSpecs);
    }
    
    EatRequiredToken(p, ';');
    
    if(p->at->kind != ';')
    {
        auto declSpecs = ParseDeclSpecs(p);
        stmt->condition = ParseDeclOrExpr(p, declSpecs, true);
    }
    
    EatRequiredToken(p, ';');
    
    if(p->at->kind != ')')
        stmt->update = ParseExpression(p);
    
    EatRequiredToken(p, ')');
    
    ParseOneOrMoreStmtBlock(p, stmt->body);
    return stmt;
}

Ast_While* ParseWhile(Parser* p)
{
    ProfileFunc(prof);
    
    auto stmt = Ast_MakeNode<Ast_While>(p->arena, p->at);
    
    EatRequiredToken(p, Tok_While);
    
    stmt->doBlock = Ast_MakeNode<Ast_Block>(p->arena, p->at);
    stmt->doBlock->enclosing = p->scope;
    p->scope = stmt->doBlock;
    defer(p->scope = p->scope->enclosing);
    
    EatRequiredToken(p, '(');
    auto declSpecs = ParseDeclSpecs(p);
    stmt->condition = ParseDeclOrExpr(p, declSpecs);
    EatRequiredToken(p, ')');
    
    ParseOneOrMoreStmtBlock(p, stmt->doBlock);
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

Ast_Switch* ParseSwitch(Parser* p)
{
    ProfileFunc(prof);
    ScratchArena caseScratch(0);
    ScratchArena stmtScratch(1);
    
    auto stmt = Ast_MakeNode<Ast_Switch>(p->arena, p->at);
    
    EatRequiredToken(p, Tok_Switch);
    EatRequiredToken(p, '(');
    stmt->switchExpr = ParseExpression(p);
    EatRequiredToken(p, ')');
    
    EatRequiredToken(p, '{');
    
    while(p->at->kind == Tok_Case || p->at->kind == Tok_Default)
    {
        if(p->at->kind == Tok_Default)
        {
            stmt->defaultIdx = stmt->cases.length;
            ++p->at;
        }
        else
        {
            ++p->at;
            stmt->cases.Append(caseScratch, ParseExpression(p));
        }
        
        EatRequiredToken(p, ':');
        stmt->stmts.Append(stmtScratch, ParseStatement(p));
    }
    
    stmt->stmts = stmt->stmts.CopyToArena(p->arena);
    stmt->cases = stmt->cases.CopyToArena(p->arena);
    
    EatRequiredToken(p, '}');
    
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
    ScratchArena scratch;
    
    auto stmt = Ast_MakeNode<Ast_Return>(p->arena, p->at);
    EatRequiredToken(p, Tok_Return);
    
    if(p->at->kind != ';')
    {
        Slice<Ast_Expr*> exprs = { 0, 0 };
        while(true)
        {
            auto expr = ParseExpression(p);
            exprs.Append(scratch, expr);
            
            if(p->at->kind == ',') ++p->at;
            else break;
        }
        
        stmt->rets = exprs.CopyToArena(p->arena);
    }
    
    EatRequiredToken(p, ';');
    return stmt;
}

template<typename t>
Ast_Stmt* ParseJump(Parser* p)
{
    ProfileFunc(prof);
    
    auto stmt = Ast_MakeNode<t>(p->arena, p->at);
    ++p->at;
    
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
    
    while(p->at->kind != '}' && p->at->kind != Tok_EOF)
    {
        Ast_Node* stmt = ParseStatement(p);
        block->stmts.Append(scratch, stmt);
    }
    
    block->stmts = block->stmts.CopyToArena(p->arena);
    
    EatRequiredToken(p, '}');
    return block;
}

void ParseOneOrMoreStmtBlock(Parser* p, Ast_Block* outBlock)
{
    ProfileFunc(prof);
    
    if(p->at->kind == '{')  // Multiple statements
    {
        outBlock->where = p->at;
        ++p->at;
        
        ScratchArena scratch;
        while(p->at->kind != '}' && p->at->kind != Tok_EOF)
        {
            Ast_Node* stmt = ParseStatement(p);
            outBlock->stmts.Append(scratch.arena(), stmt);
        }
        
        outBlock->stmts = outBlock->stmts.CopyToArena(p->arena);
        
        EatRequiredToken(p, '}');
    }
    else  // Only one statement
    {
        outBlock->stmts = { 0, 1 };
        auto stmtPtr = Arena_AllocVarPack(p->arena, Ast_Node*);
        *stmtPtr = ParseStatement(p);
        outBlock->stmts.ptr = stmtPtr;
    }
}

// NOTE: All unary operators always have higher precedence over
// binary operators. If this is not the case anymore, change
// the implementation of this procedure a bit.
Ast_Expr* ParseExpression(Parser* p, int prec, bool ignoreEqual)
{
    ProfileFunc(prof);
    
    Ast_Expr* lhs = 0;
    
    // Prefix operators (basic linked-list construction)
    Ast_Expr* prefixExpr = 0;
    Ast_Expr** baseExpr = &prefixExpr;
    while(IsOpPrefix(p->at->kind))
    {
        if(p->at->kind == Tok_Cast)  // Typecast
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
            tmp->op = p->at->kind;
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
        int opPrec = GetOperatorPrec(p->at->kind);
        
        bool undoRecurse = false;
        undoRecurse |= (opPrec == -1);
        undoRecurse |= (opPrec > prec);  // Is it's less important (=greater priority) don't recurse
        undoRecurse |= (opPrec == prec && IsOperatorLToR(p->at->kind));
        undoRecurse |= (p->at->kind == '=' && ignoreEqual);  // If we're supposed to ignore the equal sign, stop here
        if(undoRecurse)
        {
            /* TODO: test this
            if(p->at->kind == '?')  // Ternary operator(s)
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
        binOp->op = p->at->kind;
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
    while(IsOpPostfix(p->at->kind))
    {
        scratch.Reset();
        switch_nocheck(p->at->kind)
        {
            case '.':  // Member access
            {
                auto memberAccess = Ast_MakeNode<Ast_MemberAccess>(p->arena, p->at);
                ++p->at;
                
                Token* ident = EatRequiredToken(p, Tok_Ident);
                memberAccess->memberName = ident->ident;
                memberAccess->target = curExpr;
                curExpr = memberAccess;
                
                break;
            }
            case '(':  // Function call
            {
                auto call = Ast_MakeNode<Ast_FuncCall>(p->arena, p->at);
                ++p->at;
                
                if(p->at->kind != ')')
                {
                    // Parse arguments
                    while(true)
                    {
                        auto argExpr = ParseExpression(p);
                        call->args.Append(scratch.arena(), argExpr);
                        
                        if(p->at->kind == ',') ++p->at;
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
                unary->op = p->at->kind;
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
    
    if(p->at->kind == '(')  // Parenthesis expression
    {
        ++p->at;
        auto primary = ParseExpression(p);
        EatRequiredToken(p, ')');
        return primary;
    }
    else if(IsTokIdent(p->at->kind))
    {
        auto ident = Ast_MakeNode<Ast_IdentExpr>(p->arena, p->at);
        ident->name = p->at->ident;
        ++p->at;
        return ident;
    }
    else if(p->at->kind == Tok_IntNum)
    {
        auto constExpr = Ast_MakeNode<Ast_ConstValue>(p->arena, p->at);
        constExpr->type = &Typer_Int64;
        constExpr->addr = Arena_FromStackPack(p->arena, p->at->intValue);
        
        ++p->at;
        return constExpr;
    }
    else if(p->at->kind == Tok_FloatNum)
    {
        auto constExpr = Ast_MakeNode<Ast_ConstValue>(p->arena, p->at);
        constExpr->type = &Typer_Float;
        constExpr->addr = Arena_FromStackPack(p->arena, p->at->floatValue);
        
        ++p->at;
        return constExpr;
    }
    else if(p->at->kind == Tok_DoubleNum)
    {
        auto constExpr = Ast_MakeNode<Ast_ConstValue>(p->arena, p->at);
        constExpr->type = &Typer_Double;
        constExpr->addr = Arena_FromStackPack(p->arena, p->at->doubleValue);
        
        ++p->at;
        return constExpr;
    }
    else if(p->at->kind == Tok_True || p->at->kind == Tok_False)
    {
        auto constExpr = Ast_MakeNode<Ast_ConstValue>(p->arena, p->at);
        constExpr->type = &Typer_Bool;
        bool val = p->at->kind == Tok_True;
        constExpr->addr = Arena_FromStackPack(p->arena, val);
        
        ++p->at;
        return constExpr;
    }
    
    String tokType = TokTypeToString(p->at->kind, scratch.arena());
    
    StringBuilder strBuilder(scratch);
    
    strBuilder.Append("Unexpected '");
    strBuilder.Append(tokType);
    strBuilder.Append("', expecting expression");
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
        if(IsTokIdent(p->at->kind))
        {
            if(p->at->kind == Tok_Ident)  // Compound type
            {
                auto tmp = Ast_MakeType<Ast_IdentType>(p->arena);
                tmp->name = p->at->ident;
                *baseType = tmp;
                
                // TODO: polymorphic parameters
            }
            else  // Primitive type
            {
                *baseType = TokToPrimitiveType(p->at->kind);
            }
            
            ++p->at;
            
            // Ident should be the last type declarator
            loop = false;
        }
        else switch_nocheck(p->at->kind)
        {
            case '^':
            {
                auto tmp = Ast_MakeType<Ast_PtrType>(p->arena);
                ++p->at;
                
                *baseType = tmp;
                baseType = &tmp->baseType;
                break;
            }
            case '[':
            {
                auto tmp = Ast_MakeType<Ast_ArrType>(p->arena);
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
                Ast_ProcType decl = ParseProcType(p, &ident, false);
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
                Ast_StructType decl = ParseStructType(p, &ident);
                auto tmp = Arena_FromStackPack(p->arena, decl);
                if(ident)
                    *outIdent = ident;
                
                *baseType = tmp;
                
                // Struct should be the last type declarator
                loop = false;
                break;
            }
            case Tok_Raw:
            {
                *baseType = &Typer_Raw;
                loop = false;
                break;
            }
            default:
            {
                loop = false;
                ParseError(p, p->at, StrLit("Expecting a type"));
                break;
            }
        } switch_nocheck_end;
    }
    
    // The identifier is only located after
    // the type if it's not a procedure/procptr/struct
    if(!(*outIdent))
    {
        *outIdent = p->at;
        if(IsTokIdent(p->at->kind)) ++p->at;
    }
    
    return type;
}

Ast_ProcType ParseProcType(Parser* p, Token** outIdent, bool forceArgNames)
{
    ProfileFunc(prof);
    
    ScratchArena scratch;
    
    Assert(outIdent);
    *outIdent = 0;
    
    // It's a proc or an operator
    bool isOperator = p->at->kind == Tok_Operator;
    
    if(p->at->kind == Tok_Proc || p->at->kind == Tok_Operator)
        ++p->at;
    else  // This in practice never happens
    {
        String tokType = TokTypeToString(p->at->kind, scratch.arena());
        
        StringBuilder strBuilder(scratch);
        strBuilder.Append("Unexpected '");
        strBuilder.Append(tokType);
        strBuilder.Append("', expecting 'proc', or 'operator'");
        ParseError(p, p->at, strBuilder.string);
    }
    
    // Parse function declaration
    
    *outIdent = p->at;  // It's set to the identifier or the operator if it's an operator declaration
    
    if(!isOperator && IsTokIdent(p->at->kind))
        ++p->at;
    else if(isOperator && IsTokOperator(p->at->kind))
        ++p->at;
    
    if(IsTokIdent(p->at->kind)) ++p->at;
    
    EatRequiredToken(p, '(');
    
    Ast_ProcType decl;
    decl.isOperator = isOperator;
    
    // Parse arguments
    if(p->at->kind != ')')
    {
        Slice<Ast_Declaration*> args = { 0, 0 };
        while(true)
        {
            Ast_DeclSpec specs = ParseDeclSpecs(p);
            
            Token* t = 0;
            auto type = ParseType(p, &t);
            if(!IsTokIdent(t->kind))
                ExpectedTokenError(p, t, Tok_Ident);
            
            //auto argDecl = ParseVarDecl(p, (Ast_DeclSpec)0, false, true);
            auto argDecl = Ast_MakeNode<Ast_VarDecl>(p->arena, t);
            argDecl->type = type;
            argDecl->declSpecs = specs;
            argDecl->name = t->ident;
            
            args.Append(scratch, argDecl);
            argDecl->declIdx = args.length - 1;
            
            if(p->at->kind == ',') ++p->at;
            else break;
        }
        
        decl.args = args.CopyToArena(p->arena);
    }
    
    scratch.Reset();
    EatRequiredToken(p, ')');
    
    // Parse return types
    if(p->at->kind == Tok_Arrow)  // There is at least one return type
    {
        ++p->at;
        if(p->at->kind != '(')
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
                
                if(p->at->kind == ',') ++p->at;
                else break;
            }
            
            decl.retTypes = decl.retTypes.CopyToArena(p->arena);
            EatRequiredToken(p, ')');
        }
    }
    
    return decl;
}

Ast_StructType ParseStructType(Parser* p, Token** outIdent)
{
    ProfileFunc(prof);
    
    Assert(outIdent);
    *outIdent = 0;
    
    EatRequiredToken(p, Tok_Struct);
    
    Ast_StructType decl;
    
    *outIdent = p->at;
    if(IsTokIdent(p->at->kind)) ++p->at;
    
    EatRequiredToken(p, '{');
    
    if(p->at->kind == '}')
    {
        ++p->at;
        return decl;
    }
    
    // Parse arguments
    ScratchArena typeScratch(0);
    ScratchArena tokenScratch(1);
    
    Slice<TypeInfo*> types = { 0, 0 };
    Slice<Token*> tokens = { 0, 0 };
    
    while(p->at->kind != '}' && p->at->kind != Tok_EOF)
    {
        if(p->at->kind == ';')
        {
            ++p->at;
            continue;
        }
        
        Token* argName = 0;
        TypeInfo* type = ParseType(p, &argName);
        if(!IsTokIdent(argName->kind))
            ExpectedTokenError(p, argName, Tok_Ident);
        
        types.Append(typeScratch.arena(), type);
        tokens.Append(tokenScratch.arena(), argName);
        
        EatRequiredToken(p, ';');
    }
    
    decl.memberTypes      = types.CopyToArena(p->arena);
    decl.memberNameTokens = tokens.CopyToArena(p->arena);
    
    decl.memberNames.ptr    = Arena_AllocArrayPack(p->arena, tokens.length, HashedString);
    decl.memberNames.length = tokens.length;
    
    decl.memberOffsets.ptr    = Arena_AllocArrayPack(p->arena, types.length, uint32);
    decl.memberOffsets.length = types.length;
    
    //for_array(i, decl.memberNames)
    //decl.memberNames[i] = decl.memberNameTokens[i]->ident;
    
    for_array(i, decl.memberNames)
        decl.memberNames[i] = decl.memberNameTokens[i]->ident; 
    
    EatRequiredToken(p, '}');
    return decl;
}

Ast_DeclSpec ParseDeclSpecs(Parser* p)
{
    Ast_DeclSpec res = (Ast_DeclSpec)0;
    
    while(true)
    {
        Ast_DeclSpec spec = Ast_TokTypeToDeclSpec(p->at->kind);
        if(spec == 0)
            break;
        
        res = (Ast_DeclSpec)(res | spec);
        ++p->at;
    }
    
    return res;
}

// A table could be used for this information
// These are binary operators only
int GetOperatorPrec(TokenKind tokType)
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
        
        case Tok_LogAnd:
        return 11;
        
        case Tok_LogOr:
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

bool IsOpPrefix(TokenKind tokType)
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
inline bool IsOpPostfix(TokenKind tokType)
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

inline bool IsTokOperator(TokenKind tokType)
{
    return GetOperatorPrec(tokType) != -1 || IsOpPostfix(tokType) || IsOpPrefix(tokType);
}

bool IsOperatorLToR(TokenKind tokType)
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
    if(start->kind == openParen)
        ++parenLevel;
    
    Token* result = start;
    while(parenLevel > 0 && result->kind != Tok_EOF)
    {
        ++result;
        
        if(result->kind == openParen)
            ++parenLevel;
        else if(result->kind == closeParen)
            --parenLevel;
    }
    
    return result;
}

inline void ParseError(Parser* p, Token* token, String message)
{
    if(p->status)
        CompileError(p->tokenizer, token, message);
    
    p->status   = false;
    p->at->kind = Tok_EOF;
}

inline Token* EatRequiredToken(Parser* p, TokenKind tokType)
{
    if(p->at->kind != tokType && !(IsTokIdent(p->at->kind) && IsTokIdent(tokType)))
    {
        ExpectedTokenError(p, tokType);
        return p->at;
    }
    
    return p->at++;
}

inline Token* EatRequiredToken(Parser* p, char tokType)
{
    return EatRequiredToken(p, (TokenKind)tokType);
}

inline void ExpectedTokenError(Parser* p, Token* tok, TokenKind tokType)
{
    if(IsTokIdent(tokType))
        ParseError(p, tok, StrLit("Expecting identifier"));
    else
    {
        ScratchArena scratch;
        String tokTypeStr = TokTypeToString(tokType, scratch);
        
        StringBuilder strBuilder(scratch);
        strBuilder.Append(3, StrLit("Expecting '"), tokTypeStr, StrLit("'"));
        ParseError(p, tok, strBuilder.string);
    }
}

inline void ExpectedTokenError(Parser* p, TokenKind tokType)
{
    ExpectedTokenError(p, p->at, tokType);
}

inline void ExpectedTokenError(Parser* p, Token* tok, char tokType)
{
    ExpectedTokenError(p, tok, (TokenKind)tokType);
}

inline void ExpectedTokenError(Parser* p, char tokType)
{
    ExpectedTokenError(p, tokType);
}
