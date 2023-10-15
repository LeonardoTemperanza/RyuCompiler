
#include "base.h"
#include "lexer.h"
#include "ast.h"
#include "dependency_graph.h"

#include "semantics.h"

// TODO TODO: missing compute size for procedures.

// Primitive types

Typer InitTyper(Arena* arena, Tokenizer* tokenizer)
{
    Typer t;
    t.tokenizer = tokenizer;
    t.arena = arena;
    return t;
}

void ResetTyper(Typer* t)
{
    t->curScope = &t->fileScope->scope;
    t->currentProc = 0;
    t->checkedReturnStmt = false;
    t->inLoopBlock = false;
    t->inSwitchBlock = false;
    t->inDeferBlock = false;
    t->status = true;
}

Ast_PtrType* Typer_MakePtr(Arena* arena, TypeInfo* base)
{
    auto ptr = Arena_AllocAndInitPack(arena, Ast_PtrType);
    ptr->baseType = base;
    return ptr;
}

bool IsNodeConst(Typer* t, Ast_Node* node)
{
    if(node->kind == AstKind_Ident)
    {
        // TODO: Check for enum values and #comp
        return false;
    }
    
    switch_nocheck(node->kind)
    {
        case AstKind_ConstValue: return true;
        case AstKind_RunDir:     return true;
    }switch_nocheck_end;
    
    return false;
}

bool CheckNode(Typer* t, Ast_Node* node)
{
    bool outcome = true;
    
    switch(node->kind)
    {
        case AstKind_ProcDecl:      outcome = CheckProcDecl(t, (Ast_ProcDecl*)node); break;
        case AstKind_ProcDef:       outcome = CheckProcDef(t, (Ast_ProcDef*)node); break;
        case AstKind_StructDef:     outcome = CheckStructDef(t, (Ast_StructDef*)node); break;
        case AstKind_DeclBegin:     break;
        case AstKind_VarDecl:       outcome = CheckVarDecl(t, (Ast_VarDecl*)node); break;
        case AstKind_EnumDecl:      printf("Enumerator not implemented.\n"); outcome = false; break;
        case AstKind_DeclEnd:       break;
        case AstKind_StmtBegin:     break;
        case AstKind_Block:         outcome = CheckBlock(t, (Ast_Block*)node); break;
        case AstKind_If:            outcome = CheckIf(t, (Ast_If*)node); break;
        case AstKind_For:           outcome = CheckFor(t, (Ast_For*)node); break;
        case AstKind_While:         outcome = CheckWhile(t, (Ast_While*)node); break;
        case AstKind_DoWhile:       outcome = CheckDoWhile(t, (Ast_DoWhile*)node); break;
        case AstKind_Switch:        outcome = CheckSwitch(t, (Ast_Switch*)node); break;
        case AstKind_Defer:         outcome = CheckDefer(t, (Ast_Defer*)node); break;
        case AstKind_Return:        outcome = CheckReturn(t, (Ast_Return*)node); break;
        case AstKind_Break:         outcome = CheckBreak(t, (Ast_Break*)node); break;
        case AstKind_Continue:      outcome = CheckContinue(t, (Ast_Continue*)node); break;
        case AstKind_Fallthrough:   outcome = CheckFallthrough(t, (Ast_Fallthrough*)node); break;
        case AstKind_MultiAssign:   outcome = CheckMultiAssign(t, (Ast_MultiAssign*)node); break;
        case AstKind_EmptyStmt:     break;
        case AstKind_StmtEnd:       break;
        case AstKind_ExprBegin:     break;
        case AstKind_Ident:         outcome = CheckIdent(t, (Ast_IdentExpr*)node); break;
        case AstKind_FuncCall:      outcome = CheckFuncCall(t, (Ast_FuncCall*)node, false); break;
        case AstKind_BinaryExpr:    outcome = CheckBinExpr(t, (Ast_BinaryExpr*)node); break;
        case AstKind_UnaryExpr:     outcome = CheckUnaryExpr(t, (Ast_UnaryExpr*)node); break;
        case AstKind_TernaryExpr:   printf("Ternary expression not implemented.\n"); outcome = false; break;
        case AstKind_Typecast:      outcome = CheckTypecast(t, (Ast_Typecast*)node); break;
        case AstKind_Subscript:     outcome = CheckSubscript(t, (Ast_Subscript*)node); break;
        case AstKind_MemberAccess:  outcome = CheckMemberAccess(t, (Ast_MemberAccess*)node); break;
        case AstKind_ConstValue:    outcome = CheckConstValue(t, (Ast_ConstValue*)node); break;
        case AstKind_ExprEnd:       break;
        case AstKind_RunDir:        Assert(false && "Run directive semantics not implemented"); break;
        default: Assert(false && "Enum value out of bounds");
    }
    
    if(Ast_IsExpr(node))
    {
        auto expr = (Ast_Expr*)node;
        expr->castType = expr->type;
    }
    
    return outcome;
}

bool CheckProcDecl(Typer* t, Ast_ProcDecl* decl)
{
    // TODO: Ok, this is getting ridiculous. Types
    // should just have a Token* I think
    
    auto procType = Ast_GetProcType(decl);
    
    // Look up return types
    if(procType->retTypes.length > 0)
    {
        if(!CheckType(t, procType->retTypes[0], decl->where))
            return false;
    }
    
    // Look up argument types
    for(int i = 0; i < procType->args.length; ++i)
    {
        if(!CheckType(t, procType->args[i]->type, decl->where))
            return false;
    }
    
    if(!CheckNotAlreadyDeclared(t, t->curScope, decl)) return false;
    
    for_array(i, procType->args)
    {
        for(int j = 0; j < i; ++j)
        {
            if(procType->args[i]->name == procType->args[j]->name)
            {
                SemanticError(t, procType->args[i]->where, StrLit("Found duplicate argument name: this argument has the same name..."));
                SemanticErrorContinue(t, procType->args[j]->where, StrLit("... as this one."));
                return false;
            }
        }
    }
    
    return true;
}

bool CheckProcDef(Typer* t, Ast_ProcDef* proc)
{
    t->currentProc = proc;
    t->checkedReturnStmt = false;
    
    if(!NodePassedStage(proc->decl, CompPhase_Typecheck))
    {
        Dg_Yield(t->graph, proc->decl, CompPhase_Typecheck);
        return false;
    }
    
    // Definition
    if(!CheckBlock(t, &proc->block)) return false;
    
    auto retTypes = Ast_GetProcDefType(proc)->retTypes;
    if(retTypes.length > 0 && !t->checkedReturnStmt)
    {
        SemanticError(t, t->currentProc->where, StrLit("Procedure has a return type but no return statement was found."));
        return false;
    }
    
    return true;
}

bool CheckStructDef(Typer* t, Ast_StructDef* structDef)
{
    auto structType = Ast_GetStructType(structDef);
    for_array(i, structType->memberTypes)
    {
        if(!CheckType(t, structType->memberTypes[i], structType->memberNameTokens[i] - 1))
            return false;
    }
    
    return true;
}

bool CheckVarDecl(Typer* t, Ast_VarDecl* decl)
{
    if(!CheckType(t, decl->type, Ast_GetVarDeclTypeToken(decl))) return false;
    
    // TODO: check for array of pure proc types (not pointers)
    if(decl->type->typeId == Typeid_Proc)
    {
        SemanticError(t, decl->where, StrLit("Pure 'proc' type not allowed for variable declaration, use '^proc' instead"));
        return false;
    }
    
    if(!CheckNotAlreadyDeclared(t, t->curScope, decl)) return false;
    
    if(decl->initExpr)
    {
        if(!CheckNode(t, decl->initExpr)) return false;
        
        // Check that they're the same type here
        
        if(!ImplicitConversion(t, decl->initExpr, decl->initExpr->type, decl->type))
        {
            IncompatibleTypesError(t, decl->type, decl->initExpr->type, decl->where);
            return false;
        }
        
        decl->initExpr->castType = decl->type;
    }
    
    return true;
}

