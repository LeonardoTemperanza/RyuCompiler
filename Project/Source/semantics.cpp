
#include "lexer.h"
#include "ast.h"
#include "dependency_graph.h"

#include "semantics.h"

// TODO TODO: missing compute size for procedures.

// Primitive types

Typer InitTyper(Arena* arena, Parser* parser)
{
    Typer t;
    t.tokenizer = parser->tokenizer;
    //t.parser = parser;
    t.arena = arena;
    
    return t;
}

Ast_DeclaratorPtr* Typer_MakePtr(Arena* arena, TypeInfo* base)
{
    auto ptr = Arena_AllocAndInitPack(arena, Ast_DeclaratorPtr);
    ptr->baseType = base;
    return ptr;
}

bool CheckNode(Typer* t, Ast_Node* node)
{
    bool outcome = true;
    
    switch(node->kind)
    {
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
        case AstKind_MultiAssign:   outcome = CheckMultiAssign(t, (Ast_MultiAssign*)node); break;
        case AstKind_EmptyStmt:     break;
        case AstKind_StmtEnd:       break;
        case AstKind_ExprBegin:     break;
        case AstKind_Literal:       printf("Literal not implemented.\n"); outcome = false; break;
        case AstKind_NumLiteral:    outcome = CheckNumLiteral(t, (Ast_NumLiteral*)node); break;
        case AstKind_Ident:         outcome = CheckIdent(t, (Ast_IdentExpr*)node); break;
        case AstKind_FuncCall:      outcome = CheckFuncCall(t, (Ast_FuncCall*)node, false); break;
        case AstKind_BinaryExpr:    outcome = CheckBinExpr(t, (Ast_BinaryExpr*)node); break;
        case AstKind_UnaryExpr:     outcome = CheckUnaryExpr(t, (Ast_UnaryExpr*)node); break;
        case AstKind_TernaryExpr:   printf("Ternary expression not implemented.\n"); outcome = false; break;
        case AstKind_Typecast:      outcome = CheckTypecast(t, (Ast_Typecast*)node); break;
        case AstKind_Subscript:     outcome = CheckSubscript(t, (Ast_Subscript*)node); break;
        case AstKind_MemberAccess:  outcome = CheckMemberAccess(t, (Ast_MemberAccess*)node); break;
        case AstKind_ExprEnd:       break;
        default: Assert(false && "Enum value out of bounds");
    }
    
    return outcome;
}

bool CheckProcDef(Typer* t, Ast_ProcDef* proc)
{
    t->currentProc = proc;
    t->checkedReturnStmt = false;
    
    auto declProc = Ast_GetDeclProc(proc);
    
    // TODO: Ok, this is getting ridiculous. Types
    // should just have a Token* I think
    
    // Look up return types
    if(declProc->retTypes.length > 0)
    {
        if(!CheckType(t, declProc->retTypes[0], proc->where))
            return false;
    }
    
    // Look up argument types
    for(int i = 0; i < declProc->args.length; ++i)
    {
        if(!CheckType(t, declProc->args[i]->type, proc->where))
            return false;
    }
    
    if(!AddDeclaration(t, t->curScope, proc)) return false;
    
    for(int i = 0; i < declProc->args.length; ++i)
    {
        if(!AddDeclaration(t, &proc->block, declProc->args[i]))
            return false;
    }
    
    // Definition
    if(!CheckBlock(t, &proc->block)) return false;
    
    auto retTypes = Ast_GetDeclProc(proc)->retTypes;
    if(retTypes.length > 0 && !t->checkedReturnStmt)
    {
        SemanticError(t, t->currentProc->where, StrLit("Procedure has a return type but no return statement was found."));
        return false;
    }
    
    return true;
}

bool CheckStructDef(Typer* t, Ast_StructDef* structDef)
{
    auto structType = Ast_GetDeclStruct(structDef);
    for_array(i, structType->memberTypes)
    {
        if(!CheckType(t, structType->memberTypes[i], structDef->where))
            return false;
    }
    
    return true;
}

