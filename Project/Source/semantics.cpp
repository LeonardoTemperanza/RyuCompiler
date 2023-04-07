
#include "semantics.h"

// Primitive type infos
Type int32Type = { false, false, false, false, Typeid_Int32 };

static void SemanticError(Token token, Typer* ctx, char* message)
{
    ctx->errorOccurred = true;
    CompilationError(token, ctx->parser->tokenizer, "Semantic error", message);
}

Typer InitTyper(Arena* arena, Arena* tempArena, Arena* scopeStackArena, Parser* parser)
{
    Typer t;
    t.arena = arena;
    t.tempArena = tempArena;
    t.scopeStackArena = scopeStackArena;
    t.parser = parser;
    return t;
}

static void StartScope(Typer* ctx)
{
    Arena_AllocVar(ctx->scopeStackArena, uint32);
    ++ctx->curScopeIdx;
    ++ctx->scopeCounter;
    ctx->curScopeStack[ctx->curScopeIdx] = ctx->scopeCounter;
}

static void EndScope(Typer* ctx)
{
    ctx->scopeStackArena->offset = (size_t)((&ctx->curScopeStack[ctx->curScopeIdx]) - ctx->scopeStackArena->offset);
    --ctx->curScopeIdx;
}

static int IdentHashFunction(String str)
{
    int result = 0;
    int smallPrime = 3;
    
    for(int i = 0; i < str.length; ++i)
        result = (result * smallPrime + str[i]) % SymTable_ArraySize;
    
    return result;
}

// Doesn't use the scope logic
static Symbol* GetGlobalSymbol(SymbolTable* symTable, String str)
{
    Symbol* result = 0;
    int idx = IdentHashFunction(str);
    
    SymbolTableEntry* symTableEntry = &symTable->symArray[idx];
    if(symTableEntry->occupied)
    {
        for(Symbol* curSym = &symTableEntry->symbol;
            curSym; curSym = curSym->next)
        {
            if(str == curSym->ident)
                return curSym;
        }
    }
    
    return result;
}

static Symbol* GetGlobalSymbolOrError(Typer* ctx, Token token)
{
    Symbol* symbol = GetGlobalSymbol(&ctx->globalSymTable, token.ident);
    if(!symbol)
    {
        SemanticError(token, ctx, "Undeclared identifier");
        ctx->errorOccurred = true;
    }
    
    return symbol;
}

static Symbol* GetSymbol(SymbolTable* symTable, String str, uint32* scopeStack, int scopeIdx)
{
    Symbol* result = 0;
    int idx = IdentHashFunction(str);
    
    // Look for the last matching symbol (in order from farthest to closest)
    for (int i = 0; i <= scopeIdx; ++i)
    {
        // Symbol is stored in table with offset based on the scope id
        int curTableIdx = (idx + scopeStack[i]) % SymTable_ArraySize;
        
        SymbolTableEntry* symTableEntry = &symTable->symArray[curTableIdx];
        if(symTableEntry->occupied)
        {
            for(Symbol* curSym = &symTableEntry->symbol;
                curSym; curSym = curSym->next)
            {
                if(str == curSym->ident)
                    result = curSym;
            }
        }
    }
    
    return result;
}

static Symbol* GetSymbolOrError(Typer* ctx, Token token)
{
    Symbol* symbol = GetSymbol(ctx->curSymTable, token.ident, ctx->curScopeStack, ctx->curScopeIdx);
    if(!symbol)
    {
        SemanticError(token, ctx, "Undeclared identifier");
    }
    
    return symbol;
}