bool CheckBlock(Typer* t, Ast_Block* block)
{
    ProfileFunc(prof);
    
    auto prevScope = t->curScope;
    t->curScope = block;
    defer(t->curScope = prevScope);
    
    bool result = true;
    for(int i = 0; i < block->stmts.length && result; ++i)
    {
        Ast_Node* node = block->stmts[i];
        result &= CheckNode(t, node);
        if(!result) break;
    }
    
    return result;
}

bool CheckIf(Typer* t, Ast_If* stmt)
{
    auto prevScope = t->curScope;
    t->curScope = stmt->thenBlock;
    defer(t->curScope = prevScope);
    
    TypeInfo* condType = CheckCondition(t, stmt->condition);
    if(!condType) return false;
    
    if(condType)
    {
        if(!IsTypeScalar(condType))
            CannotConvertToScalarTypeError(t, condType, stmt->condition->where);
        else
        {
            if(!CheckBlock(t, stmt->thenBlock))
                return false;
            
            if(stmt->elseStmt)
            {
                // If the else statement contains a single declaration,
                // it will be added to this ficticious scope
                Ast_Block tmpBlock;
                tmpBlock.enclosing = t->curScope;
                t->curScope = &tmpBlock;
                defer(tmpBlock.decls.FreeAll());
                
                if(!CheckNode(t, stmt->elseStmt))
                    return false;
            }
        }
    }
    
    return true;
}

bool CheckFor(Typer* t, Ast_For* stmt)
{
    bool wasInLoopBlock = t->inLoopBlock;
    t->inLoopBlock = true;
    defer(t->inLoopBlock = wasInLoopBlock);
    
    auto prevScope = t->curScope;
    t->curScope = stmt->body;
    defer(t->curScope = prevScope);
    
    if(stmt->initialization && !CheckNode(t, stmt->initialization))
        return false;
    
    if(stmt->condition)
    {
        TypeInfo* condType = CheckCondition(t, stmt->condition);
        if(condType)
        {
            if(!IsTypeScalar(condType))
            {
                CannotConvertToScalarTypeError(t, condType, stmt->condition->where);
                return false;
            }
        }
    }
    
    if(!CheckNode(t, stmt->update)) return false;
    if(!CheckBlock(t, stmt->body)) return false;
    
    return true;
}

bool CheckWhile(Typer* t, Ast_While* stmt)
{
    ProfileFunc(prof);
    
    bool wasInLoopBlock = t->inLoopBlock;
    t->inLoopBlock = true;
    defer(t->inLoopBlock = wasInLoopBlock);
    
    auto prevScope = t->curScope;
    t->curScope = stmt->doBlock;
    defer(t->curScope = prevScope);
    
    TypeInfo* condType = CheckCondition(t, stmt->condition);
    if(!condType) return false;
    
    if(!IsTypeScalar(condType))
    {
        CannotConvertToScalarTypeError(t, condType, stmt->condition->where);
        return false;
    }
    
    if(!CheckBlock(t, stmt->doBlock))
        return false;
    
    return true;
}

bool CheckDoWhile(Typer* t, Ast_DoWhile* stmt)
{
    ProfileFunc(prof);
    
    bool wasInLoopBlock = t->inLoopBlock;
    t->inLoopBlock = true;
    defer(t->inLoopBlock = wasInLoopBlock);
    
    // If the do statement is a single declaration,
    // it will be added to this ficticious block
    Ast_Block tmpBlock;
    tmpBlock.enclosing = t->curScope;
    auto prevScope = t->curScope;
    t->curScope = &tmpBlock;
    defer(t->curScope = prevScope;
          tmpBlock.decls.FreeAll(););
    
    if(!CheckNode(t, stmt->doStmt))    return false;
    if(!CheckNode(t, stmt->condition)) return false;
    stmt->condition->castType = &Typer_Bool;
    
    TypeInfo* condType = stmt->condition->type;
    if(condType)
    {
        if(!IsTypeScalar(condType))
            CannotConvertToScalarTypeError(t, condType, stmt->condition->where);
    }
    
    return true;
}

bool CheckSwitch(Typer* t, Ast_Switch* stmt)
{
    ProfileFunc(prof);
    
    bool wasInSwitchBlock = t->inSwitchBlock;
    t->inSwitchBlock = true;
    defer(t->inSwitchBlock = wasInSwitchBlock);
    
    if(!CheckNode(t, stmt->switchExpr)) return false;
    
    if(stmt->switchExpr->type->typeId != Typeid_Integer)
    {
        SemanticError(t, stmt->switchExpr->where, StrLit("Switch condition should be of integer type, not '%T'."), stmt->switchExpr->type);
        return false;
    }
    
    for_array(i, stmt->cases)
    {
        auto caseExpr = stmt->cases[i];
        
        if(!CheckNode(t, caseExpr)) return false;
        
        if(!IsNodeConst(t, caseExpr))
        {
            SemanticError(t, caseExpr->where, StrLit("Switch case expressions are necessarily known at compile-time."));
            return false;
        }
        
        if(caseExpr->type->typeId != Typeid_Integer)
        {
            SemanticError(t, caseExpr->where, StrLit("Switch case expressions should be of integer type, not '%T'."),  caseExpr->type);
            return false;
        }
    }
    
    for_array(i, stmt->stmts)
    {
        auto caseStmt = stmt->stmts[i];
        if(!CheckNode(t, caseStmt)) return false;
    }
    
    return true;
}

bool CheckDefer(Typer* t, Ast_Defer* stmt)
{
    ProfileFunc(prof);
    
    if(t->inDeferBlock)
    {
        SemanticError(t, stmt->where, StrLit("Cannot use defer statements inside other defer statements."));
        return false;
    }
    
    t->inDeferBlock = true;
    defer(t->inDeferBlock = false);
    
    // If the defer statement contains a single declaration, it will
    // be added to this ficticious scope.
    Ast_Block tmpBlock;
    tmpBlock.enclosing = t->curScope;
    auto prevScope = t->curScope;
    t->curScope = &tmpBlock;
    defer(t->curScope = prevScope);
    if(!CheckNode(t, stmt->stmt)) return false;
    
    return true;
}

bool CheckReturn(Typer* t, Ast_Return* stmt)
{
    ProfileFunc(prof);
    
    if(t->inDeferBlock)
    {
        SemanticError(t, stmt->where, StrLit("Illegal return; can't use return in defer statements."));
        return false;
    }
    
    t->checkedReturnStmt = true;
    
    auto retStmt = (Ast_Return*)stmt;
    Slice<TypeInfo*> retTypes = Ast_GetProcDefType(t->currentProc)->retTypes;
    
    // Mismatching
    if(retTypes.length != retStmt->rets.length)
    {
        IncompatibleReturnsError(t, retStmt->where, retStmt->rets.length, retTypes.length);
        return false;
    }
    
    if(retStmt->rets.length > 0)
    {
        for_array(i, retStmt->rets)
        {
            if(!CheckNode(t, retStmt->rets[i])) return false;
            
            TypeInfo* exprType = retStmt->rets[i]->type;
            TypeInfo* retType  = retTypes[i];
            retStmt->rets[i]->castType = retType;
            
            if(!ImplicitConversion(t, retStmt->rets[i], exprType, retType))
            {
                // TODO: print a more specific error in place of this.
                IncompatibleTypesError(t, exprType, retType, retStmt->where);
                return false;
            }
        }
    }
    else  // Check if this function is supposed to have one or more ret types
    {
        if(retTypes.length > 0)
        {
            SemanticError(t, retStmt->where, StrLit("Return statement does not specify a value, ..."));
            SemanticErrorContinue(t, t->currentProc->where, StrLit("... but the procedure requires a return value."));
            return 0;
        }
    }
    
    return true;
}

