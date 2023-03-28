
#include "llvm_ir_generation.h"
#include "semantics.h"

static void StartScope(IR_Context* ctx);
static void EndScope(IR_Context* ctx);
// TODO: this will replace the other function later
static LLVMValueRef GenerateVar(IR_Context *ctx, Ast_Declaration* stmt, bool expectingRValue = false);
static LLVMValueRef GenerateVar(IR_Context *ctx, Ast_DeclStmt* stmt, bool expectingRValue = false);
static LLVMValueRef GenerateExpr(IR_Context* ctx, Ast_ExprNode* node, bool expectRValue = true);
static LLVMValueRef GenerateVarOrExpr(IR_Context* ctx, Ast_ExprOrDecl* node);
static LLVMValueRef GenerateCond(IR_Context* ctx, Ast_ExprOrDecl* node);
static LLVMValueRef GenerateProto(IR_Context* ctx, Ast_Prototype* proto);
static void GenerateBody(IR_Context* ctx, Ast_Function* func, LLVMValueRef proto);
static LLVMValueRef GenerateBinOp(IR_Context* ctx, Ast_BinExprNode* node);
static LLVMValueRef GenerateCall(IR_Context* ctx, Ast_CallExprNode* node);
// This can be called for new BasicBlocks, for example
// the statement(s) resulting from a condition. It will
// ignore nested blocks and just generate all the statements
// in order.
// Returns true if returned, false otherwise.
static bool GenerateBlock(IR_Context* ctx, Ast_Stmt* stmt);
static void GenerateSimpleStmt(IR_Context* ctx, Ast_Stmt* node);
static void GenerateIf(IR_Context* ctx, Ast_If* ifStmt);
static void GenerateFor(IR_Context* ctx, Ast_For* forStmt);
static void GenerateWhile(IR_Context* ctx, Ast_While* whileStmt);

static void StartScope(IR_Context* ctx)
{
    Arena_AllocVar(ctx->scopeStackArena, uint32);
    ++ctx->curScopeIdx;
    ++ctx->scopeCounter;
    ctx->curScopeStack[ctx->curScopeIdx] = ctx->scopeCounter;
}

static void EndScope(IR_Context* ctx)
{
    ctx->scopeStackArena->offset = (size_t)((&ctx->curScopeStack[ctx->curScopeIdx]) - ctx->scopeStackArena->offset);
    --ctx->curScopeIdx;
}