bool CheckVarDecl(Typer* t, Ast_VarDecl* decl)
{
    if(!CheckType(t, decl->type, Ast_GetVarDeclTypeToken(decl))) return false;
    if(!AddDeclaration(t, t->curScope, decl)) return false;
    
    if(decl->initExpr)
    {
        if(!CheckNode(t, decl->initExpr)) return false;
        
        // Check that they're the same type here
        
        if(!ImplicitConversion(t, decl->initExpr, decl->initExpr->type, decl->type))
        {
            IncompatibleTypesError(t, decl->type, decl->initExpr->type, decl->where);
            return false;
        }
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
    }
    
    return result;
}

bool CheckIf(Typer* t, Ast_If* stmt)
{
    auto prevScope = t->curScope;
    t->curScope = stmt->thenBlock;
    defer(t->curScope = prevScope);
    
    TypeInfo* condType = CheckDeclOrExpr(t, stmt->condition);
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
                if(!CheckNode(t, stmt->elseStmt))
                    return false;
            }
        }
    }
    
    return true;
}

bool CheckFor(Typer* t, Ast_For* stmt)
{
    auto prevScope = t->curScope;
    t->curScope = stmt->body;
    defer(t->curScope = prevScope);
    
    if(!CheckNode(t, stmt->initialization))
        return false;
    
    TypeInfo* condType = CheckDeclOrExpr(t, stmt->condition);
    if(condType)
    {
        if(!IsTypeScalar(condType))
        {
            CannotConvertToScalarTypeError(t, condType, stmt->condition->where);
            return false;
        }
        else
        {
            if(!CheckNode(t, stmt->update)) return false;
            if(!CheckBlock(t, stmt->body)) return false;
        }
    }
    
    return true;
}

bool CheckWhile(Typer* t, Ast_While* stmt)
{
    ProfileFunc(prof);
    
    auto prevScope = t->curScope;
    t->curScope = stmt->doBlock;
    defer(t->curScope = prevScope);
    
    TypeInfo* condType = CheckDeclOrExpr(t, stmt->condition);
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
    
    if(!CheckNode(t, stmt->doStmt))    return false;
    if(!CheckNode(t, stmt->condition)) return false;
    
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
    
    printf("Switch not implemented.\n");
    return false;
}

bool CheckDefer(Typer* t, Ast_Defer* stmt)
{
    ProfileFunc(prof);
    
    printf("Defer not implemented.\n");
    return false;
}