bool CheckBreak(Typer* t, Ast_Break* stmt)
{
    if(!t->inLoopBlock && !t->inSwitchBlock)
    {
        SemanticError(t, stmt->where, StrLit("Illegal break; needs to be in a loop block or a switch block"));
        return false;
    }
    
    return true;
}

bool CheckContinue(Typer* t, Ast_Continue* stmt)
{
    if(!t->inLoopBlock)
    {
        SemanticError(t, stmt->where, StrLit("Illegal continue; needs to be in a loop block"));
        return false;
    }
    
    return true;
}

bool CheckFallthrough(Typer* t, Ast_Fallthrough* stmt)
{
    if(!t->inSwitchBlock)
    {
        SemanticError(t, stmt->where, StrLit("Illegal fallthrough; needs to be in a switch block"));
        return false;
    }
    
    return true;
}

bool CheckMultiAssign(Typer* t, Ast_MultiAssign* stmt)
{
    auto& lefts = stmt->lefts;
    auto& rights = stmt->rights;
    
    // Check all declarations
    ScratchArena scratch;
    Slice<TypeInfo*> leftTypes;
    for_array(i, lefts)
    {
        TypeInfo* type = CheckDeclOrExpr(t, lefts[i]);
        if(!type) return false;
        
        leftTypes.Append(scratch, type);
    }
    
    int leftCounter = 0;
    int rightCounter = 0;
    while(leftCounter < lefts.length && rightCounter < rights.length)
    {
        Slice<TypeInfo*> toCheck;  // > 1 if there's a function call with multiple return values, = 1 for anything else
        
        // Current rhs (which means, zero or more types due to multiple returns)
        if(stmt->rights[rightCounter]->kind == AstKind_FuncCall)
        {
            // Check against multiple statements
            auto call = (Ast_FuncCall*)stmt->rights[rightCounter];
            if(!CheckFuncCall(t, call, true)) return false;
            
            auto procDecl = (Ast_ProcType*)GetBaseType(call->target->type);
            toCheck = procDecl->retTypes;
        }
        else
        {
            if(!CheckNode(t, stmt->lefts[leftCounter])) return false;
            
            if(stmt->lefts[leftCounter]->kind == AstKind_VarDecl)
            {
                auto varDecl = (Ast_VarDecl*)stmt->lefts[leftCounter];
                toCheck.ptr = &varDecl->type;
            }
            else
            {
                auto expr = (Ast_Expr*)stmt->lefts[leftCounter];
                toCheck.ptr = &expr->type;
            }
            
            toCheck.length = 1;
        }
        
        ++rightCounter;
        
        // Check type compatibility between lhs and rhs
        bool exit = false;
        for(int i = 0; i < toCheck.length && leftCounter < lefts.length; ++i)
        {
            auto lhs = lefts[leftCounter];
            auto lhsType = leftTypes[leftCounter];
            
            if(!IsNodeLValue(lhs) || lhsType->typeId == Typeid_Proc)
            {
                SemanticError(t, lhs->where, StrLit("Left-hand side of assignment is not assignable"));
                return false;
            }
            else if(!TypesCompatible(t, lhsType, toCheck[i]))
            {
                IncompatibleTypesError(t, lhsType, toCheck[i], stmt->where);
                return false;
            }
            
            ++leftCounter;
        }
    }
    
    if(leftCounter != lefts.length || rightCounter != rights.length)
    {
        SemanticError(t, stmt->where, StrLit("Mismatching number of left-hand and right-hand side values (%d and %d, respectively)"), lefts.length, rightCounter);
        return false;
    }
    
    return true;
}

bool CheckConstValue(Typer* t, Ast_ConstValue* expr)
{
    return true;
}

Ast_Declaration* CheckIdent(Typer* t, Ast_IdentExpr* expr)
{
    ProfileFunc(prof);
    
    auto node = IdentResolution(t, t->curScope, expr->where, expr->name);
    if(!node)
    {
        SemanticError(t, expr->where, StrLit("Undeclared identifier."));
        return 0;
    }
    
    // Check the compPhase only if it's an independent entity (so, != Dg_Null)
    if(node->entityIdx != Dg_Null && !NodePassedStage(node, CompPhase_Typecheck))
    {
        Dg_Yield(t->graph, node, CompPhase_Typecheck);
        return 0;
    }
    
    auto decl = (Ast_Declaration*)node;
    expr->type = decl->type;
    
    auto identExpr = (Ast_IdentExpr*)expr;
    identExpr->declaration = decl;
    
    return decl;
}

// NOTE: Only considers "normal" function calls with single
// return value
bool CheckFuncCall(Typer* t, Ast_FuncCall* call, bool isMultiAssign)
{
    ProfileFunc(prof);
    
    if(!CheckNode(t, call->target)) return false;
    
    // Implicit dereference
    TypeInfo* callType = GetBaseTypeShallow(call->target->type);
    
    if(callType->typeId != Typeid_Proc)
    {
        SemanticError(t, call->where, StrLit("Cannot call target of type '%T'"), call->target->type);
        return false;
    }
    
    auto procType = (Ast_ProcType*)callType;
    
    if(t->currentProc)
    {
        // Functions with returns can be called without really using said
        // returns. This ensures that the size of the returns will be computed
        // before moving on to bytecode generation.
        for_array(i, procType->retTypes)
        {
            t->currentProc->toComputeSize.Append(procType->retTypes[i]);
        }
    }
    
    // If the number of return types is > 0, and we're not in a
    // multi assign statement, then throw an error because multiple
    // returns cannot be used in this context.
    // NOTE: Maybe rethink this language feature, so that it can be more
    // flexible?
    if(!isMultiAssign && procType->retTypes.length > 1)
    {
        SemanticError(t, call->where,
                      StrLit("Calling procedures with multiple return values in expression is not allowed. Use a MultiAssign statement instead."));
        return false;
    }
    
    // Check that number of parameters and types correspond
    // Support varargs in the future maybe?
    
    if(call->args.length != procType->args.length)
    {
        SemanticError(t, call->where, StrLit("Procedure does not take %d arguments"), call->args.length);
        return false;
    }
    
    int numArgs = call->args.length;
    for(int i = 0; i < numArgs; ++i)
    {
        if(!CheckNode(t, call->args[i]))
            return false;
        if(!ImplicitConversion(t, call->args[i], call->args[i]->type, procType->args[i]->type))
            return false;
        
        call->args[i]->castType = procType->args[i]->type;
    }
    
    if(procType->retTypes.length <= 0)
        call->type = &Typer_None;
    else
        call->type = procType->retTypes[0];
    
    return true;
}