// Generates and executes Intermediate Representation
int GenerateIR(IR_Context *ctx, Ast_Root *root)
{
    ProfileFunc();
    
    if (!ctx || !root)
        return -1;
    
    LLVMValueRef genCode = 0;
    
    // First pass for function declarations
    for (int i = 0; i < root->topStmts.length; ++i)
    {
        printf("prova %lld\n", root->topStmts.length);
        auto stmt = root->topStmts[i];
        
        if (!stmt)
            return -2;
        
        if(stmt->type == Function)
        {
            auto funcDefinition = (Ast_Function*)stmt;
            genCode = GenerateProto(ctx, funcDefinition->prototype);
        }
        else if(stmt->type == Prototype)
            genCode = GenerateProto(ctx, (Ast_Prototype*)stmt);
        else
            return -3;
        
        if (!genCode)
            return -4;
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
                return -5;
            }
            
            GenerateBody(ctx, funcDefinition, proto->address);
            ctx->curSymTable = 0;
        }
    }
    
    if (!genCode || ctx->errorOccurred)
        return -4;
    
    char* errorMessage = 0;
    if(LLVMVerifyModule(ctx->module, LLVMPrintMessageAction, &errorMessage) == 1)
        return -5;
    
    printf("IR generation was completed successfully.\n\n");
    
    // Generation of object file
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllAsmParsers();
    LLVMInitializeAllAsmPrinters();
    
    char* triple  = LLVMGetDefaultTargetTriple();
    LLVMTargetRef target = 0;
    LLVMGetTargetFromTriple(triple, &target, &errorMessage);
    if(!target)
    {
        printf(errorMessage);
        LLVMDisposeMessage(errorMessage);
        return -9;
    }
    
    char* CPU     = "generic";
    char* options = "";
    LLVMTargetMachineRef targetMachine = LLVMCreateTargetMachine(target, triple,
                                                                 "generic", "",
                                                                 LLVMCodeGenLevelNone,
                                                                 LLVMRelocDefault,
                                                                 LLVMCodeModelDefault);
    bool isError = LLVMTargetMachineEmitToFile(targetMachine, ctx->module,
                                               "output.o", LLVMObjectFile,
                                               &errorMessage);
    if(isError)
    {
        printf(errorMessage);
        LLVMDisposeMessage(errorMessage);
        return -10;
    }
    
    printf("Object file successfully generated.\n");
    
    // Execution of generated code
    String mainString = { "main", 4 };
    Symbol* mainProc = GetGlobalSymbol(&ctx->globalSymTable, mainString);
    if(!mainProc)
        fprintf(stderr, "Could not find main() function, so the generated code will not be executed.\n");
    else
    {
        errorMessage = 0;
        
        LLVMLinkInMCJIT();
        LLVMExecutionEngineRef engine;
        if(LLVMCreateExecutionEngineForModule(&engine, ctx->module, &errorMessage) != 0)
        {
            fprintf(stderr, "Runtime error: failed to create execution engine\n");
            LLVMDisposeMessage(errorMessage);
            return -6;
        }
        
        printf("Executing code...\n");
        
        LLVMGenericValueRef res = LLVMRunFunction(engine, mainProc->address, 0, 0);
        printf("\nMain return value: %f\n", (double)LLVMGenericValueToFloat(LLVMDoubleType(), res));
    }
    
    return 0;
} 

IR_Context InitIRContext(Arena* arena, Arena* tempArena, Arena* scopeStackArena, char* fileContents)
{
    IR_Context ctx;
    ctx.module = LLVMModuleCreateWithName("kal");
    ctx.builder = LLVMCreateBuilder();
    ctx.arena = arena;
    ctx.tempArena = tempArena;
    ctx.scopeStackArena = scopeStackArena;
    ctx.fileContents = fileContents;
    return ctx;
}

static LLVMTypeRef GetType(Token typeIdent, uint8 numPointers)
{
    if(typeIdent.type != TokenIdent)
        return 0;
    
    LLVMTypeRef result = 0;
    String ident = typeIdent.ident;
    
    // TODO: Can i do a switch statement?
    if(ident == "int")
        result = LLVMIntType(32);
    else if(ident == "float")
        result = LLVMFloatType();
    else if(ident == "double")
        result = LLVMDoubleType();
    else if(ident == "bool")
        result = LLVMIntType(8);
    
    for(int i = 0; i < numPointers; ++i)
        result = LLVMPointerType(result, 0);
    
    return result;
}

static LLVMValueRef GenerateVar(IR_Context* ctx, Ast_Declaration* stmt, bool expectingRValue)
{
    Symbol* argSym = InsertSymbolOrError(ctx, stmt->nameIdent);
    if(!argSym)
        return 0;
    
    auto type = GetType(stmt->typeInfo.ident, stmt->typeInfo.numPointers);
    if(!type)
        return 0;
    
    argSym->address = LLVMBuildAlloca(ctx->builder, type, "var");
    
    /*
    if(stmt->isPointer)
        argSym->address = LLVMBuildAlloca(ctx->builder, LLVMPointerType(LLVMDoubleType(), 0), "ptr");
    else
        argSym->address = LLVMBuildAlloca(ctx->builder, LLVMDoubleType(), "var");
    */
    
    if (stmt->initialization)
    {
        LLVMValueRef expr = GenerateExpr(ctx, stmt->initialization, expectingRValue);
        if(!expr)
            return 0;
        
        //if(expectingRValue)
        //return expr;
    }
    
    return argSym->address;
}