static InsertSymbol_Ret InsertSymbol(Arena* arena, SymbolTable* symTable, String str, uint32 scopeId)
{
    InsertSymbol_Ret result;
    
    int idx = IdentHashFunction(str);
    idx = (idx + scopeId) % SymTable_ArraySize;
    
    Symbol* resultSym = 0;
    bool found = false;
    
    // Allocate the string
    char* copiedStr = Arena_PushString(arena,
                                       str.ptr, str.length);
    
    SymbolTableEntry* curTableEntry = symTable->symArray + idx;
    if(curTableEntry->occupied)
    {
        Symbol* curSymbol = &curTableEntry->symbol;
        while(curSymbol->next)
        {
            if(curSymbol->ident == str && curSymbol->scopeId == scopeId)
                found = true;
            
            curSymbol = curSymbol->next;
        }
        
        if(curSymbol->ident == str && curSymbol->scopeId == scopeId)
            found = true;
        
        if(!found)
        {
            auto newSymbol = Arena_AllocAndInit(arena, Symbol);
            newSymbol->ident   = { copiedStr, str.length };
            curSymbol->next    = newSymbol;
            curSymbol->scopeId = scopeId;
            resultSym = curSymbol->next;
        }
    }
    else
    {
        curTableEntry->symbol.ident = { copiedStr, str.length };
        curTableEntry->occupied = true;
        curTableEntry->symbol.next = 0;
        curTableEntry->symbol.scopeId = scopeId;
        resultSym = &(curTableEntry->symbol);
    }
    
    result = { resultSym, found };
    return result;
}


static InsertSymbol_Ret InsertSymbol(Arena* arena, SymbolTable* symTable, Token token, uint32 scopeId, void* ast, SymbolType type)
{
    String str = token.ident;
    
    InsertSymbol_Ret result;
    
    int idx = IdentHashFunction(str);
    idx = (idx + scopeId) % SymTable_ArraySize;
    
    Symbol* resultSym = 0;
    bool found = false;
    
    // Allocate the string
    char* copiedStr = Arena_PushString(arena,
                                       str.ptr, str.length);
    
    SymbolTableEntry* curTableEntry = symTable->symArray + idx;
    if(curTableEntry->occupied)
    {
        Symbol* curSymbol = &curTableEntry->symbol;
        while(curSymbol->next)
        {
            if(curSymbol->ident == str && curSymbol->scopeId == scopeId)
                found = true;
            
            curSymbol = curSymbol->next;
        }
        
        if(curSymbol->ident == str && curSymbol->scopeId == scopeId)
            found = true;
        
        if(!found)
        {
            auto newSymbol = Arena_AllocAndInit(arena, Symbol);
            newSymbol->ident   = { copiedStr, str.length };
            curSymbol->next    = newSymbol;
            curSymbol->scopeId = scopeId;
            curSymbol->symType = type;
            curSymbol->decl    = (Ast_Declaration*)ast;
            resultSym = curSymbol->next;
        }
    }
    else
    {
        curTableEntry->symbol.ident = { copiedStr, str.length };
        curTableEntry->occupied = true;
        curTableEntry->symbol.next = 0;
        curTableEntry->symbol.scopeId = scopeId;
        curTableEntry->symbol.symType = type;
        curTableEntry->symbol.decl    = (Ast_Declaration*)ast;
        resultSym = &(curTableEntry->symbol);
    }
    
    result = { resultSym, found };
    return result;
}

static InsertSymbol_Ret InsertSymbol(Arena* arena, SymbolTable* symTable, Token token, uint32 scopeId, void* astPtr)
{
    String str = token.ident;
    
    InsertSymbol_Ret result;
    
    int idx = IdentHashFunction(str);
    idx = (idx + scopeId) % SymTable_ArraySize;
    
    Symbol* resultSym = 0;
    bool found = false;
    
    // Allocate the string
    char* copiedStr = Arena_PushString(arena,
                                       str.ptr, str.length);
    
    SymbolTableEntry* curTableEntry = symTable->symArray + idx;
    if(curTableEntry->occupied)
    {
        Symbol* curSymbol = &curTableEntry->symbol;
        while(curSymbol->next)
        {
            if(curSymbol->ident == str && curSymbol->scopeId == scopeId)
                found = true;
            
            curSymbol = curSymbol->next;
        }
        
        if(curSymbol->ident == str && curSymbol->scopeId == scopeId)
            found = true;
        
        if(!found)
        {
            auto newSymbol = Arena_AllocAndInit(arena, Symbol);
            newSymbol->ident   = { copiedStr, str.length };
            //newSymbol->token   = token;
            //newSymbol->astPtr  = astPtr;
            curSymbol->next    = newSymbol;
            curSymbol->scopeId = scopeId;
            resultSym = curSymbol->next;
        }
    }
    else
    {
        curTableEntry->symbol.ident = { copiedStr, str.length };
        curTableEntry->occupied = true;
        curTableEntry->symbol.next = 0;
        curTableEntry->symbol.scopeId = scopeId;
        resultSym = &(curTableEntry->symbol);
    }
    
    result = { resultSym, found };
    return result;
}