bool CheckBinExpr(Typer* t, Ast_BinaryExpr* bin)
{
    ProfileFunc(prof);
    
    if(!CheckNode(t, bin->lhs)) return false;
    if(!CheckNode(t, bin->rhs)) return false;
    
    switch(bin->op)
    {
        // Arithmetic expressions
        default: Assert(false && "Case not handled"); return false;
        case '+': case '-':
        case '*': case '/':
        case '%':
        case Tok_LogAnd: case Tok_LogOr:
        case '^':
        case Tok_LShift: case Tok_RShift:
        {
            auto lhs = bin->lhs;
            auto rhs = bin->rhs;
            auto ltype = lhs->type;
            auto rtype = rhs->type;
            
            if((bin->op == '+' || bin->op == '-') &&
               (IsTypeDereferenceable(ltype) || IsTypeDereferenceable(rtype)))  // Pointer arithmetic
            {
                if(IsTypeDereferenceable(rtype))
                {
                    Swap(TypeInfo*, ltype, rtype);
                    Swap(Ast_Expr*, lhs, rhs);
                }
                
                if(IsTypeDereferenceable(rtype))
                {
                    if(bin->op == '-')  // Pointer difference is allowed, ...
                    {
                        if(!TypesIdentical(t, ltype, rtype))
                        {
                            SemanticError(t, bin->where,
                                          StrLit("Pointer difference disallowed for pointers of different base types ('%T' and '%T'"), ltype, rtype);
                            return false;
                        }
                        else
                            bin->type = ltype;
                    }
                    else  // ... unlike pointer addition
                    {
                        SemanticError(t, bin->where, StrLit("Pointer addition is not allowed, one must be integral"));
                        return false;
                    }
                }
                else  // Ptr + integral
                {
                    //lhs->castType = lhs->type;
                    //rhs->castType = &primitiveTypes[Typeid_Uint64];
                    
                    if(!IsTypeIntegral(rtype))
                    {
                        ScratchArena scratch;
                        String ltypeStr = TypeInfo2String(ltype, scratch);
                        String rtypeStr = TypeInfo2String(rtype, scratch);
                        StringBuilder b(scratch);
                        b.Append("Cannot apply binary operator to '");
                        b.Append(ltypeStr);
                        b.Append("' and '");
                        b.Append(rtypeStr);
                        b.Append("'");
                        SemanticError(t, bin->where, b.string);
                        return false;
                    }
                    
                    bin->type = lhs->type;
                }
                
                bin->type = lhs->type;
            }
            else  // Any other binary operation
            {
                if(!IsTypePrimitive(ltype) || !IsTypePrimitive(rtype))
                {
                    // NOTE: This is where you would wait for an operator to be overloaded
                    // Look for overloaded operators (they're procs with certain names) and
                    // yield if none are found.
                    SemanticError(t, bin->where, StrLit("Cannot apply binary operator to types '%T' and '%T'"), ltype, rtype);
                    return false;
                }
                
                // Bit-manipulation operators only work on integer types
                if(Ast_IsExprBitManipulation(bin))
                {
                    SemanticError(t, bin->where, StrLit("Cannot apply bit manipulation operators to types '%T' and '%T'"), ltype, rtype);
                    return false;
                    
                    // TODO: if it's not an integer, it's not allowed to do it.
                }
                else if(bin->op == '%' &&
                        (ltype->typeId == Typeid_Float || rtype->typeId == Typeid_Float))
                {
                    SemanticError(t, bin->where, StrLit("Cannot apply modulo operator to types '%T' and '%T'"), ltype, rtype);
                    return false;
                }
                
                // Logical operators should make the operands cast to bool
                if(bin->op == Tok_LogAnd || bin->op == Tok_LogOr)
                {
                    bin->lhs->castType = &Typer_Bool;
                    bin->rhs->castType = &Typer_Bool;
                    bin->type = &Typer_Bool;
                }
                else 
                {
                    // @cleanup Is this necessary? I forgot what GetCommonType even does
                    TypeInfo* commonType = GetCommonType(t, ltype, rtype);
                    bool conv1Success = ImplicitConversion(t, lhs, ltype, commonType);
                    bool conv2Success = ImplicitConversion(t, rhs, rtype, commonType);
                    if(!conv1Success || !conv2Success) return false;
                    
                    bin->lhs->castType = commonType;
                    bin->rhs->castType = commonType;
                    bin->type = commonType;
                }
            }
            
            break;
        }
        // Comparisons
        case Tok_EQ: case Tok_NEQ:
        case Tok_GE: case Tok_LE:
        case '>': case '<':
        {
            if(TypesCompatible(t, bin->lhs->type, bin->rhs->type))
            {
                TypeInfo* commonType = GetCommonType(t, bin->lhs->type, bin->rhs->type);
                bin->lhs->castType = commonType;
                bin->rhs->castType = commonType;
            }
            else
            {
                IncompatibleTypesError(t, bin->lhs->type, bin->rhs->type, bin->where);
                return false;
            }
            
            bin->type = &Typer_Bool;
            break;
        }
        // Assignments
        case '=': 
        case Tok_PlusEquals: case Tok_MinusEquals:
        case Tok_MulEquals: case Tok_DivEquals:
        case Tok_ModEquals: case Tok_AndEquals:
        case Tok_OrEquals: case Tok_XorEquals:
        case Tok_LShiftEquals: case Tok_RShiftEquals:
        {
            if(!IsNodeLValue(bin->lhs) || bin->lhs->type->typeId == Typeid_Proc)
            {
                SemanticError(t, bin->lhs->where, StrLit("Left-hand side of assignment is not assignable"));
                return false;
            }
            else if(!TypesCompatible(t, bin->lhs->type, bin->rhs->type))
            {
                IncompatibleTypesError(t, bin->lhs->type, bin->rhs->type, bin->where);
                return false;
            }
            
            bin->rhs->castType = bin->lhs->type;
            bin->type = bin->lhs->type;
            break;
        }
    }
    
    // NOTE: By default, keep the same type.
    // Change it if necessary in the above levels
    // of the call stack.
    bin->castType = bin->type;
    return true;
}

bool CheckUnaryExpr(Typer* t, Ast_UnaryExpr* expr)
{
    if(!CheckNode(t, expr->expr)) return false;
    
    switch(expr->op)
    {
        default: Assert(false && "Unsupported postfix operator"); return false;
        case Tok_Increment:
        case Tok_Decrement:
        {
            if(!IsNodeLValue(expr->expr))
            {
                SemanticError(t, expr->expr->where, StrLit("The expression needs to be an l-value to apply ++/-- operators."));
                return false;
            }
            
            if(!IsTypeIntegral(expr->expr->type))
            {
                CannotConvertToIntegralTypeError(t, expr->expr->type, expr->expr->where);
                return false;
            }
            
            expr->type = expr->expr->type;
            break;
        }
        case '+': case '-':
        {
            bool typeNotAllowed = !IsTypeNumeric(expr->expr->type);
            if(typeNotAllowed)
            {
                CannotConvertToIntegralTypeError(t, expr->expr->type, expr->expr->where);
                return false;
            }
            
            expr->type = expr->expr->type;
            break;
        }
        case '!':
        {
            // TODO: I don't think this is correct...
            if(!IsTypeScalar(expr->expr->type))
            {
                CannotConvertToScalarTypeError(t, expr->expr->type, expr->expr->where);
                return false;
            }
            
            // Logical operators should make the operands cast to bool
            expr->type = &Typer_Bool;
            expr->expr->castType = &Typer_Bool;
            break;
        }
        case '~':
        {
            if(!IsTypeIntegral(expr->expr->type))
            {
                return false;
            }
            
            expr->type = expr->expr->type;
            break;
        }
        case '*':
        {
            if(!IsTypeDereferenceable(expr->expr->type))
            {
                CannotDereferenceTypeError(t, expr->expr->type, expr->expr->where);
                return false;
            }
            
            expr->type = GetBaseTypeShallow(expr->expr->type);
            if(t->currentProc)
            {
                t->currentProc->toComputeSize.Append(expr->type);
            }
            break;
        }
        case '&':
        {
            if(!IsNodeLValue(expr->expr))
            {
                SemanticError(t, expr->expr->where, StrLit("Cannot use operator '&' on r-value"));
                return false;
            }
            
            expr->type = Typer_MakePtr(t->arena, expr->expr->type);
            break;
        }
    }
    
    expr->castType = expr->type;
    return true;
}