static LLVMValueRef GenerateVar(IR_Context *ctx, Ast_DeclStmt *stmt, bool expectingRValue)
{
    Symbol* argSym = InsertSymbolOrError(ctx, stmt->id);
    if(!argSym)
        return 0;
    
    if(stmt->isPointer)
        argSym->address = LLVMBuildAlloca(ctx->builder, LLVMPointerType(LLVMDoubleType(), 0), "ptr");
    else
        argSym->address = LLVMBuildAlloca(ctx->builder, LLVMDoubleType(), "var");
    
    if (stmt->expr)
    {
        LLVMValueRef expr = GenerateExpr(ctx, stmt->expr);
        if(!expr)
            return 0;
        
        if(stmt->expr->isLeftValue)
            expr = LLVMBuildLoad(ctx->builder, expr, "ldtmp");
        
        // Expression on the right should be the right type
        LLVMTypeRef exprType = LLVMTypeOf(expr);
        if(LLVMTypeOf(argSym->address) != LLVMPointerType(exprType, 0))
        {
            SemanticError(stmt->equal, ctx->fileContents, "Mismatching types");
            return 0;
        }
        
        LLVMBuildStore(ctx->builder, expr, argSym->address);
        
        if(expectingRValue)
            return expr;
    }
    
    return argSym->address;
}

static LLVMValueRef GenerateExpr(IR_Context* ctx, Ast_ExprNode* node, bool expectRValue)
{
    switch(node->type)
    {
        case IdentNode:
        {
            Symbol* sym = GetSymbolOrError(ctx, node->token);
            if(!sym)
                return 0;
            
            if(expectRValue)
                return LLVMBuildLoad(ctx->builder, sym->address, "tmpld");
            
            node->isLeftValue = true;
            return sym->address;
        }
        case SubscriptNode:
        {
            Symbol* sym = GetSymbolOrError(ctx, node->token);
            if(!sym)
                return 0;
            
            // If not a pointer
            if(LLVMTypeOf(sym->address) != LLVMPointerType(LLVMPointerType(LLVMDoubleType(), 0), 0))
            {
                SemanticError(node->token, ctx->fileContents, "Incorrect type, expecting pointer");
                ctx->errorOccurred = true;
                return 0;
            }
            
            auto subscript = (Ast_SubscriptExprNode*)node;
            LLVMValueRef idxValue = GenerateExpr(ctx, subscript->indexExpr);
            if(!idxValue)
                return 0;
            
            if(LLVMTypeOf(idxValue) != LLVMDoubleType())
            {
                SemanticError(subscript->indexExpr->token, ctx->fileContents, "Incorrect type, expecting double");
                ctx->errorOccurred = true;
                return 0;
            }
            
            idxValue = LLVMBuildCast(ctx->builder, LLVMFPToSI, idxValue, LLVMIntType(32), "cast");
            LLVMValueRef ptrValue = LLVMBuildLoad(ctx->builder, sym->address, "ldtmp");
            LLVMValueRef offset = LLVMBuildGEP2(ctx->builder, LLVMDoubleType(), ptrValue, &idxValue, 1, "idxptr");
            
            if(!expectRValue)
            {
                node->isLeftValue = true;
                return offset;
            }
            
            offset = LLVMBuildLoad(ctx->builder, offset, "ldtmp");
            return LLVMBuildCast(ctx->builder, LLVMSIToFP, offset, LLVMDoubleType(), "cast");
        }
        case NumNode:
        {
            return LLVMConstReal(LLVMDoubleType(), node->token.doubleValue);
        }
        case BinNode:
        {
            return GenerateBinOp(ctx, (Ast_BinExprNode*)node);
        }
        case UnaryNode:
        {
            auto unaryNode = (Ast_UnaryExprNode*)node;
            LLVMValueRef value = GenerateExpr(ctx, unaryNode->expr, true);
            if(!value)
                return 0;
            
            if(LLVMTypeOf(value) != LLVMDoubleType())
            {
                SemanticError(unaryNode->token, ctx->fileContents, "Mismatching type, expecting double");
                ctx->errorOccurred = true;
                return 0;
            }
            
            switch(unaryNode->token.type)
            {
                default:
                {
                    fprintf(stderr, "Internal error: Unknown unary operator\n");
                    return 0;
                }
                case '-':
                return LLVMBuildFSub(ctx->builder, LLVMConstReal(LLVMDoubleType(), 0), value, "tmp");
                case '+':
                return value;
            }
        }
        case CallNode:
        {
            return GenerateCall(ctx, (Ast_CallExprNode*)node);
        }
        case ArrayInitNode:
        {
            auto arrayNode = (Ast_ArrayInitNode*)node;
            LLVMValueRef ptrToArray = LLVMBuildAlloca(ctx->builder,
                                                      LLVMArrayType(LLVMDoubleType(),
                                                                    arrayNode->expressions.length), "array");
            LLVMValueRef firstElementAddress = 0;
            for(int i = 0; i < arrayNode->expressions.length; ++i)
            {
                LLVMValueRef value = GenerateExpr(ctx, arrayNode->expressions[i]);
                if(!value)
                    return 0;
                
                if(LLVMTypeOf(value) != LLVMDoubleType())
                {
                    SemanticError(arrayNode->expressions[i]->token, ctx->fileContents, "Incorrect type, expected double");
                    ctx->errorOccurred = true;
                    return 0;
                }
                
                LLVMValueRef idx = LLVMConstInt(LLVMIntType(32), i, true);
                LLVMValueRef offset = LLVMBuildGEP2(ctx->builder, LLVMDoubleType(), ptrToArray, &idx, 1, "idxptr");
                
                if(i == 0)
                    firstElementAddress = offset;
                
                LLVMBuildStore(ctx->builder, value, offset);
            }
            
            return firstElementAddress;
        }
        default:
        {
            Assert(false && "Not implemented!");
            return 0;
        }
    }
    
    return 0;
}

