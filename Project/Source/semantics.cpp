
#include "semantics.h"

// TODO: Basic stuff to get going
enum BasicType
{
    ErrorType = -1,
    IntType,
    FloatType
};

BasicType GetType(String str)
{
    if(str == "int")
        return IntType;
    if(str == "float")
        return FloatType;
    
    return ErrorType;
};

struct Semantics_Ctx
{
    Parser* parser;
};

static void SemanticError(Token token, IR_Context* ctx, char* message)
{
    CompilationError(token, ctx->parser->tokenizer, "Semantic error", message);
}

// TODO(Leo): better hash function
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

static Symbol* GetGlobalSymbolOrError(IR_Context* ctx, Token token)
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

static Symbol* GetSymbolOrError(IR_Context* ctx, Token token)
{
    Symbol* symbol = GetSymbol(ctx->curSymTable, token.ident, ctx->curScopeStack, ctx->curScopeIdx);
    if(!symbol)
    {
        SemanticError(token, ctx, "Undeclared identifier");
        ctx->errorOccurred = true;
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
            newSymbol->token   = token;
            newSymbol->astPtr  = astPtr;
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

static Symbol* InsertGlobalSymbolOrError(IR_Context* ctx, Token token, void* astPtr)
{
    InsertSymbol_Ret ret = InsertSymbol(ctx->arena, &ctx->globalSymTable, token, 0, astPtr);
    if(ret.alreadyDefined)
    {
        SemanticError(token, ctx, "Redefinition");
        return 0;
    }
    
    return ret.res;
}

static Symbol* InsertGlobalSymbolOrError(IR_Context* ctx, Token token)
{
    InsertSymbol_Ret ret = InsertSymbol(ctx->arena, &ctx->globalSymTable, token.ident, 0);
    if(ret.alreadyDefined)
    {
        SemanticError(token, ctx, "Redefinition");
        return 0;
    }
    
    return ret.res;
}

static Symbol* InsertSymbolOrError(IR_Context* ctx, Token token)
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

static Symbol* InsertSymbolOrError(IR_Context* ctx, Token token, void* astPtr)
{
    InsertSymbol_Ret ret = InsertSymbol(ctx->arena, ctx->curSymTable,
                                        token, ctx->curScopeStack[ctx->curScopeIdx], astPtr);
    if(ret.alreadyDefined)
    {
        SemanticError(token, ctx, "Redefinition in the current scope");
        return 0;
    }
    
    return ret.res;
}

void PerformTypingStage(IR_Context* ctx, Ast_Root* ast)
{
    printf("Performing typing stage!\n");
    
    auto root = ast;
    LLVMValueRef genCode = 0;
    
    // First pass for function declarations
    for (int i = 0; i < root->topStmts.length; ++i)
    {
        auto stmt = root->topStmts[i];
        
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
        
        if (!genCode)
            return;
    }
    
    // Second pass for function definitions
    for (int i = 0; i < root->topStmts.length; ++i)
    {
        auto stmt = root->topStmts[i];
        
        if(stmt->type == Function)
        {
            ctx->scopeCounter = 0;
            ctx->curScopeIdx  = 0;
            TempArenaMemory savepoint = Arena_TempBegin(ctx->arena);
            defer(Arena_TempEnd(savepoint));
            ctx->curSymTable = Arena_AllocAndInit(ctx->arena, SymbolTable);
            
            auto funcDefinition = (Ast_Function*)stmt;
            
            Symbol* proto = GetGlobalSymbol(&ctx->globalSymTable, funcDefinition->prototype->token.ident);
            if(!proto)
            {
                fprintf(stderr, "Internal error: declaration not found during function definition\n");
                return;
            }
            
            //GenerateBody(ctx, funcDefinition, proto->address);
            ctx->curSymTable = 0;
        }
    }
    
    if (!genCode || ctx->errorOccurred)
        return;
    
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
}

void Semantics_Block(IR_Context* ctx, Ast_BlockStmt* block)
{
    //StartScope(ctx);
    
    for(Ast_BlockNode* it = block->first; it; it = it->next)
    {
        if(it->stmt->type == BlockStmt)
        {
            Semantics_Block(ctx, (Ast_BlockStmt*)it->stmt);
        }
        else
        {
            Semantics_Stmt(ctx, it->stmt);
        }
    }
}

void Semantics_Declaration(Ast_Declaration* decl)
{
    
}

void Semantics_Stmt(IR_Context* ctx, Ast_Stmt* stmt)
{
    switch(stmt->type)
    {
        default: Assert(false && "Not implemented yet!"); return;
        case EmptyStmt: break;
        case DeclStmt: Semantics_Declaration((Ast_Declaration*)stmt); break;
        case ExprStmt: TypeCheckExpr(((Ast_ExprStmt*)stmt)->expr); break;
        case BlockStmt: Semantics_Block(ctx, (Ast_BlockStmt*)stmt); break;
        case ReturnStmt: break;
    }
}

void TypeCheckExpr(Ast_ExprNode* expr)
{
    switch(expr->type)
    {
        default: Assert(false && "Not implemented yet!"); return;
        case BinNode:
        {
            
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
    }
}