bool CheckTypecast(Typer* t, Ast_Typecast* expr)
{
    ProfileFunc(prof);
    
    // expr->type is already set by the parser,
    // because under no circumstance does the cast
    // expression change type from the one that's
    // specified by the user.
    if(!CheckType(t, expr->type, Ast_GetTypecastTypeToken(expr))) return false;
    if(!CheckNode(t, expr->expr)) return false;
    
    // TODO: Warnings for some casts
    
    // Implicitly convert arrays to pointers
    if(expr->expr->type->typeId == Typeid_Arr)
    {
        auto base = ((Ast_ArrType*)expr->expr)->baseType;
        expr->expr->type = Typer_MakePtr(t->arena, base);
    }
    
    if(expr->type->typeId == Typeid_Arr)
    {
        auto base = ((Ast_ArrType*)expr)->baseType;
        expr->type = Typer_MakePtr(t->arena, base);
    }
    
    TypeInfo* type1 = expr->type;
    TypeInfo* type2 = expr->expr->type;
    
    if(type2->typeId == Typeid_Ptr || type2->typeId == Typeid_Ident)
        Swap(TypeInfo*, type1, type2);
    
    if(type1->typeId == Typeid_Ptr && type2->typeId == Typeid_Float)
    {
        SemanticError(t, expr->where, StrLit("Cannot cast from type '%T' to type '%T'"), type2, type1);
        return false;
    }
    
    if(type1->typeId == Typeid_Ident)
    {
        if(type2->typeId == Typeid_Ident)
        {
            auto ident1 = (Ast_IdentType*)type1;
            auto ident2 = (Ast_IdentType*)type2;
            
            if(ident1->name != ident2->name)
            {
                SemanticError(t, expr->where, StrLit("Cannot cast from type '%T' to type '%T'"), type2, type1);
                return false;
            }
        }
        else
        {
            SemanticError(t, expr->where, StrLit("Cannot cast from type '%T' to type '%T'"), type2, type1);
            return false;
        }
    }
    
    return true;
}

bool CheckSubscript(Typer* t, Ast_Subscript* expr)
{
    ProfileFunc(prof);
    
    if(!CheckNode(t, expr->target)) return false;
    if(!CheckNode(t, expr->idxExpr)) return false;
    
    if(!IsTypeDereferenceable(expr->target->type))
    {
        CannotDereferenceTypeError(t, expr->target->type, expr->target->where);
        return false;
    }
    
    if(!IsTypeIntegral(expr->idxExpr->type))
    {
        SemanticError(t, expr->idxExpr->where, StrLit("Subscript is not of integral type, it's of type '%T'"), expr->idxExpr->type);
        return false;
    }
    
    expr->type = GetBaseTypeShallow(expr->target->type);
    return true;
}

bool CheckMemberAccess(Typer* t, Ast_MemberAccess* expr)
{
    if(!CheckNode(t, expr->target)) return false;
    
    // Implicit dereference
    auto targetType = expr->target->type;
    if(targetType->typeId == Typeid_Ptr)
    {
        targetType = GetBaseTypeShallow(expr->target->type);
        if(t->currentProc)
        {
            t->currentProc->toComputeSize.Append(targetType);
        }
    }
    
    TypeId targetTypeId = targetType->typeId;
    if(targetTypeId != Typeid_Struct && targetTypeId != Typeid_Ident)
    {
        SemanticError(t, expr->target->where, StrLit("Cannot apply '.' operator to expression of type '%T'"), expr->target->type);
        return false;
    }
    
    // Type lookup
    
    Ast_StructType* structType = 0;
    if(targetTypeId == Typeid_Struct)
        structType = (Ast_StructType*)targetType;
    else if(targetTypeId == Typeid_Ident)
        structType = (Ast_StructType*)(((Ast_IdentType*)targetType)->structDef->type);  // This is kind of stupid
    
    // TODO: later when types are a result of run directives or other things
    // We might need to yield here
    Assert(structType);
    
    // The struct declaration has already been typechecked at this point.
    expr->structDecl = structType;
    
    int idx = -1;
    for(int i = 0; i < structType->memberNames.length; ++i)
    {
        if(structType->memberNames[i] == expr->memberName)
        {
            idx = i;
            break;
        }
    }
    
    if(idx == -1)
    {
        MemberNotFoundError(t, expr->where, expr->memberName->s, expr->target->where->text);
        return false;
    }
    
    // Get the type corresponding to the member in the struct
    
    if(!CheckType(t, structType->memberTypes[idx], expr->where))
        return false;
    
    expr->memberIdx = idx;
    expr->type      = structType->memberTypes[idx];
    return true;
}

TypeInfo* CheckDeclOrExpr(Typer* t, Ast_Node* node)
{
    ProfileFunc(prof);
    
    TypeInfo* result = 0;
    
    if(node->kind == AstKind_VarDecl)
    {
        auto decl = (Ast_VarDecl*)node;
        if(CheckVarDecl(t, decl))
            result = decl->type;
    }
    else if(Ast_IsExpr(node))
    {
        auto expr = (Ast_Expr*) node;
        if(CheckNode(t, expr))
            result = expr->type;
    }
    else
        Assert(false && "Node is neither declaration nor expression");
    
    return result;
}

TypeInfo* CheckCondition(Typer* t, Ast_Node* node)
{
    ProfileFunc(prof);
    
    auto type = CheckDeclOrExpr(t, node);
    if(Ast_IsExpr(node))
    {
        auto expr = (Ast_Expr*)node;
        expr->castType = &Typer_Bool;
    }
    
    return type;
}

bool CheckType(Typer* t, TypeInfo* type, Token* where)
{
    // Check if there is a pure 'raw' type
    {
        if(type->typeId == Typeid_Raw)
        {
            SemanticError(t, where, StrLit("Pure 'raw' type is not allowed, only '^raw' is"));
            return false;
        }
        
        TypeInfo* tmp = type;
        TypeInfo* base = 0;
        do
        {
            base = 0;
            switch_nocheck(tmp->typeId)
            {
                case Typeid_Ptr: base = ((Ast_PtrType*)type)->baseType; break;
                case Typeid_Arr: base = ((Ast_ArrType*)type)->baseType; break;
            } switch_nocheck_end;
            
            if(base && base->typeId == Typeid_Raw && tmp->typeId != Typeid_Ptr)
            {
                SemanticError(t, where, StrLit("Pure 'raw' type is not allowed, only '^raw' is"));
                return false;
            }
            
            tmp = base;
        }
        while(base);
    }
    
    // Check type
    TypeInfo* baseType = GetBaseType(type);
    if(baseType->typeId == Typeid_Ident)
    {
        auto identDecl = (Ast_IdentType*)baseType;
        auto node = IdentResolution(t, t->curScope, where, identDecl->name);
        if(!node || node->kind != AstKind_StructDef)
        {
            SemanticError(t, where, StrLit("Type with such name was not found."));
            return false;
        }
        
        identDecl->structDef = (Ast_StructDef*)node;
    }
    
    return true;
}

bool ComputeSize(Typer* t, Ast_Node* node)
{
    switch_nocheck(node->kind)
    {
        case AstKind_StructDef:
        {
            bool outcome = true;
            auto structDef = (Ast_StructDef*)node;
            auto declStruct = Ast_GetStructType(structDef);
            ComputeSize_Ret ret = ComputeStructSize(t, declStruct, structDef->where, &outcome);
            if(outcome)
            {
                declStruct->size = ret.size;
                declStruct->align = ret.align;
            }
            
            return outcome;
        }
        case AstKind_VarDecl:
        {
            auto varDecl = (Ast_VarDecl*)node;
            return FillInTypeSize(t, varDecl->type, Ast_GetVarDeclTypeToken(varDecl));
        }
        case AstKind_ProcDecl:
        {
            return true;
        }
        case AstKind_ProcDef:
        {
            auto procDef = (Ast_ProcDef*)node;
            for_array(i, procDef->declsFlat)
            {
                auto decl = procDef->declsFlat[i];
                if(!FillInTypeSize(t, decl->type, decl->typeTok)) return false;
            }
            
            for_array(i, procDef->toComputeSize)
            {
                if(!FillInTypeSize(t, procDef->toComputeSize[i], 0)) return false;
            }
            
            return true;
        }
    } switch_nocheck_end;
    
    return false;
}

