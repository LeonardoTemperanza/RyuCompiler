
#include "semantics.h"

// Primitive types

Typer InitTyper(Arena* arena, Parser* parser)
{
    Typer t;
    t.tokenizer = parser->tokenizer;
    t.parser = parser;
    t.arena = arena;
    
    return t;
}

Ast_DeclaratorPtr* Typer_MakePtr(Typer* t, TypeInfo* base)
{
    auto ptr = Arena_AllocAndInitPack(t->arena, Ast_DeclaratorPtr);
    ptr->baseType = base;
    return ptr;
}

void TypingStage(Typer* t, Ast_Block* fileScope)
{
    for(int i = 0; i < fileScope->stmts.length; ++i)
        TypingStage(t, fileScope->stmts[i], fileScope);
}

// Perform typing stage for a single "compilation unit",
// walks through the entire tree and fills in the type info
void TypingStage(Typer* t, Ast_Node* ast, Ast_Block* curScope)
{
    ProfileFunc(prof);
    
    switch(ast->kind)
    {
        default: Assert(false && "Not implemented yet"); break;
        case AstKind_StructDef:
        {
            // This could be stopped by ctfe
            AddDeclaration(t, curScope, (Ast_Declaration*)ast);
            break;
        }
        case AstKind_FunctionDef:
        {
            auto func = (Ast_FunctionDef*)ast;
            
            // Declaration (could be stopped here by ctfe or by types)
            AddDeclaration(t, curScope, &func->decl);
            
            // Definition
            CheckBlock(t, &func->block);
            
            break;
        }
        case AstKind_Block:
        {
            CheckBlock(t, (Ast_Block*)ast);
            break;
        }
    }
}