static LLVMValueRef GenerateVarOrExpr(IR_Context* ctx, Ast_ExprOrDecl* node)
{
    LLVMValueRef value = 0;
    if(node->isDecl)
        value = GenerateVar(ctx, node->decl, true);
    else
        value = GenerateExpr(ctx, node->expr, true);
    return value;
}

// Used when expecting a condition (such as in if/while statements, and for conditions
static LLVMValueRef GenerateCond(IR_Context* ctx, Ast_ExprOrDecl* node)
{
    LLVMValueRef cond = GenerateVarOrExpr(ctx, node);
    if(!cond)
        return 0;
    
    LLVMTypeRef condType = LLVMTypeOf(cond);
    // Implicit conversion
    if(condType == LLVMDoubleType())
        cond = LLVMBuildFCmp(ctx->builder, LLVMRealUNE, cond, LLVMConstReal(LLVMDoubleType(), 0), "cmp");
    else if(condType != LLVMIntType(1))
    {
        Token token = node->isDecl? node->decl->id : node->expr->token;
        SemanticError(token, ctx->fileContents, "Incorrect type, expecting bool");
        return 0;
    }
    
    return cond;
}

static LLVMValueRef GenerateProto(IR_Context* ctx, Ast_Prototype* proto)
{
    auto types = (LLVMTypeRef*)malloc(sizeof(LLVMTypeRef)*proto->args.length);
    defer(free(types));
    
    for(int i = 0; i < proto->args.length; ++i)
    {
        if(proto->args[i].isPointer)
            types[i] = LLVMPointerType(LLVMDoubleType(), 0);
        else
            types[i] = LLVMDoubleType();
    }
    
    LLVMTypeRef functionType = LLVMFunctionType(LLVMDoubleType(),
                                                types,
                                                proto->args.length,
                                                false);
    
    Symbol* symbol = InsertGlobalSymbolOrError(ctx, proto->token);
    if(!symbol)
        return 0;
    
    // NOTE(Leo): This is needed because the string needs to be
    // null-terminated for LLVM-C API functions.
    char* toCopy = proto->token.ident.ptr;
    int length = proto->token.ident.length;
    char* nullTerminatedStr = (char*)malloc(length+1);
    defer(free(nullTerminatedStr));
    
    memcpy(nullTerminatedStr, toCopy, length);
    nullTerminatedStr[length] = '\0';
    
    LLVMValueRef result = LLVMAddFunction(ctx->module,
                                          nullTerminatedStr,
                                          functionType);
    symbol->address = result;
    
    // Print the generated code
    char* toPrint = LLVMPrintValueToString(result);
    printf("%s", toPrint);
    LLVMDisposeMessage(toPrint);
    
    return result;
}