bool FillInTypeSize(Typer* t, TypeInfo* type, Token* errTok)
{
    if(type->typeId == Typeid_Ident)
    {
        auto identType = (Ast_IdentType*)type;
        auto referTo = identType->structDef;
        if(!NodePassedStage(referTo, CompPhase_ComputeSize))
        {
            Dg_Yield(t->graph, referTo, CompPhase_ComputeSize);
            return false;
        }
        
        // The struct we're referring to has passed the ComputeSize phase
        type->size = referTo->type->size;
        type->align = referTo->type->align;
    }
    else if(type->typeId == Typeid_Arr)
    {
        // Recursively call FillInTypeSize for subtype and then figure out if the count is usable
        auto arrType = (Ast_ArrType*)type;
        if(!FillInTypeSize(t, arrType->baseType, errTok)) return false;
        
        Assert(arrType->sizeExpr->kind == AstKind_ConstValue && "Non-const values are currently not supported");
        
        auto constValue = (Ast_ConstValue*)arrType->sizeExpr;
        Assert(constValue->type->typeId == Typeid_Integer);
        
        int64 sizeVal  = *(int64*)constValue->addr;
        arrType->size  = arrType->baseType->size * sizeVal;
        arrType->align = arrType->baseType->align;
        return true;
    }
    
    return true;
}

ComputeSize_Ret ComputeStructSize(Typer* t, Ast_StructType* declStruct, Token* errTok, bool* outcome)
{
    struct Funcs
    {
        static uint32 Align(uint32 curSize, uint32 elAlign)
        {
            Assert(IsPowerOf2(elAlign));
            
            uint32 modulo = curSize & (elAlign - 1);  // (curSize % align)
            if(modulo != 0)
                curSize += elAlign - modulo;
            return curSize;
        }
    };
    
    Assert(outcome);
    *outcome = true;
    uint64 sizeResult = 0;
    uint64 alignResult = 0;
    
    for_array(i, declStruct->memberTypes)
    {
        auto it = declStruct->memberTypes[i];
        uint64 curSize = 0;
        uint64 curAlign = 0;
        
        if(it->typeId == Typeid_Struct)
        {
            auto itStruct = (Ast_StructType*)it;
            bool outcomeSubStruct = true;
            ComputeSize_Ret ret = ComputeStructSize(t, itStruct, errTok, &outcomeSubStruct);
            curSize = ret.size;
            curAlign = ret.align;
            
            if(!outcomeSubStruct)
            {
                *outcome = false;
                continue;
            }
        }
        else if(it->typeId == Typeid_Ident)
        {
            auto itIdent = (Ast_IdentType*)it;
            auto structDef = itIdent->structDef;
            auto structDecl = Ast_GetStructType(structDef);
            if(!NodePassedStage(structDef, CompPhase_ComputeSize))
            {
                Dg_Yield(t->graph, structDef, CompPhase_ComputeSize);
                *outcome = false;
                continue;
            }
            
            curSize = structDecl->size;
            curAlign = structDecl->align;
        }
        else if(it->typeId == Typeid_Ptr)
        {
            // TODO: when 32-bit is supported, this should be changed
            curSize = curAlign = 8;
        }
        else  // Primitive types are trivial
        {
            curSize = it->size;
            curAlign = it->align;
        }
        
        sizeResult = Funcs::Align(sizeResult, curAlign);
        declStruct->memberOffsets[i] = sizeResult;
        sizeResult += curSize;
        alignResult = max(alignResult, curAlign);
    }
    
    // Final size needs to be aligned
    sizeResult = Funcs::Align(sizeResult, alignResult);
    return { sizeResult, alignResult };
}

cforceinline bool ApplyOrderConstraint(Ast_Declaration* decl)
{
    bool res = true;
    res &= (decl->kind != AstKind_ProcDecl);
    res &= (decl->kind != AstKind_StructDef);
    if(decl->kind == AstKind_VarDecl)
    {
        auto var = (Ast_VarDecl*)decl;
        res &= (var->declIdx != -1);  // If it's a global variable declaration don't apply it
    }
    
    return res;
}

// This will later use a hash-table.
// Each scope will have its own hash-table, and the search will just be
// a linked-list traversal of hash-tables (or arrays if the number of decls is low)
Ast_Declaration* IdentResolution(Typer* t, Ast_Block* scope, Token* where, Atom* ident)
{
    ProfileFunc(prof);
    
    for(Ast_Block* curScope = scope; curScope; curScope = curScope->enclosing)
    {
        if(curScope->flags & Block_UseHashTable)
        {
            uint32 hash = curScope->declsTable.HashFunction((uintptr)ident);
            for(int i = 0; ; ++i)
            {
                uint32 idx = HashTable_ProbingScheme(hash, i, curScope->declsTable.capacity);
                auto& entry = curScope->declsTable.entries[idx];
                if(!entry.occupied) break;
                
                if(entry.key == ident)
                {
                    bool applyOrderConstraint = ApplyOrderConstraint(entry.val);
                    if(applyOrderConstraint && entry.val->where >= where)
                        continue;
                    
                    return entry.val;
                }
            }
        }
        else
        {
            auto& decls = curScope->decls;
            
            for(int i = 0; i < decls.length; ++i)
            {
                bool applyOrderConstraint = ApplyOrderConstraint(decls[i]);
                if(applyOrderConstraint && curScope->decls[i]->where >= where)
                    continue;
                
                if(curScope->decls[i]->name == ident)
                    return curScope->decls[i];
            }
        }
    }
    
    return 0;
}

bool CheckNotAlreadyDeclared(Typer* t, Ast_Block* scope, Ast_Declaration* decl)
{
    if(scope->flags & Block_UseHashTable)
    {
        uint32 hash = scope->declsTable.HashFunction((uintptr)decl->name);
        for(int i = 0; i <= scope->declsTable.count + 1; ++i)
        {
            Assert(i != scope->declsTable.count + 1);
            
            uint32 idx = HashTable_ProbingScheme(hash, i, scope->declsTable.capacity);
            auto& entry = scope->declsTable.entries[idx];
            if(!entry.occupied) break;
            
            if(entry.key == decl->name && entry.val->where < decl->where)
            {
                SemanticError(t, decl->where, StrLit("Redefinition, this symbol was already defined in this scope, ..."));
                SemanticErrorContinue(t, scope->decls[i]->where, StrLit("... here"));
                return false;
            }
        }
    }
    else
    {
        for(int i = 0; i < scope->decls.length && scope->decls[i]->where < decl->where; ++i)
        {
            if(scope->decls[i]->name == decl->name)
            {
                if(scope->decls[i] != decl)
                {
                    SemanticError(t, decl->where, StrLit("Redefinition, this symbol was already defined in this scope, ..."));
                    SemanticErrorContinue(t, scope->decls[i]->where, StrLit("... here"));
                    return false;
                }
            }
        }
    }
    
    return true;
}

TypeInfo* GetBaseType(TypeInfo* type)
{
    while(true)
    {
        switch_nocheck(type->typeId)
        {
            default: return type;
            case Typeid_Ptr: type = ((Ast_PtrType*)type)->baseType; break;
            case Typeid_Arr: type = ((Ast_ArrType*)type)->baseType; break;
        } switch_nocheck_end;
    }
    
    Assert(false && "Unreachable code");
    return 0;
}

TypeInfo* GetBaseTypeShallow(TypeInfo* type)
{
    switch_nocheck(type->typeId)
    {
        default: break;
        case Typeid_Ptr: type = ((Ast_PtrType*)type)->baseType; break;
        case Typeid_Arr: type = ((Ast_ArrType*)type)->baseType; break;
    } switch_nocheck_end;
    
    return type;
}