bool CheckReturn(Typer* t, Ast_Return* stmt)
{
    ProfileFunc(prof);
    
    t->checkedReturnStmt = true;
    
    auto retStmt = (Ast_Return*)stmt;
    Array<TypeInfo*> retTypes = Ast_GetDeclProc(t->currentProc)->retTypes;
    
#if 0
    if(retTypes.length > 1)
    {
        SemanticError(t, retStmt->where, StrLit("Multiple return types are currently not supported."));
        return false;
    }
#endif
    
    if(retTypes.length != retStmt->rets.length)
    {
        SemanticError(t, retStmt->where, StrLit("Trying to return x values, ..."));
        SemanticErrorContinue(t, t->currentProc->where, StrLit("... but the procedure returns y values."));
        return false;
    }
    
    if(retStmt->rets.length > 0)
    {
        if(retTypes.length == 0)
        {
            SemanticError(t, retStmt->where, StrLit("Trying to return a value, ..."));
            SemanticErrorContinue(t, t->currentProc->where, StrLit("... but the procedure does not return a value."));
            return false;
        }
        
        for_array(i, retStmt->rets)
        {
            if(!CheckNode(t, retStmt->rets[i])) return false;
            
            TypeInfo* exprType = retStmt->rets[i]->type;
            TypeInfo* retType  = retTypes[i];
            
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
    printf("Break not implemented\n");
    return false;
}

bool CheckContinue(Typer* t, Ast_Continue* stmt)
{
    printf("Continue not implemented\n");
    return false;
}

bool CheckMultiAssign(Typer* t, Ast_MultiAssign* stmt)
{
    auto& lefts = stmt->lefts;
    auto& rights = stmt->rights;
    
    // Check all declarations
    ScratchArena scratch;
    Array<TypeInfo*> leftTypes;
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
        Array<TypeInfo*> toCheck;  // > 1 if there's a function call with multiple return values, = 1 for anything else
        
        // Current rhs (which means, zero or more types due to multiple returns)
        if(stmt->rights[rightCounter]->kind == AstKind_FuncCall)
        {
            // Check against multiple statements
            auto call = (Ast_FuncCall*)stmt->rights[rightCounter];
            if(!CheckFuncCall(t, call, true)) return false;
            
            auto procDecl = Ast_GetDeclCallTarget(call);
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
        bool tempPrint = t->tokenizer->status != CompStatus_Error;
        SemanticError(t, stmt->where, StrLit("Mismatching number of left-hand and right-hand side values (x and y, respectively)"));
        if(tempPrint) fprintf(stderr, "%d %d\n", leftCounter, rightCounter);
        
        return false;
    }
    
    return true;
}

bool CheckNumLiteral(Typer* t, Ast_NumLiteral* expr)
{
    // @cleanup The type is currently set by the parser. I don't think that's good...
    return true;
}

Ast_Declaration* CheckIdent(Typer* t, Ast_IdentExpr* expr)
{
    ProfileFunc(prof);
    
    auto node = IdentResolution(t, t->curScope, expr->ident);
    if(!node)
    {
        SemanticError(t, expr->where, StrLit("Undeclared identifier."));
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
    if(call->target->type->typeId == Typeid_Ptr)
        call->target->type = GetBaseType(call->target->type);
    
    if(call->target->type->typeId != Typeid_Proc)
    {
        ScratchArena scratch;
        String typeString = TypeInfo2String(call->target->type, scratch);
        StringBuilder builder;
        builder.Append(scratch, 3, StrLit("Cannot call target of type '"), typeString, StrLit("'"));
        SemanticError(t, call->where, builder.string);
        return false;
    }
    
    auto procDecl = (Ast_DeclaratorProc*)call->target->type;
    
    // If the number of return types is > 0, and we're not in a
    // multi assign statement, then throw an error because multiple
    // returns cannot be used in this context.
    // NOTE: Maybe rethink this language feature, so that it can be more
    // flexible?
    if(!isMultiAssign && procDecl->retTypes.length > 1)
    {
        SemanticError(t, call->where,
                      StrLit("Calling procedures with multiple return values in expression is not allowed. Use a MultiAssign statement instead."));
        return false;
    }
    
    // Check that number of parameters and types correspond
    // Support varargs in the future maybe?
    
    if(call->args.length != procDecl->args.length)
    {
        // TODO: print actual number of arguments, need to improve the print function
        SemanticError(t, call->where, StrLit("Procedure does not take n arguments"));
        return false;
    }
    
    int numArgs = call->args.length;
    for(int i = 0; i < numArgs; ++i)
    {
        if(!CheckNode(t, call->args[i]))
            return false;
        if(!ImplicitConversion(t, call->args[i], call->args[i]->type, procDecl->args[i]->type))
            return false;
    }
    
    if(procDecl->retTypes.length <= 0)
        call->type = &noneType;
    else
        call->type = procDecl->retTypes[0];
    
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
        case Tok_And: case Tok_Or:
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
                            ScratchArena scratch;
                            String ltypeStr = TypeInfo2String(ltype, scratch);
                            String rtypeStr = TypeInfo2String(rtype, scratch);
                            StringBuilder builder;
                            builder.Append(StrLit("Pointer difference disallowed for pointers of different base types ('"), scratch.arena());
                            builder.Append(scratch, 4, ltypeStr, StrLit("' and '"), rtypeStr, StrLit("')"));
                            SemanticError(t, bin->where, builder.string);
                            bin->type = 0;
                            return false;
                        }
                        else
                        {
                            lhs->castType = lhs->type;
                            rhs->castType = rhs->type;
                            bin->type = ltype;
                        }
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
                        StringBuilder b;
                        b.Append(scratch, 5, StrLit("Cannot apply binary operator to '"), ltypeStr,
                                 StrLit("' and '"), rtypeStr, StrLit("'"));
                        SemanticError(t, bin->where, b.string);
                        return false;
                    }
                    
                    bin->type = lhs->type;
                }
                
                bin->type = lhs->type;
            }
            else  // Any other binary operation
            {
                if(!(ltype->typeId >= Typeid_Bool && ltype->typeId <= Typeid_Double &&
                     rtype->typeId >= Typeid_Bool && rtype->typeId <= Typeid_Double))
                {
                    // NOTE: This is where you would wait for an operator to be overloaded
                    // Look for overloaded operators (they're procs with certain names) and
                    // yield if none are found.
                    
                    ScratchArena scratch;
                    String ltypeStr = TypeInfo2String(ltype, scratch.arena());
                    String rtypeStr = TypeInfo2String(rtype, scratch.arena());
                    StringBuilder b;
                    b.Append(scratch.arena(), 5, StrLit("Cannot apply binary operator to types '"), ltypeStr,
                             StrLit("' and '"), rtypeStr, StrLit("'"));
                    SemanticError(t, bin->where, b.string);
                    return false;
                }
                
                // @cleanup Is this necessary? I forgot what GetCommonType even does
                TypeInfo* commonType = GetCommonType(t, ltype, rtype);
                bool conv1Success = ImplicitConversion(t, lhs, ltype, commonType);
                bool conv2Success = ImplicitConversion(t, rhs, rtype, commonType);
                if(!conv1Success || !conv2Success) return false;
                
                bin->lhs->castType = commonType;
                bin->rhs->castType = commonType;
                bin->type = commonType;
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
                //bin->type = commonType; // ??
                bin->type = &primitiveTypes[Typeid_Bool];
            }
            else
            {
                IncompatibleTypesError(t, bin->lhs->type, bin->rhs->type, bin->where);
                return false;
            }
            
            bin->type = &primitiveTypes[Typeid_Bool];
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
            
            bin->type = &primitiveTypes[Typeid_Bool];
            break;
        }
    }
    
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
        case '+': case '-':
        {
            if(!IsTypeIntegral(expr->expr->type))
            {
                CannotConvertToIntegralTypeError(t, expr->expr->type, expr->expr->where);
                return false;
            }
            
            expr->type = expr->expr->type;
            break;
        }
        case '!':
        {
            if(!IsTypeScalar(expr->expr->type))
            {
                CannotConvertToScalarTypeError(t, expr->expr->type, expr->expr->where);
                return false;
            }
            
            expr->type = &primitiveTypes[Typeid_Bool];
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
        auto base = ((Ast_DeclaratorArr*)expr->expr)->baseType;
        expr->expr->type = Typer_MakePtr(t->arena, base);
    }
    
    if(expr->type->typeId == Typeid_Arr)
    {
        auto base = ((Ast_DeclaratorArr*)expr)->baseType;
        expr->type = Typer_MakePtr(t->arena, base);
    }
    
    TypeInfo* type1 = expr->type;
    TypeInfo* type2 = expr->expr->type;
    
    if(type2->typeId == Typeid_Ptr || type2->typeId == Typeid_Ident)
        Swap(TypeInfo*, type1, type2);
    
    if(type1->typeId == Typeid_Ptr && IsTypeFloat(type2))
    {
        SemanticError(t, expr->where, StrLit("Cannot cast from type '%T' to type '%T'"));
        return false;
    }
    
    if(type1->typeId == Typeid_Ident)
    {
        if(type2->typeId == Typeid_Ident)
        {
            auto ident1 = (Ast_DeclaratorIdent*)type1;
            auto ident2 = (Ast_DeclaratorIdent*)type2;
            
            if(ident1->ident != ident2->ident)
            {
                SemanticError(t, expr->where, StrLit("Cannot cast from type '%T' to type '%T'"));
                return false;
            }
        }
        else
        {
            SemanticError(t, expr->where, StrLit("Cannot cast from type '%T' to type '%T'"));
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
        SemanticError(t, expr->idxExpr->where, StrLit("Subscript is not of integral type, it's of type '%T'"));
        return false;
    }
    
    expr->type = GetBaseTypeShallow(expr->target->type);
    return true;
}