static void GenerateBody(IR_Context* ctx, Ast_Function* func, LLVMValueRef proto)
{
    ctx->currentFunction = proto;
    
    TempArenaMemory savepoint = Arena_TempBegin(ctx->scopeStackArena);
    ctx->curScopeStack = Arena_AllocVar(ctx->scopeStackArena, uint32);
    ctx->curScopeStack[0] = 0;
    defer(Arena_TempEnd(savepoint));
    
    // Create basic block
    LLVMBasicBlockRef block = LLVMAppendBasicBlock(proto, "entry");
    LLVMPositionBuilderAtEnd(ctx->builder, block);
    
    for(int i = 0; i < func->prototype->args.length; ++i)
    {
        Symbol* argSym = InsertSymbolOrError(ctx, func->prototype->args[i].id);
        if(!argSym)
        {
            ctx->errorOccurred = true;
            return;
        }
        
        if(func->prototype->args[i].isPointer)
            argSym->address = LLVMBuildAlloca(ctx->builder, LLVMPointerType(LLVMDoubleType(), 0), "param");
        else
            argSym->address = LLVMBuildAlloca(ctx->builder, LLVMDoubleType(), "param");
        LLVMBuildStore(ctx->builder, LLVMGetParam(proto, i), argSym->address);
    }
    
    // Generate body
    bool returned = GenerateBlock(ctx, func->body);
    if(ctx->errorOccurred)
        return;
    
    if(!returned)
        LLVMBuildRet(ctx->builder,
                     LLVMConstReal(LLVMDoubleType(), 0.0f));
    
    // Print the generated code
    char* toPrint = LLVMPrintValueToString(proto);
    printf("%s\n", toPrint);
    LLVMDisposeMessage(toPrint);
}