static Symbol* InsertGlobalSymbolOrError(Typer* ctx, Token token, void* astPtr)
{
    InsertSymbol_Ret ret = InsertSymbol(ctx->arena, &ctx->globalSymTable, token, 0, astPtr);
    if(ret.alreadyDefined)
    {
        SemanticError(token, ctx, "Redefinition");
        return 0;
    }
    
    return ret.res;
}

static Symbol* InsertGlobalSymbolOrError(Typer* ctx, Token token)
{
    InsertSymbol_Ret ret = InsertSymbol(ctx->arena, &ctx->globalSymTable, token.ident, 0);
    if(ret.alreadyDefined)
    {
        SemanticError(token, ctx, "Redefinition");
        return 0;
    }
    
    return ret.res;
}

static Symbol* InsertSymbolOrError(Typer* ctx, Token token)
{
    InsertSymbol_Ret ret = InsertSymbol(ctx->arena, ctx->curSymTable,
                                        token.ident, ctx->curScopeStack[ctx->curScopeIdx]);
    if(ret.alreadyDefined)
    {
        SemanticError(token, ctx, "Redefinition in the current scope");
        return 0;
    }
    
    return ret.res;
}

static Symbol* InsertSymbolOrError(Typer* ctx, Token token, void* astPtr, SymbolType type)
{
    InsertSymbol_Ret ret = InsertSymbol(ctx->arena, ctx->curSymTable,
                                        token, ctx->curScopeStack[ctx->curScopeIdx], astPtr, type);
    if(ret.alreadyDefined)
    {
        SemanticError(token, ctx, "Redefinition in the current scope");
        return 0;
    }
    
    return ret.res;
}

bool TypesEqual(Type* type1, Type* type2)
{
    if(type1 == type2)
        return true;
    
    if(type1->typeId != type2->typeId)
        return false;
    
    if(type1->typeId >= Typeid_BasicTypesBegin && type1->typeId <= Typeid_BasicTypesEnd)
        return true;
    
    switch(type1->typeId)
    {
        default: Assert(false);
        case Typeid_Ptr:
        {
            return TypesEqual(type1->ptrType.baseType, type2->ptrType.baseType);
        }
        break;
        case Typeid_Array:  // Here you may have to evaluate this expression.
        case Typeid_Struct:
        break;
    }
    
    return false;
};

void PerformTypingStage(Typer* ctx, Ast_Root* ast)
{
    printf("Performing typing stage!\n");
    
    // First pass for function declarations
    for (int i = 0; i < ast->topStmts.length; ++i)
    {
        auto stmt = ast->topStmts[i];
        
        if (!stmt)
            return;
        
        if(stmt->type == Function)
        {
            auto funcDefinition = (Ast_Function*)stmt;
            
            // Insert in the symbol table
            InsertGlobalSymbolOrError(ctx, funcDefinition->prototype->token, funcDefinition->prototype);
            
            //genCode = GenerateProto(ctx, funcDefinition->prototype);
        }
        else if(stmt->type == Prototype)
        {
            auto proto = (Ast_Prototype*)stmt;
            InsertGlobalSymbolOrError(ctx, proto->token, proto);
            //genCode = GenerateProto(ctx, (Ast_Prototype*)stmt);
        }
        else
            return;
    }
    
    // Second pass for function definitions
    for (int i = 0; i < ast->topStmts.length; ++i)
    {
        auto stmt = ast->topStmts[i];
        
        if(stmt->type == Function)
        {
            ctx->scopeCounter = 0;
            ctx->curScopeIdx  = 0;
            auto savepoint = Arena_TempBegin(ctx->arena);
            defer(Arena_TempEnd(savepoint));
            ctx->curSymTable = Arena_AllocAndInit(ctx->arena, SymbolTable);
            
            auto funcDefinition = (Ast_Function*)stmt;
            
            Symbol* proto = GetGlobalSymbol(&ctx->globalSymTable, funcDefinition->prototype->token.ident);
            if(!proto)
            {
                fprintf(stderr, "Internal error: declaration not found during function definition\n");
                return;
            }
            
            auto savepoint2 = Arena_TempBegin(ctx->scopeStackArena);
            ctx->curScopeStack = Arena_AllocVar(ctx->scopeStackArena, uint32);
            ctx->curScopeStack[0] = 0;
            defer(Arena_TempEnd(savepoint));
            
            StartScope(ctx);
            Semantics_Block(ctx, funcDefinition->body);
            
            //GenerateBody(ctx, funcDefinition, proto->address);
            ctx->curSymTable = 0;
            
            if(ctx->errorOccurred)
                return;
        }
    }
    
#if 0
    for(int i = 0; i < ast->topStmts.length; ++i)
    {
        Ast_TopLevelStmt* topStmt = ast->topStmts[i];
        
        if(topStmt->type == Function)
        {
            auto body = ((Ast_Function*)topStmt)->body;
            Semantics_Block(ctx, body);
        }
        else if(topStmt->type == Prototype)
        {
            
        }
        else
            Assert(false && "Not yet implemented!");
    }
#endif
}