bool TypesCompatible(Typer* t, TypeInfo* src, TypeInfo* dst)
{
    ProfileFunc(prof);
    
    // 'None' doesn't match with anything
    if(src->typeId == Typeid_None || dst->typeId == Typeid_None)
        return false;
    
    if(src == dst) return true;
    
    // TODO: Zero can convert into anything
    
    
    // @cleanup This should just get cleaned up
    
    // Implicitly convert arrays into pointers
    if(src->typeId == Typeid_Arr && dst->typeId == Typeid_Ptr)
    {
        auto arrayOf = ((Ast_ArrType*)src)->baseType;
        src = Typer_MakePtr(t->arena, arrayOf);
    }
    
    // Implicitly convert procs into pointers
    if(src->typeId == Typeid_Ptr && dst->typeId == Typeid_Proc)
    {
        auto base = (Ast_ProcType*)dst;
        dst = Typer_MakePtr(t->arena, base);
    }
    else if(src->typeId == Typeid_Proc && dst->typeId == Typeid_Ptr)
    {
        auto base = (Ast_ProcType*)src;
        src = Typer_MakePtr(t->arena, base);
    }
    
    if(src->typeId != dst->typeId)
    {
        if(IsTypeIntegral(src) && IsTypeIntegral(dst))
            return true;
        
        if(IsTypeIntegral(src) && dst->typeId == Typeid_Ptr)
        {
            // Only if src is 0
        }
        else if(IsTypeScalar(src) && IsTypeScalar(dst))
        {
            return true;
        }
        else if(src->typeId == Typeid_Proc && dst->typeId == Typeid_Ptr)
        {
            auto dstPtrTo = ((Ast_PtrType*) dst)->baseType;
            if(dstPtrTo->typeId == Typeid_Proc)
            {
                return TypesIdentical(t, dstPtrTo, src);
            }
        }
        
        // In all other cases, not compatible
        return false;
    }
    
    // At this point, src and dst are superficially the same type
    
    if(src->typeId == Typeid_Proc)
    {
        return TypesIdentical(t, src, dst);
    }
    else if(src->typeId == Typeid_Ptr)
    {
        // If the destination is a raw pointer, anything works.
        if(GetBaseTypeShallow(dst)->typeId == Typeid_Raw)
            return true;
        
        return TypesIdentical(t, src, dst);
    }
    
    // Allow kind matching for most things (bool, int, float, etc.)
    return true;
}

TypeInfo* GetCommonType(Typer* t, TypeInfo* type1, TypeInfo* type2)
{
    ProfileFunc(prof);
    
    // Implicitly convert arrays into pointers
    if(type1->typeId == Typeid_Arr)
    {
        auto base = ((Ast_ArrType*)type1)->baseType;
        type1 = Typer_MakePtr(t->arena, base);
    }
    
    // Implicitly convert arrays into pointers
    if(type2->typeId == Typeid_Arr)
    {
        auto base = ((Ast_ArrType*)type2)->baseType;
        type2 = Typer_MakePtr(t->arena, base);
    }
    
    // Implicitly convert functions into function pointers
    if(type1->typeId == Typeid_Proc)
        type1 = Typer_MakePtr(t->arena, type1);
    if(type2->typeId == Typeid_Proc)
        type2 = Typer_MakePtr(t->arena, type2);
    
    // Operations with floats promote up to floats
    
    if(type1->typeId == Typeid_Float && type2->typeId == Typeid_Float)
    {
        if(type1->size > type2->size) return type1;
        else return type2;
    }
    else if(type1->typeId == Typeid_Float)
        return type1;
    else 
        return type2;
    
    // Promote any small integral types into ints
    
    // If the types don't match pick the bigger one
    
    // Unsigned types are preferred
    
    return type1;
}

bool TypesIdentical(Typer* t, TypeInfo* type1, TypeInfo* type2)
{
    ProfileFunc(prof);
    
    if(type1 == type2) return true;
    
    while(true)
    {
        if(type1->typeId != type2->typeId)
            return false;
        
        switch_nocheck(type1->typeId)
        {
            default: return true;
            case Typeid_Ident:
            {
                auto ident1 = (Ast_IdentType*)type1;
                auto ident2 = (Ast_IdentType*)type2;
                if(ident1->name == ident2->name)
                    return true;
                return false;
            }  // Check if same ident
            case Typeid_Ptr:
            {
                if(type1->typeId != type2->typeId)
                    return false;
                type1 = ((Ast_PtrType*)type1)->baseType;
                type2 = ((Ast_PtrType*)type2)->baseType;
                break;
            }
            case Typeid_Arr:
            {
                if(type1->typeId != type2->typeId)
                    return false;
                type1 = ((Ast_ArrType*)type1)->baseType;
                type2 = ((Ast_ArrType*)type2)->baseType;
                break;
            }
            case Typeid_Proc:
            {
                auto proc1 = (Ast_ProcType*)type1;
                auto proc2 = (Ast_ProcType*)type2;
                if(proc1->args.length != proc2->args.length)
                    return false;
                if(proc1->retTypes.length != proc2->retTypes.length)
                    return false;
                
                for_array(i, proc1->args)
                {
                    if(!TypesIdentical(t, proc1->args[i]->type, proc2->args[i]->type))
                        return false;
                }
                for_array(i, proc1->retTypes)
                {
                    if(!TypesIdentical(t, proc1->retTypes[i], proc2->retTypes[i]))
                        return false;
                }
                
                return true; // Nothing else to check
                break;
            }
        } switch_nocheck_end;
    }
    
    return true;
}

bool ImplicitConversion(Typer* t, Ast_Expr* exprSrc, TypeInfo* src, TypeInfo* dst)
{
    ProfileFunc(prof);
    
    // Specialized error for None type?
    
    // Implicitly convert functions & arrays into pointers
    if(dst->typeId == Typeid_Proc)
        dst = Typer_MakePtr(t->arena, dst);
    else if(dst->typeId == Typeid_Arr)
    {
        auto arrType = Ast_MakeType<Ast_ArrType>(t->arena);
        dst = Typer_MakePtr(t->arena, arrType->baseType);
    }
    
    if(false)  // Data loss warnings
    {
        // Int and float conversions
        if(IsTypeNumeric(src) && IsTypeNumeric(dst))
        {
            bool isSrcFloat  = src->typeId == Typeid_Float;
            bool isDstFloat  = dst->typeId == Typeid_Float;
            bool isSrcSigned = src->isSigned;
            bool isDstSigned = dst->isSigned;
            
            if(isSrcFloat == isSrcFloat)
            {
                if(!isSrcFloat && isSrcSigned != isDstSigned)
                    SemanticError(t, exprSrc->where, StrLit("Implicit conversion affects signedness"));
                if(src->typeId > dst->typeId)
                    SemanticError(t, exprSrc->where, StrLit("Implicit conversion may lose data"));
            }
            else
            {
                SemanticError(t, exprSrc->where, StrLit("Implicit conversion may lose data"));
            }
        }
    }
    
    if(!TypesCompatible(t, src, dst))
    {
        //SemanticError(t, exprSrc->where, StrLit("Cannot implicitly convert type '%T' to '%T'"), src, dst);
        return false;
    }
    
    return true;
}