static LLVMValueRef GenerateBinOp(IR_Context* ctx, Ast_BinExprNode* node)
{
    bool isEqualOp = (node->token.type == '=');
    LLVMValueRef left  = GenerateExpr(ctx, node->left, !isEqualOp);
    LLVMValueRef right = GenerateExpr(ctx, node->right, true);
    if(!left || !right)
        return 0;
    
    // Besides '=', all other operations only involve double types
    if(!isEqualOp && ((LLVMTypeOf(left) != LLVMDoubleType()) || (LLVMTypeOf(right) != LLVMDoubleType())))
    {
        SemanticError(node->token, ctx->fileContents, "Incorrect types");
        ctx->errorOccurred = true;
        return 0;
    }
    
    switch(node->token.type)
    {
        case '+':
        return LLVMBuildFAdd(ctx->builder, left, right, "addtmp");
        case '-':
        return LLVMBuildFSub(ctx->builder, left, right, "subtmp");
        case '*':
        return LLVMBuildFMul(ctx->builder, left, right, "multmp");
        case '/':
        return LLVMBuildFDiv(ctx->builder, left, right, "divtmp");
        case TokenLE:
        {
            return LLVMBuildFCmp(ctx->builder, LLVMRealULE,
                                 left, right, "cmptmp");
        }
        case TokenGE:
        {
            return LLVMBuildFCmp(ctx->builder, LLVMRealUGE,
                                 left, right, "cmptmp");
        }
        case TokenEQ:
        {
            return LLVMBuildFCmp(ctx->builder, LLVMRealUEQ,
                                 left, right, "cmptmp");
        }
        case TokenNEQ:
        {
            return LLVMBuildFCmp(ctx->builder, LLVMRealUNE,
                                 left, right, "cmptmp");
        }
        case '<':
        {
            return LLVMBuildFCmp(ctx->builder, LLVMRealULT,
                                 left, right, "cmptmp");
        }
        case '>':
        {
            return LLVMBuildFCmp(ctx->builder, LLVMRealUGT,
                                 left, right, "cmptmp");
        }
        case '=':
        {
            // Left must be lvalue
            if(!node->left->isLeftValue)
            {
                SemanticError(node->token, ctx->fileContents, "lhs of '=' must be lvalue");
                return 0;
            }
            
            if(node->right->isLeftValue)
                right = LLVMBuildLoad(ctx->builder, right, "ldtmp");
            
            // lhs must be of type "pointer to type of rhs"
            LLVMTypeRef rightType = LLVMTypeOf(right);
            if(LLVMTypeOf(left) != LLVMPointerType(rightType, 0))
            {
                SemanticError(node->token, ctx->fileContents, "Mismatching types");
                return 0;
            }
            
            node->isLeftValue = true;
            LLVMBuildStore(ctx->builder, right, left);
            return left;
        }
        default:
        Assert(false && "Not implemented!");
        return 0;
    }
    
    return 0;
}

static LLVMValueRef GenerateCall(IR_Context* ctx, Ast_CallExprNode* node)
{
    auto values = (LLVMValueRef*)malloc(sizeof(LLVMValueRef) *
                                        node->args.length);
    defer(free(values));
    
    for(int i = 0; i < node->args.length; ++i)
    {
        values[i] = GenerateExpr(ctx, node->args[i]);
        if(!values[i])
            return 0;
    }
    
    Symbol* symbol = GetGlobalSymbolOrError(ctx, node->token);
    if(!symbol)
        return 0;
    
    // Check parameter count and types
    LLVMValueRef curParam = LLVMGetFirstParam(symbol->address);
    int count = 0;
    while(curParam && count < node->args.length)
    {
        if(LLVMTypeOf(curParam) != LLVMTypeOf(values[count]))
        {
            SemanticError(node->token, ctx->fileContents, "Parameter types do not match function signature");
            return 0;
        }
        
        curParam = LLVMGetNextParam(curParam);
        ++count;
    }
    
    if(count != node->args.length || curParam)
    {
        SemanticError(node->token, ctx->fileContents, "Incorrect number of parameters in function call");
        return 0;
    }
    
    LLVMValueRef result = LLVMBuildCall(ctx->builder,
                                        symbol->address,
                                        values,
                                        node->args.length,
                                        "tmp");
    return result;
}