TypeInfo* CheckArithmeticExpr(Typer* t, Ast_BinaryExpr* bin, Ast_Block* block)
{
    ScratchArena scratch;
    
    CheckExpression(t, bin->lhs, block);
    CheckExpression(t, bin->rhs, block);
    
    auto lhs = bin->lhs;
    auto rhs = bin->rhs;
    auto ltype = lhs->type;
    auto rtype = rhs->type;
    
    if(!ltype || !rtype)
        return bin->type;
    
    if((bin->op == '+' || bin->op == '-') &&
       (IsTypeDereferenceable(ltype) || IsTypeDereferenceable(rtype)))  // Pointer arithmetic
    {
        if(IsTypeDereferenceable(rtype))
        {
            // Swap 
            Swap(TypeInfo*, ltype, rtype);
            Swap(Ast_Expr*, lhs, rhs);
        }
        
        if(IsTypeDereferenceable(rtype))
        {
            if(bin->op == '-')  // Pointer difference is allowed, ...
            {
                if(!TypesIdentical(t, ltype, rtype))
                {
                    String ltypeStr = TypeInfo2String(ltype, scratch);
                    String rtypeStr = TypeInfo2String(rtype, scratch);
                    StringBuilder builder;
                    builder.Append(StrLit("Pointer difference disallowed for pointers of different base types ('"), scratch);
                    builder.Append(scratch, 4, ltypeStr, StrLit("' and '"), rtypeStr, StrLit("')"));
                    SemanticError(t, bin->where, builder.string);
                    bin->type = 0;
                    return bin->type;
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
            }
        }
        else  // Ptr + integral
        {
            lhs->castType = lhs->type;
            rhs->castType = &primitiveTypes[Typeid_Uint64];
            
            if(!IsTypeIntegral(rtype))
            {
                String ltypeStr = TypeInfo2String(ltype, scratch);
                String rtypeStr = TypeInfo2String(rtype, scratch);
                StringBuilder b;
                b.Append(scratch, 5, StrLit("Cannot apply binary operator to '"), ltypeStr,
                         StrLit("' and '"), rtypeStr, StrLit("'"));
                SemanticError(t, bin->where, b.string);
            }
            
            // Need to know the size of lhs!!! (and potentially wait until you get it)
            // Maybe not right now... I just need it later in code generation
            
            bin->type = lhs->type;
        }
    }
    else  // Any other binary operation
    {
        if(!(ltype->typeId >= Typeid_Bool && ltype->typeId <= Typeid_Double &&
             rtype->typeId >= Typeid_Bool && rtype->typeId <= Typeid_Double))
        {
            // NOTE: This is where you would wait for an operator to be overloaded, maybe
            
            String ltypeStr = TypeInfo2String(ltype, scratch);
            String rtypeStr = TypeInfo2String(rtype, scratch);
            StringBuilder b;
            b.Append(scratch, 5, StrLit("Cannot apply binary operator to '"), ltypeStr,
                     StrLit("' and '"), rtypeStr, StrLit("'"));
            SemanticError(t, bin->where, b.string);
            return bin->type;
        }
        
        TypeInfo* commonType = GetCommonType(t, ltype, rtype);
        ImplicitConversion(t, lhs, ltype, commonType);
        ImplicitConversion(t, rhs, rtype, commonType);
        
        bin->lhs->castType = commonType;
        bin->rhs->castType = commonType;
        bin->type = commonType;
    }
    
    return bin->type;
}

TypeInfo* CheckBinExpr(Typer* t, Ast_BinaryExpr* bin, Ast_Block* block)
{
    switch(bin->op)
    {
        // Arithmetic expressions
        default: Assert(false && "Invalid expression"); break;
        case '+': case '-':
        case '*': case '/':
        case '%':
        case Tok_And: case Tok_Or:
        case '^':
        case Tok_LShift: case Tok_RShift:
        {
            CheckArithmeticExpr(t, bin, block);
            break;
        }
        // Comparisons
        case Tok_EQ: case Tok_NEQ:
        case Tok_GE: case Tok_LE:
        case '>': case '<':
        {
            CheckExpression(t, bin->lhs, block);
            CheckExpression(t, bin->rhs, block);
            if(TypesCompatible(t, bin->lhs->type, bin->rhs->type))
            {
                TypeInfo* commonType = GetCommonType(t, bin->lhs->type, bin->rhs->type);
                //bin->type = commonType; // ??
                bin->type = &primitiveTypes[Typeid_Bool];
            }
            else
            {
                IncompatibleTypesError(t, bin->lhs->type, bin->rhs->type, bin->where);
            }
        }
        // Assign
        case '=': 
        case Tok_PlusEquals: case Tok_MinusEquals:
        case Tok_MulEquals: case Tok_DivEquals:
        case Tok_ModEquals: case Tok_AndEquals:
        case Tok_OrEquals: case Tok_XorEquals:
        case Tok_LShiftEquals: case Tok_RShiftEquals:
        {
            if(!IsExprAssignable(bin->lhs))
            {
                SemanticError(t, bin->lhs->where, StrLit("Left-hand side of assignment is not assignable"));
                
                bin->type = 0;
            }
            else
                bin->type = &primitiveTypes[Typeid_Bool];
        }
    }
    
    return bin->type;
}

TypeInfo* CheckExpression(Typer* t, Ast_Expr* expr, Ast_Block* block)
{
    ProfileFunc(prof);
    ScratchArena scratch;
    
    // Have i visited this already?
    
    switch(expr->kind)
    {
        default: printf("Not supported\n"); break;
        case AstKind_BinaryExpr: CheckBinExpr(t, (Ast_BinaryExpr*)expr, block); break;
        case AstKind_NumLiteral: break;
        case AstKind_Ident:
        {
            auto decl = IdentResolution(t, block, expr->where->ident);
            
            if(!decl)
                SemanticError(t, expr->where, StrLit("Undeclared identifier"));
            else
            {
                expr->type = decl->type;
                auto identExpr = (Ast_IdentExpr*)expr;
                identExpr->declaration = decl;
            }
            
            break;
        }
        case AstKind_UnaryExpr:
        {
            
            Assert(false);
            break;
        }
        case AstKind_Subscript:
        {
            auto subscriptExpr = (Ast_Subscript*)expr;
            CheckExpression(t, subscriptExpr->array, block);
            
            if(!IsTypeDereferenceable(subscriptExpr->array->type))
            {
                CannotDereferenceTypeError(t, subscriptExpr->array->type, subscriptExpr->array->where);
                expr->type = 0;
            }
            else
            {
                
            }
            
            // Static bounds-checking if it's an array?
            break;
        }
    }
    
    return expr->type;
}

TypeInfo* CheckStmt(Typer* t, Ast_Stmt* stmt, Ast_Block* block)
{
    if(Ast_IsExpr(stmt))
        return CheckExpression(t, (Ast_Expr*)stmt, block);
    else switch(stmt->kind)
    {
        default: printf("Not supported\n"); break;
        case AstKind_EmptyStmt: break;
        case AstKind_Block: CheckBlock(t, (Ast_Block*)stmt); break;
        case AstKind_Declaration:
        {
            auto decl = (Ast_Declaration*)stmt;
            AddDeclaration(t, block, decl);
            if(decl->initExpr)
            {
                CheckExpression(t, decl->initExpr, block);
                if(decl->initExpr->type)
                {
                    // Check that they're the same type here
                    /*
                    if(!TypesIdentical(t, decl->type, decl->initExpr->type))
                    {
                        InvalidConversionError(t, decl->where, decl->type, decl->initExpr->type);
                    }*/
                }
            }
            
            return ((Ast_Declaration*) stmt)->type;
        }
        case AstKind_Return:
        {
            auto retStmt = (Ast_Return*)stmt;
            if(retStmt->expr)
            {
                // TODO: add this
                //TypeInfo* exprType = retStmt->expr->type;
                //TypeInfo* retType  = 
            }
            else  // Check if this function is supposed to have one or more ret types
            {
                
            }
            
            break;
        }
        case AstKind_If:
        {
            auto ifStmt = (Ast_If*)stmt;
            
            // CheckStmt is performed because it might be a declaration
            TypeInfo* condType = CheckStmt(t, (Ast_Stmt*)ifStmt->condition, ifStmt->thenBlock);
            if(condType)
            {
                if(!IsTypeScalar(condType))
                    CannotConvertToScalarTypeError(t, condType, ifStmt->condition->where);
                else
                {
                    CheckBlock(t, ifStmt->thenBlock);
                    
                    if(ifStmt->elseStmt)
                        CheckStmt(t, ifStmt->elseStmt, block);
                }
            }
            
            break;
        }
        case AstKind_While:
        {
            auto whileStmt = (Ast_While*)stmt;
            
            // CheckStmt is performed because it might be a declaration
            TypeInfo* condType = CheckStmt(t, (Ast_Stmt*)whileStmt->condition, whileStmt->doBlock);
            if(condType)
            {
                if(!IsTypeScalar(condType))
                    CannotConvertToScalarTypeError(t, condType, whileStmt->condition->where);
                else
                    CheckBlock(t, whileStmt->doBlock);
            }
            
            break;
        }
        case AstKind_DoWhile:
        {
            auto doWhileStmt = (Ast_DoWhile*)stmt;
            
            CheckStmt(t, doWhileStmt->doStmt, block);
            TypeInfo* condType = CheckExpression(t, doWhileStmt->condition, block);
            if(condType)
            {
                if(!IsTypeScalar(condType))
                    CannotConvertToScalarTypeError(t, condType, doWhileStmt->condition->where);
            }
            
            break;
        }
        case AstKind_For:
        {
            
            break;
        }
        // Missing: Switch, case, default, continue, break.
    }
    
    return 0;
}

void CheckBlock(Typer* t, Ast_Block* ast)
{
    ProfileFunc(prof);
    ScratchArena scratch;
    
    for(int i = 0; i < ast->stmts.length; ++i)
    {
        Ast_Node* node = ast->stmts[i];
        if(Ast_IsStmt(node))
            CheckStmt(t, (Ast_Stmt*)node, ast);
        else Assert(false);
    }
}

// This will later use a hash-table.
// Each scope will have its own hash-table, and the search will just be
// a linked-list traversal of hash-tables (or arrays if the number of decls is low)
Ast_Declaration* IdentResolution(Typer* t, Ast_Block* scope, String ident)
{
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

bool CheckRedefinition(Typer* t, Ast_Block* scope, Ast_Declaration* decl)
{
    // If the same name was found x
    for(int i = 0; i < scope->decls.length; ++i)
    {
        if(scope->decls[i]->name == decl->name)
        {
            SemanticError(t, decl->where, StrLit("Redefinition, this variable was already defined in this scope, ..."));
            SemanticErrorContinue(t, scope->decls[i]->where, StrLit("... here"));
            return false;
        }
    }
    
    return true;
}

void AddDeclaration(Typer* t, Ast_Block* scope, Ast_Declaration* decl)
{
    if(!CheckRedefinition(t, scope, decl))
        return;
    
    scope->decls.AddElement(decl);
}

TypeInfo* GetBaseType(TypeInfo* type)
{
    while(true)
    {
        switch(type->typeId)
        {
            default: return 0;
            case AstKind_DeclaratorIdent: return type;
            case AstKind_DeclaratorPtr: type = ((Ast_DeclaratorPtr*)type)->baseType; break;
            case AstKind_DeclaratorArr: type = ((Ast_DeclaratorArr*)type)->baseType; break;
        }
    }
    
    Assert(false && "Unreachable code");
    return 0;
}

bool TypesCompatible(Typer* t, TypeInfo* src, TypeInfo* dst)
{
    ProfileFunc(prof);
    
    if(src == dst)
        return true;
    
    // Zero can convert into anything
    
    // Implicitly convert arrays into pointers
    if(src->typeId == Typeid_Arr && dst->typeId == Typeid_Ptr)
    {
        auto arrayOf = ((Ast_DeclaratorArr*)src)->baseType;
        src = Typer_MakePtr(t, arrayOf);
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
    
    // src and dst are superficially the same type
    if(src->typeId == Typeid_Proc)
    {
        
        return TypesIdentical(t, src, dst);
    }
    else if(src->typeId == Typeid_Ptr)
    {
        // If void*, everything is ok.
        
        return TypesIdentical(t, src, dst);
    }
    
    // Allow kind matching for most things (bool, int, float, etc.)
    return true;
}

TypeInfo* GetCommonType(Typer* t, TypeInfo* type1, TypeInfo* type2)
{
    // Implicitly convert arrays into pointers
    if(type1->typeId == Typeid_Arr)
    {
        auto arrayOf = ((Ast_DeclaratorArr*)type1)->baseType;
        type1 = Typer_MakePtr(t, arrayOf);
    }
    
    // Implicitly convert arrays into pointers
    if(type2->typeId == Typeid_Arr)
    {
        auto arrayOf = ((Ast_DeclaratorArr*)type2)->baseType;
        type2 = Typer_MakePtr(t, arrayOf);
    }
    
    // Implicitly convert functions into function pointers
    
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
        
        switch(type1->typeId)
        {
            default: return 0;
            case AstKind_DeclaratorIdent:
            {
                auto ident1 = (Ast_DeclaratorIdent*)type1;
                auto ident2 = (Ast_DeclaratorIdent*)type2;
                if(ident1->ident == ident2->ident)
                    return true;
                return false;
            }  // Check if same ident
            case AstKind_DeclaratorPtr:
            {
                if(type1->typeId != type2->typeId)
                    return false;
                type1 = ((Ast_DeclaratorPtr*)type1)->baseType;
                type2 = ((Ast_DeclaratorPtr*)type2)->baseType;
                break;
            }
            case AstKind_DeclaratorArr:
            {
                if(type1->typeId != type2->typeId)
                    return false;
                type1 = ((Ast_DeclaratorArr*)type1)->baseType;
                type2 = ((Ast_DeclaratorArr*)type2)->baseType;
                break;
            }
        }
    }
    
    return true;
}

bool ImplicitConversion(Typer* t, Ast_Expr* exprSrc, TypeInfo* src, TypeInfo* dst)
{
    ProfileFunc(prof);
    
    // Implicitly convert functions & arrays into pointers
    if(dst->typeId == Typeid_Proc)
        dst = Ast_MakeNode<Ast_DeclaratorPtr>(t->arena, 0);
    else if(dst->typeId == Typeid_Arr)
        dst = Ast_MakeNode<Ast_DeclaratorArr>(t->arena, 0);
    
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
        builder.Append(scratch, 5, StrLit("Could not implicitly convert type '"), type1Str, StrLit("' and '"), type2Str, StrLit("'"));
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
        else switch(type->typeId)
        {
            default: Assert(false); break;
            case Typeid_Ident:
            {
                strBuilder.Append(((Ast_DeclaratorIdent*)type)->ident, scratch);
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
        }
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