void Semantics_Block(Typer* ctx, Ast_BlockStmt* block)
{
    for(Ast_BlockNode* it = block->first; it; it = it->next)
    {
        if(it->stmt->type == BlockStmt)
        {
            StartScope(ctx);
            Semantics_Block(ctx, (Ast_BlockStmt*)it->stmt);
            EndScope(ctx);
        }
        else
        {
            Semantics_Stmt(ctx, it->stmt);
        }
        
        if(ctx->errorOccurred)
            return;
    }
}

void Semantics_Declaration(Typer* ctx, Ast_Declaration* decl)
{
    // Maybe handle type inference here in the future?
    
    InsertSymbolOrError(ctx, decl->nameIdent, decl, IsSymbolVar);
    
    if(decl->initExpr)
    {
        TypeCheckExpr(ctx, decl->initExpr);
        
        if(!TypesEqual(decl->typeInfo, decl->initExpr->typeInfo))
        {
            SemanticError(decl->nameIdent, ctx, "Mismatching types");
        }
    }
}

void Semantics_Stmt(Typer* ctx, Ast_Stmt* stmt)
{
    switch(stmt->type)
    {
        default: Assert(false && "Not implemented yet!"); return;
        case EmptyStmt: break;
        case DeclStmt: Semantics_Declaration(ctx, (Ast_Declaration*)stmt); break;
        case ExprStmt: TypeCheckExpr(ctx, ((Ast_ExprStmt*)stmt)->expr); break;
        case BlockStmt: Semantics_Block(ctx, (Ast_BlockStmt*)stmt); break;
        case ReturnStmt: break;
    }
}

void TypeCheckExpr(Typer* ctx, Ast_ExprNode* expr)
{
    if(!expr)
        return;
    
    switch(expr->type)
    {
        default: Assert(false && "Not implemented yet!"); return;
        case BinNode:
        {
            auto binExpr = (Ast_BinExprNode*)expr;
            TypeCheckExpr(ctx, binExpr->left);
            TypeCheckExpr(ctx, binExpr->right);
            
            if(ctx->errorOccurred)
                return;
            
            if(!TypesEqual(binExpr->left->typeInfo, binExpr->right->typeInfo))
            {
                SemanticError(binExpr->token, ctx, "Mismatching types");
                return;
            }
            
            binExpr->typeInfo = binExpr->left->typeInfo;
        }
        break;
        case UnaryNode:
        {
            
        }
        break;
        case CallNode:
        {
            
        }
        break;
        case IdentNode:
        {
            Symbol* sym = GetSymbolOrError(ctx, expr->token);
            if(!sym)
                return;
            
            expr->typeInfo = sym->decl->typeInfo;
        }
        break;
        case NumNode:
        {
            expr->typeInfo = &int32Type;
        }
        break;
    }
}