bool CheckMemberAccess(Typer* t, Ast_MemberAccess* expr)
{
    if(!CheckNode(t, expr->target)) return false;
    
    // Implicit dereference
    if(expr->target->type->typeId == Typeid_Ptr)
        expr->target->type = GetBaseType(expr->target->type);
    
    TypeId targetTypeId = expr->target->type->typeId;
    if(targetTypeId != Typeid_Struct && targetTypeId != Typeid_Ident)
    {
        SemanticError(t, expr->target->where, StrLit("Cannot apply '.' operator to expression of type '%T'"));
        return false;
    }
    
    // Type lookup
    
    Ast_DeclaratorStruct* structDecl = 0;
    if(targetTypeId == Typeid_Struct)
        structDecl = (Ast_DeclaratorStruct*)expr->target->type;
    else if(targetTypeId == Typeid_Ident)
        structDecl = (Ast_DeclaratorStruct*)(((Ast_DeclaratorIdent*)expr->target->type)->structDef->type);  // This is kind of stupid
    
    Assert(structDecl);
    
    // TODO: rework this to use the CompPhase
    
    int idx = -1;
    for(int i = 0; i < structDecl->memberNames.length; ++i)
    {
        if(structDecl->memberNames[i] == expr->memberName)
        {
            idx = i;
            break;
        }
    }
    
    if(idx == -1)
    {
        SemanticError(t, expr->where, StrLit("Member '...' in struct '...' was not found"));
        return false;
    }
    
    // Get the type corresponding to the member in the struct
    if(!CheckType(t, structDecl->memberTypes[idx], expr->where))
    {
        return false;
    }
    
    expr->type = structDecl->memberTypes[idx];
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

bool CheckType(Typer* t, TypeInfo* type, Token* where)
{
    // TODO: this should be modified in the future to yield
    // in particular cases (#run directive for instance, or #type_of)
    
    // Check type
    TypeInfo* baseType = GetBaseType(type);
    if(baseType->typeId == Typeid_Ident)
    {
        auto identDecl = (Ast_DeclaratorIdent*)baseType;
        auto node = IdentResolution(t, t->curScope, identDecl->ident);
        if(!node || node->kind != AstKind_StructDef)
        {
            SemanticError(t, where, StrLit("Struct with such name was not found."));
            return false;
        }
        
        identDecl->structDef = (Ast_StructDef*)node;
    }
    
    return true;
}

bool ComputeSize(Typer* t, Ast_StructDef* structDef)
{
    bool outcome = true;
    auto declStruct = Ast_GetDeclStruct(structDef);
    ComputeSize_Ret ret = ComputeSize(t, declStruct, structDef->where, &outcome);
    
    if(outcome)
    {
        declStruct->size = ret.size;
        declStruct->align = ret.align;
        printf("Outcome = true, Size: %d\n", ret.size);
    }
    else
    {
        printf("Outcome = false\n");
    }
    
    return outcome;
}

ComputeSize_Ret ComputeSize(Typer* t, Ast_DeclaratorStruct* declStruct, Token* errTok, bool* outcome)
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
    uint32 sizeResult = 0;
    uint32 alignResult = 0;
    
    // TODO: refactor this
    for_array(i, declStruct->memberTypes)
    {
        auto it = declStruct->memberTypes[i];
        
        if(it->typeId == Typeid_Struct)
        {
            auto itStruct = (Ast_DeclaratorStruct*)it;
            bool outcomeSubStruct = true;
            ComputeSize_Ret ret = ComputeSize(t, itStruct, errTok, &outcomeSubStruct);
            uint32 sizeSubStruct = ret.size;
            uint32 alignSubStruct = ret.align;
            
            if(!outcomeSubStruct)
            {
                *outcome = false;
                continue;
            }
            
            sizeResult = Funcs::Align(sizeResult, alignSubStruct);
            declStruct->memberOffsets[i] = sizeResult;
            sizeResult += sizeSubStruct;
            alignResult = max(alignResult, alignSubStruct);
        }
        else if(it->typeId == Typeid_Ident)
        {
            auto itIdent = (Ast_DeclaratorIdent*)it;
            auto structDef = itIdent->structDef;
            auto structDecl = Ast_GetDeclStruct(structDef);
            if(structDef->phase < CompPhase_ComputeSize)
            {
                Dg_Yield(t->graph, structDef, CompPhase_ComputeSize);
                *outcome = false;
                continue;
            }
            
            sizeResult = Funcs::Align(sizeResult, structDecl->align);
            declStruct->memberOffsets[i] = sizeResult;
            sizeResult += structDecl->size;
            alignResult = max(alignResult, structDecl->align);
        }
        else if(it->typeId == Typeid_Ptr)
        {
            // TODO: when 32-bit is supported, this should be changed
            sizeResult = Funcs::Align(sizeResult, 8);
            declStruct->memberOffsets[i] = sizeResult;
            sizeResult += 8;
            alignResult = max(alignResult, (uint32)8);
        }
        else  // Primitive types are trivial
        {
            if(it->typeId < StArraySize(typeSize))
            {
                uint32 curTypeSize = typeSize[it->typeId];
                sizeResult = Funcs::Align(sizeResult, curTypeSize);
                declStruct->memberOffsets[i] = sizeResult;
                sizeResult += curTypeSize;
                alignResult = max(alignResult, curTypeSize);
            }
        }
    }
    
    // Final size needs to be aligned
    sizeResult = Funcs::Align(sizeResult, alignResult);
    return { sizeResult, alignResult };
}