// TODO(Leo): This needs a major refactor
static bool GenerateBlock(IR_Context* ctx, Ast_Stmt* stmt)
{
    if(stmt->type != BlockStmt)
    {
        GenerateSimpleStmt(ctx, stmt);
        if(stmt->type == ReturnStmt)
            return true;
        return false;
    }
    
    // Statement is a block
    auto block = (Ast_BlockStmt*) stmt;
    
    TempArenaMemory savepoint = Arena_TempBegin(ctx->tempArena);
    defer(Arena_TempEnd(savepoint));
    
    auto nodes = Arena_AllocVar(ctx->tempArena, Ast_BlockNode*);
    int curNode = 0;
    nodes[0] = block->first;
    
    // TODO(Leo): This part can be confusing, so it must be cleaned-up
    // NOTE(Leo): for consecutive blocks, such
    // as '{ { int a; { int b; } } }', unless those are a
    // consequence of if/for/while statements we don't want
    // to create a new block for each, just generate the
    // statements in order and put them in a single BasicBlock.
    while(curNode >= 0)
    {
        while(nodes[curNode])
        {
            if(nodes[curNode]->stmt->type != BlockStmt)
            {
                GenerateSimpleStmt(ctx, nodes[curNode]->stmt);
                
                if(nodes[curNode]->stmt->type == ReturnStmt)
                {
                    // Skip every other statement
                    return true;
                }
                
                nodes[curNode] = nodes[curNode]->next;
            }
            else
            {
                StartScope(ctx);
                
                // "Push" on the stack
                auto blockNode = (Ast_BlockStmt*) nodes[curNode]->stmt;
                
                nodes[curNode] = nodes[curNode]->next;
                Arena_AllocVar(ctx->tempArena, Ast_BlockNode*);
                ++curNode;
                nodes[curNode] = blockNode->first;
            }
        }
        
        if(curNode > 0)
            EndScope(ctx);
        
        // "Pop" the stack
        ctx->tempArena->offset = size_t((uchar*)(&nodes[curNode]) - ctx->tempArena->buffer);
        --curNode;
    }
    
    return false;
}

static void GenerateSimpleStmt(IR_Context *ctx, Ast_Stmt *stmt)
{
    Assert(stmt->type != BlockStmt);
    
    switch (stmt->type)
    {
        case EmptyStmt:
        break;
        
        case DeclStmt:
        {
            if(!GenerateVar(ctx, (Ast_Declaration*)stmt))
                ctx->errorOccurred = true;
        }
        break;
        case ExprStmt:
        {
            auto exprStmt = (Ast_ExprStmt*)stmt;
            if(!GenerateExpr(ctx, exprStmt->expr))
                ctx->errorOccurred = true;
        }
        break;
        case ReturnStmt:
        {
            auto returnStmt = (Ast_ReturnStmt*)stmt;
            auto expr = GenerateExpr(ctx, returnStmt->returnValue);
            if(!expr)
            {
                ctx->errorOccurred = true;
                return;
            }
            
            // Can only return double
            if(LLVMTypeOf(expr) != LLVMDoubleType())
            {
                SemanticError(returnStmt->returnValue->token, ctx->fileContents, "Incorrect type, expecting double");
                ctx->errorOccurred = true;
                return;
            }
            
            LLVMBuildRet(ctx->builder, expr);
        }
        break;
        case IfStmt:
        {
            GenerateIf(ctx, (Ast_If*)stmt);
        }
        break;
        case ForStmt:
        {
            GenerateFor(ctx, (Ast_For*)stmt);
        }
        break;
        case WhileStmt:
        {
            GenerateWhile(ctx, (Ast_While*)stmt);
        }
        break;
        default:
        Assert(false && "Not implemented!");
        break;
    }
}

static void GenerateIf(IR_Context* ctx, Ast_If* ifStmt)
{
    StartScope(ctx);
    defer(EndScope(ctx));
    
    LLVMValueRef cond = GenerateCond(ctx, ifStmt->cond);
    if(!cond)
    {
        ctx->errorOccurred = true;
        return;
    }
    
    // Insert the jump instruction
    LLVMBasicBlockRef thenBlock = LLVMAppendBasicBlock(ctx->currentFunction, "then");
    LLVMBasicBlockRef endBlock  = LLVMAppendBasicBlock(ctx->currentFunction, "end");
    LLVMBasicBlockRef elseBlock = 0;
    if(ifStmt->falseStmt)
    {
        elseBlock = LLVMAppendBasicBlock(ctx->currentFunction, "else");
        LLVMBuildCondBr(ctx->builder, cond, thenBlock, elseBlock);
    }
    else
        LLVMBuildCondBr(ctx->builder, cond, thenBlock, endBlock);
    
    // Beginning of 'if' basic block
    LLVMPositionBuilderAtEnd(ctx->builder, thenBlock);
    bool returned = GenerateBlock(ctx, ifStmt->trueStmt);
    if(!returned)
        LLVMBuildBr(ctx->builder, endBlock);
    
    // Beginning of 'else' basic block
    if(ifStmt->falseStmt)
    {
        Assert(elseBlock);
        
        LLVMPositionBuilderAtEnd(ctx->builder, elseBlock);
        bool returned = GenerateBlock(ctx, ifStmt->falseStmt);
        if(!returned)
            LLVMBuildBr(ctx->builder, endBlock);
    }
    
    // After if statement
    LLVMPositionBuilderAtEnd(ctx->builder, endBlock);
}

