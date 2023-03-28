
#include "semantics.h"

static void SemanticError(Token token, char* fileContents, char* message)
{
    CompilationError(token, fileContents, "Semantic error", message);
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
        SemanticError(token, ctx->fileContents, "Undeclared identifier");
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
        SemanticError(token, ctx->fileContents, "Undeclared identifier");
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

static Symbol* InsertGlobalSymbolOrError(IR_Context* ctx, Token token)
{
    InsertSymbol_Ret ret = InsertSymbol(ctx->arena, &ctx->globalSymTable, token.ident, 0);
    if(ret.alreadyDefined)
    {
        SemanticError(token, ctx->fileContents, "Redefinition");
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
        SemanticError(token, ctx->fileContents, "Redefinition in the current scope");
        return 0;
    }
    
    return ret.res;
}