// This will later use a hash-table.
// Each scope will have its own hash-table, and the search will just be
// a linked-list traversal of hash-tables (or arrays if the number of decls is low)
Ast_Declaration* IdentResolution(Typer* t, Ast_Block* scope, Atom* ident)
{
    ProfileFunc(prof);
    
    for(Ast_Block* curScope = scope; curScope; curScope = curScope->enclosing)
    {
        for(int i = 0; i < curScope->decls.length; ++i)
        {
            if(curScope->decls[i]->name == ident)
                return curScope->decls[i];
        }
    }
    
    return 0;
}

bool AddDeclaration(Typer* t, Ast_Block* scope, Ast_Declaration* decl)
{
    // Check redefinition
    bool redefinition = false;
    bool alreadyDefined = false;
    for(int i = 0; i < scope->decls.length; ++i)
    {
        // Doesn't count as redefinition if we just find ourselves,
        // somebody could have added us earlier to the symbol table
        if(scope->decls[i]->name == decl->name)
        {
            if(scope->decls[i] != decl)
            {
                SemanticError(t, decl->where, StrLit("Redefinition, this symbol was already defined in this scope, ..."));
                SemanticErrorContinue(t, scope->decls[i]->where, StrLit("... here"));
                redefinition = true;
            }
            else
                alreadyDefined = true;
        }
    }
    
    if(!redefinition && !alreadyDefined)
    {
        scope->decls.Append(decl);
        // currentProc == null means we're top level
        if(t->currentProc && decl->kind == AstKind_VarDecl)
        {
            auto varDecl = (Ast_VarDecl*)decl;
            t->currentProc->declsFlat.Append(decl);
            varDecl->declIdx = t->currentProc->declsFlat.length - 1;
        }
    }
    
    return !redefinition;
}