static void GenerateFor(IR_Context* ctx, Ast_For* forStmt)
{
    StartScope(ctx);
    defer(EndScope(ctx));
    
    // First instruction
    if(forStmt->init)
    {
        if(!GenerateVarOrExpr(ctx, forStmt->init))
        {
            ctx->errorOccurred = true;
            return;
        }
    }
    
    LLVMBasicBlockRef condBlock;
    LLVMBasicBlockRef thenBlock;
    if(forStmt->cond)
    {
        condBlock = LLVMAppendBasicBlock(ctx->currentFunction, "cond");
        thenBlock = LLVMAppendBasicBlock(ctx->currentFunction, "loop");
    }
    else
    {
        thenBlock = LLVMAppendBasicBlock(ctx->currentFunction, "loop");
        condBlock = thenBlock;
    }
    
    LLVMBasicBlockRef endBlock = LLVMAppendBasicBlock(ctx->currentFunction, "end");
    
    LLVMBuildBr(ctx->builder, condBlock);
    
    if(forStmt->cond)
    {
        LLVMPositionBuilderAtEnd(ctx->builder, condBlock);
        LLVMValueRef condValue = GenerateCond(ctx, forStmt->cond);
        if(!condValue)
        {
            ctx->errorOccurred = true;
            return;
        }
        
        LLVMBuildCondBr(ctx->builder, condValue, thenBlock, endBlock);
    }
    
    LLVMPositionBuilderAtEnd(ctx->builder, thenBlock);
    
    bool returned = GenerateBlock(ctx, forStmt->execStmt);
    if(!returned)
    {
        // Loop instruction
        if(forStmt->iteration)
            GenerateExpr(ctx, forStmt->iteration);
        
        LLVMBuildBr(ctx->builder, condBlock);
    }
    
    // After for statement
    LLVMPositionBuilderAtEnd(ctx->builder, endBlock);
}

static void GenerateWhile(IR_Context* ctx, Ast_While* node)
{
    StartScope(ctx);
    defer(EndScope(ctx));
    
    LLVMBasicBlockRef loopBlock = LLVMAppendBasicBlock(ctx->currentFunction, "loop");
    LLVMBasicBlockRef endBlock  = LLVMAppendBasicBlock(ctx->currentFunction, "end");
    
    // Condition for first entering into the loop
    LLVMValueRef condValue = GenerateCond(ctx, node->cond);
    if(!condValue)
    {
        ctx->errorOccurred = true;
        return;
    }
    
    LLVMBuildCondBr(ctx->builder, condValue, loopBlock, endBlock);
    
    LLVMPositionBuilderAtEnd(ctx->builder, loopBlock);
    bool returned = GenerateBlock(ctx, node->execStmt);
    if(!returned)
    {
        LLVMValueRef condValue = GenerateVarOrExpr(ctx, node->cond);
        LLVMBuildCondBr(ctx->builder, condValue, loopBlock, endBlock);
    }
    
    // After for statement
    LLVMPositionBuilderAtEnd(ctx->builder, endBlock);
}