String TypeInfo2String(TypeInfo* type, Arena* dest)
{
    if(!type)
        return { 0, 0 };
    
    ScratchArena scratch(dest);
    ScratchArena scratch2(dest, scratch);
    
    StringBuilder strBuilder(scratch);
    
    bool quit = false;
    while(!quit)
    {
        if(type->typeId <= Typeid_BasicTypesEnd && type->typeId >= Typeid_BasicTypesBegin)
        {
            switch_nocheck(type->typeId)
            {
                default: Assert(false && "TypeInfo2Str, unrecognized type");
                case Typeid_Float:
                {
                    if(type->size == 4)
                        strBuilder.Append("float");
                    else if(type->size == 8)
                        strBuilder.Append("double");
                    else Assert(false && "Unreachable code");
                    
                    break;
                }
                case Typeid_Integer:
                {
                    if(!type->isSigned)
                        strBuilder.Append('u');
                    
                    char buf[6];
                    int numChars = snprintf(buf, 6, "int%lld", type->size * 8);
                    String str = { buf, numChars };
                    strBuilder.Append(str);
                    
                    break;
                }
                case Typeid_Bool: strBuilder.Append("bool"); break;
                case Typeid_Char: strBuilder.Append("char"); break;
                case Typeid_None: strBuilder.Append("none"); break;
            } switch_nocheck_end;
            
            // For any primitive type, just quit
            quit = true;
        }
        else switch_nocheck(type->typeId)
        {
            default: Assert(false); break;
            case Typeid_Ident:
            {
                String ident = ((Ast_IdentType*)type)->name->s;
                strBuilder.Append(ident);
                quit = true;
                break;
            }
            case Typeid_Struct:
            {
                strBuilder.Append("type");
                quit = true;
                break;
            }
            case Typeid_Ptr:
            {
                strBuilder.Append("^");
                type = ((Ast_PtrType*)type)->baseType;
                break;
            }
            case Typeid_Arr:
            {
                strBuilder.Append("[]");
                type = ((Ast_ArrType*)type)->baseType;
                break;
            }
            case Typeid_Proc:
            {
                auto procType = (Ast_ProcType*)type;
                auto argTypesStr = Arena_AllocArray(scratch2, procType->args.length, String);
                auto retTypesStr = Arena_AllocArray(scratch2, procType->retTypes.length, String);
                
                for_array(i, procType->args)
                    argTypesStr[i] = TypeInfo2String(procType->args[i]->type, scratch2);
                
                for_array(i, procType->retTypes)
                    retTypesStr[i] = TypeInfo2String(procType->retTypes[i], scratch2);
                
                strBuilder.Append("proc (");
                
                for_array(i, procType->args)
                {
                    strBuilder.Append(argTypesStr[i]);
                    if(i != procType->args.length - 1)
                        strBuilder.Append(", ");
                }
                
                strBuilder.Append(")->");
                
                if(procType->retTypes.length > 1)
                    strBuilder.Append("(");
                
                for_array(i, procType->retTypes)
                    strBuilder.Append(retTypesStr[i]);
                
                if(procType->retTypes.length > 1)
                    strBuilder.Append(StrLit(")"));
                
                quit = true;
                break;
            }
            case Typeid_Raw:
            {
                strBuilder.Append(StrLit("raw"));
                quit = true;
                break;
            }
        } switch_nocheck_end;
    }
    
    return strBuilder.ToString(dest);
}

// Supports formatted strings (but only %d and %T)
// NOTE(Leo): Because of how printf and C's varargs work, you can't really
// easily add functinoality to the existing printf, so you just have to reimplement
// the whole thing.
// TODO: refactor, remove repetition. Some of this stuff (like support of %d)
// should be available to the other modules as well (parser, lexer...)
String GenerateErrorString(String message, va_list args, Arena* allocTo)
{
    ScratchArena typeArena(allocTo);
    ScratchArena stringArena(allocTo, typeArena);
    
    StringBuilder resBuilder(stringArena);
    
    // Filter format specifiers and replace them with the string representation of the type
    
    int i = 0;
    // First and last characters of string separated by %T (to append)
    int first = 0;
    int last  = 0;
    while(i < message.length)
    {
        bool isLastChar = i >= message.length - 1;
        if(!isLastChar && message[i] == '%')
        {
            if(message[i+1] == 'T')
            {
                last = i-1;
                if(first <= last)
                {
                    String toAppend;
                    toAppend.ptr = message.ptr + first;
                    toAppend.length = last - first + 1;
                    resBuilder.Append(toAppend);
                }
                
                auto type = va_arg(args, TypeInfo*);
                String typeStr = TypeInfo2String(type, typeArena);
                resBuilder.Append(typeStr);
                
                first = i+2;
                ++i;
            }
            else if(message[i+1] == 'd')
            {
                last = i-1;
                if(first <= last)
                {
                    String toAppend;
                    toAppend.ptr = message.ptr + first;
                    toAppend.length = last - first + 1;
                    resBuilder.Append(toAppend);
                }
                
                auto toPrint = va_arg(args, int);
                constexpr int bufferLen = 16;
                char buffer[bufferLen];
                snprintf(buffer, bufferLen, "%d", toPrint);
                String valStr = { buffer, (int64)strlen(buffer) };
                resBuilder.Append(valStr);
                
                first = i+2;
                ++i;
            }
        }
        
        ++i;
    }
    
    last = message.length - 1;
    if(first <= last)
    {
        String toAppend;
        toAppend.ptr = message.ptr + first;
        toAppend.length = last - first + 1;
        resBuilder.Append(toAppend);
    }
    
    return resBuilder.ToString(allocTo);
}

// @temporary @robustness Can't figure out how to properly do this without
// using this. Maybe the printing API just needs to change, and calling
// these two functions consecutively is just not a very good idea?
// Consider using just a single function (e.g. SemanticErrorLong),
// where an array of tokens is specified
int semErrorContinueFlag = 0;
void SemanticError(Typer* t, Token* token, String message, ...)
{
    if(t->status)
    {
        semErrorContinueFlag = min(semErrorContinueFlag + 1, 2);
        t->status = false;
    }
    
    Dg_Error(t->graph);
    
    ScratchArena scratch;
    
    va_list args;
    va_start(args, message);
    defer(va_end(args););
    
    String errStr = GenerateErrorString(message, args, scratch);
    
    CompileError(t->tokenizer, token, errStr);
}

// It's assumed that this function is called immediately after
// SemanticError if at all
void SemanticErrorContinue(Typer* t, Token* token, String message, ...)
{
    if(semErrorContinueFlag >= 2)
        return;
    
    ScratchArena scratch;
    
    va_list args;
    va_start(args, message);
    defer(va_end(args););
    
    String errStr = GenerateErrorString(message, args, scratch);
    
    CompileErrorContinue(t->tokenizer, token, errStr);
}

void CannotConvertToScalarTypeError(Typer* t, TypeInfo* type, Token* where)
{
    SemanticError(t, where, StrLit("Cannot convert type '%T' to any scalar type"), type);
}

void CannotConvertToIntegralTypeError(Typer* t, TypeInfo* type, Token* where)
{
    SemanticError(t, where, StrLit("Cannot convert type '%T' to any integral type"), type);
}

void CannotDereferenceTypeError(Typer* t, TypeInfo* type, Token* where)
{
    SemanticError(t, where, StrLit("Cannot dereference type '%T'"), type);
}

void IncompatibleTypesError(Typer* t, TypeInfo* type1, TypeInfo* type2, Token* where)
{
    SemanticError(t, where, StrLit("The following types are incompatible: '%T' and '%T'"), type1, type2);
}

void IncompatibleReturnsError(Typer* t, Token* where, int numStmtRets, int numProcRets)
{
    if(numStmtRets == 0)
        SemanticError(t, where, StrLit("Statement does not return a value, ..."));
    else if(numStmtRets == 1)
    {
        if(numProcRets > 1)
            SemanticError(t, where, StrLit("Trying to return a single value, ..."));
        else
            SemanticError(t, where, StrLit("Trying to return a value, ..."));
    }
    else
        SemanticError(t, where, StrLit("Trying to return %d values, ..."), numStmtRets);
    
    if(numProcRets == 0)
        SemanticErrorContinue(t, t->currentProc->where, StrLit("... but the procedure does not return any value."));
    else if(numProcRets == 1)
        SemanticErrorContinue(t, t->currentProc->where, StrLit("... but the procedure returns 1 value."));
    else
        SemanticErrorContinue(t, t->currentProc->where, StrLit("... but the procedure returns %d values."), numProcRets);
}

void MemberNotFoundError(Typer* t, Token* where, String memberName, String structName)
{
    ScratchArena scratch;
    StringBuilder b(scratch);
    b.Append("Could not find member '");
    b.Append(memberName);
    b.Append("' in '");
    b.Append(structName);
    b.Append('\'');
    SemanticError(t, where, b.string);
}