// TODO: I actually assumed that this was a shallow check only, which is not true
// so I should check all the calls to this function to make sure it's used correctly
TypeInfo* GetBaseType(TypeInfo* type)
{
    while(true)
    {
        switch_nocheck(type->typeId)
        {
            default: return type;
            case Typeid_Ptr: type = ((Ast_DeclaratorPtr*)type)->baseType; break;
            case Typeid_Arr: type = ((Ast_DeclaratorArr*)type)->baseType; break;
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
        case Typeid_Ptr: type = ((Ast_DeclaratorPtr*)type)->baseType; break;
        case Typeid_Arr: type = ((Ast_DeclaratorArr*)type)->baseType; break;
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
    
    // Zero can convert into anything
    
    
    // @cleanup This should just get cleaned up
    
    // Implicitly convert arrays into pointers
    if(src->typeId == Typeid_Arr && dst->typeId == Typeid_Ptr)
    {
        auto arrayOf = ((Ast_DeclaratorArr*)src)->baseType;
        src = Typer_MakePtr(t->arena, arrayOf);
    }
    
    // Implicitly convert procs into pointers
    if(src->typeId == Typeid_Ptr && dst->typeId == Typeid_Proc)
    {
        auto base = (Ast_DeclaratorProc*)dst;
        dst = Typer_MakePtr(t->arena, base);
    }
    else if(src->typeId == Typeid_Proc && dst->typeId == Typeid_Ptr)
    {
        auto base = (Ast_DeclaratorProc*)src;
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
            auto dstPtrTo = ((Ast_DeclaratorPtr*) dst)->baseType;
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
        // If void*, everything is ok. Maybe I should have a "^raw"?
        
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
        auto base = ((Ast_DeclaratorArr*)type1)->baseType;
        type1 = Typer_MakePtr(t->arena, base);
    }
    
    // Implicitly convert arrays into pointers
    if(type2->typeId == Typeid_Arr)
    {
        auto base = ((Ast_DeclaratorArr*)type2)->baseType;
        type2 = Typer_MakePtr(t->arena, base);
    }
    
    // Implicitly convert functions into function pointers
    if(type1->typeId == Typeid_Proc)
        type1 = Typer_MakePtr(t->arena, type1);
    if(type2->typeId == Typeid_Proc)
        type2 = Typer_MakePtr(t->arena, type2);
    
    // Operations with floats promote up to floats
    if(type1->typeId == Typeid_Double || type2->typeId == Typeid_Double)
        return &primitiveTypes[Typeid_Double];
    if(type1->typeId == Typeid_Float || type2->typeId == Typeid_Float)
        return &primitiveTypes[Typeid_Float];
    
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
                auto ident1 = (Ast_DeclaratorIdent*)type1;
                auto ident2 = (Ast_DeclaratorIdent*)type2;
                if(ident1->ident == ident2->ident)
                    return true;
                return false;
            }  // Check if same ident
            case Typeid_Ptr:
            {
                if(type1->typeId != type2->typeId)
                    return false;
                type1 = ((Ast_DeclaratorPtr*)type1)->baseType;
                type2 = ((Ast_DeclaratorPtr*)type2)->baseType;
                break;
            }
            case Typeid_Arr:
            {
                if(type1->typeId != type2->typeId)
                    return false;
                type1 = ((Ast_DeclaratorArr*)type1)->baseType;
                type2 = ((Ast_DeclaratorArr*)type2)->baseType;
                break;
            }
            case Typeid_Proc:
            {
                auto proc1 = (Ast_DeclaratorProc*)type1;
                auto proc2 = (Ast_DeclaratorProc*)type2;
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
        auto arrType = Ast_MakeType<Ast_DeclaratorArr>(t->arena);
        dst = Typer_MakePtr(t->arena, arrType->baseType);
    }
    
    if(false)  // Data loss warnings
    {
        // Int and float conversions
        if(src->typeId >= Typeid_Char && src->typeId <= Typeid_Double &&
           dst->typeId >= Typeid_Char && src->typeId <= Typeid_Double)
        {
            bool isSrcFloat  = IsTypeFloat(src);
            bool isDstFloat  = IsTypeFloat(dst);
            bool isSrcSigned = IsTypeSigned(dst);
            bool isDstSigned = IsTypeSigned(dst);
            
            if(isSrcFloat == isSrcFloat)
            {
                if(!isSrcFloat && isSrcSigned != isDstSigned)
                    SemanticError(t, exprSrc->where, StrLit("Implicit conversion affects signedess"));
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
        ScratchArena scratch;
        String type1Str = TypeInfo2String(src, scratch);
        String type2Str = TypeInfo2String(dst, scratch);
        StringBuilder builder;
        builder.Append(scratch, 5, StrLit("Cannot implicitly convert type '"), type1Str, StrLit("' to '"), type2Str, StrLit("'"));
        SemanticError(t, exprSrc->where, builder.string);
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
    
    StringBuilder strBuilder;
    
    bool quit = false;
    while(!quit)
    {
        if(type->typeId <= Typeid_BasicTypesEnd && type->typeId >= Typeid_BasicTypesBegin)
        {
            TokenType tokType = PrimitiveTypeidToTokType(type->typeId);
            String tokTypeStr = TokTypeToString(tokType, scratch2);
            
            strBuilder.Append(tokTypeStr, scratch);
            quit = true;
        }
        else switch_nocheck(type->typeId)
        {
            default: Assert(false); break;
            case Typeid_None:
            {
                strBuilder.Append(StrLit("none"), scratch);
                quit = true;
                break;
            }
            case Typeid_Ident:
            {
                char* nullTerminated = ((Ast_DeclaratorIdent*)type)->ident->string;
                String ident = { nullTerminated, (int64)strlen(nullTerminated) };
                strBuilder.Append(ident, scratch);
                quit = true;
                break;
            }
            case Typeid_Ptr:
            {
                strBuilder.Append(StrLit("^"), scratch);
                type = ((Ast_DeclaratorPtr*)type)->baseType;
                break;
            }
            case Typeid_Arr:
            {
                strBuilder.Append(StrLit("[]"), scratch);
                type = ((Ast_DeclaratorArr*)type)->baseType;
                break;
            }
            case Typeid_Proc:
            {
                auto procType = (Ast_DeclaratorProc*)type;
                auto argTypesStr = Arena_AllocArray(scratch2, procType->args.length, String);
                auto retTypesStr = Arena_AllocArray(scratch2, procType->retTypes.length, String);
                
                for_array(i, procType->args)
                    argTypesStr[i] = TypeInfo2String(procType->args[i]->type, scratch2);
                
                for_array(i, procType->retTypes)
                    retTypesStr[i] = TypeInfo2String(procType->retTypes[i], scratch2);
                
                strBuilder.Append(StrLit("proc ("), scratch);
                
                for_array(i, procType->args)
                {
                    strBuilder.Append(argTypesStr[i], scratch);
                    if(i != procType->args.length - 1)
                        strBuilder.Append(StrLit(", "), scratch);
                }
                
                strBuilder.Append(StrLit(")->"), scratch);
                
                if(procType->retTypes.length > 1)
                    strBuilder.Append(StrLit("("), scratch);
                
                for_array(i, procType->retTypes)
                    strBuilder.Append(retTypesStr[i], scratch);
                
                if(procType->retTypes.length > 1)
                    strBuilder.Append(StrLit(")"), scratch);
                
                quit = true;
                break;
            }
        } switch_nocheck_end;
    }
    
    return strBuilder.ToString(dest);
}

void SemanticError(Typer* t, Token* token, String message)
{
    CompileError(t->tokenizer, token, message);
}

void SemanticErrorContinue(Typer* t, Token* token, String message)
{
    CompileErrorContinue(t->tokenizer, token, message);
}

void CannotConvertToScalarTypeError(Typer* t, TypeInfo* type, Token* where)
{
    ScratchArena scratch;
    String typeStr = TypeInfo2String(type, scratch);
    
    StringBuilder builder;
    builder.Append(scratch, 3, StrLit("Cannot convert type '"), typeStr, StrLit("' to any scalar type"));
    SemanticError(t, where, builder.string);
}

void CannotConvertToIntegralTypeError(Typer* t, TypeInfo* type, Token* where)
{
    ScratchArena scratch;
    String typeStr = TypeInfo2String(type, scratch);
    
    StringBuilder builder;
    builder.Append(scratch, 3, StrLit("Cannot convert type '"), typeStr, StrLit("' to any integral type"));
    SemanticError(t, where, builder.string);
}

void CannotDereferenceTypeError(Typer* t, TypeInfo* type, Token* where)
{
    ScratchArena scratch;
    String typeStr = TypeInfo2String(type, scratch);
    StringBuilder builder;
    builder.Append(scratch, 3, StrLit("Cannot dereference type '"), typeStr, StrLit("'"));
    SemanticError(t, where, builder.string);
}

void IncompatibleTypesError(Typer* t, TypeInfo* type1, TypeInfo* type2, Token* where)
{
    ScratchArena scratch;
    String type1Str = TypeInfo2String(type1, scratch);
    String type2Str = TypeInfo2String(type2, scratch);
    
    StringBuilder builder;
    builder.Append(scratch, 5, StrLit("The following types are incompatible: '"),
                   type1Str, StrLit("' and '"), type2Str, StrLit("'"));
    SemanticError(t, where, builder.string);
}