
#include "semantics.h"

// Primitive types

Typer InitTyper(Arena* astArena, Parser* parser)
{
    Typer t;
    t.tokenizer = parser->tokenizer;
    t.parser = parser;
    t.arena = astArena;
    
    return t;
}

void SemanticError(Typer* t, Token* token, String message)
{
    CompileError(t->parser->tokenizer, token, message);
}

void InvalidConversionError(Typer* t, Token* token, TypeInfo* type1, TypeInfo* type2)
{
    ScratchArena scratch;
    String str1 = TypeInfo2String(type1, scratch);
    String str2 = TypeInfo2String(type2, scratch);
    StringBuilder strBuilder;
    strBuilder.Append(scratch, 5, StrLit("Mismatching types: '"), str1, StrLit("' and '"), str2, StrLit("'"));
    SemanticError(t, token, strBuilder.string);
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
            SemanticError(t, decl->where, StrLit("Redefinition, this was defined more than once"));
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

bool TypesCompatible(Typer* t, TypeInfo* type1, TypeInfo* type2)
{
    ProfileFunc(prof);
    
    if(type1 == type2)
        return true;
    
    // Pointers can be explicitly casted to pointers to other types
    if(type1->typeId == type2->typeId)
        return true;
    
    
    while(true)
    {
        if(type1->typeId != type2->typeId)
        {
            return true;
        }
        
        switch(type1->typeId)
        {
            default: return 0;
            case AstKind_DeclaratorIdent: return true;  // Check if same ident
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
    
    return false;
}

TypeInfo* GetCommonType(Typer* t, TypeInfo* type1, TypeInfo* type2)
{
    // Implicitly convert arrays into pointers
    if(type1->typeId == Typeid_Arr) return 0;  // TODO TODO TODO Return pointer here
    
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

inline bool IsTypeFloat(TypeInfo* type)
{
    return type->typeId == Typeid_Float || type->typeId == Typeid_Double;
}

inline bool IsTypeSigned(TypeInfo* type)
{
    // Floating point types are automatically signed
    if(IsTypeFloat(type)) return true;
    
    if(type->typeId >= Typeid_Char && type->typeId <= Typeid_Int64) return true;
    
    return false;
}

inline bool IsTypeDereferenceable(TypeInfo* type)
{
    return type->typeId == Typeid_Ptr || type->typeId == Typeid_Arr;
}

bool ImplicitConversion(Typer* t, Ast_Expr* exprSrc, TypeInfo* src, TypeInfo* dst)
{
    ProfileFunc(prof);
    
    // Implicitly convert functions & arrays into pointers
    if(dst->typeId == Typeid_Proc)
        dst = Ast_MakeNode<Ast_DeclaratorPtr>(t->arena, 0);
    else if(dst->typeId == Typeid_Arr)
        dst = Ast_MakeNode<Ast_DeclaratorArr>(t->arena, 0);
    
    if(true)  // Data loss warnings
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
        SemanticError(t, exprSrc->where, StrLit("Could not implicitly convert type "));
        return false;
    }
    
    return true;
}

// @incomplete
bool TypesImplicitlyCompatible(Typer* t, TypeInfo* type1, TypeInfo* type2)
{
    if(type1 == type2)
        return true;
    
    return false;
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
                strBuilder.Append(StrLit("ident"), scratch);
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

void CheckExpression(Typer* t, Ast_Expr* expr, Ast_Block* block)
{
    ProfileFunc(prof);
    ScratchArena scratch;
    
    // Have i visited this already?
    
    switch(expr->kind)
    {
        //default: Assert(false); break;
        default: printf("not supported\n"); break;
        case AstKind_BinaryExpr:
        {
            auto bin = (Ast_BinaryExpr*)expr;
            CheckExpression(t, bin->lhs, block);
            CheckExpression(t, bin->rhs, block);
            
            auto ltype = bin->lhs->type;
            auto rtype = bin->rhs->type;
            
            if(!ltype || !rtype)
                break;
            
            if((bin->binaryOp == '+' || bin->binaryOp == '-') &&
               (IsTypeDereferenceable(ltype) || IsTypeDereferenceable(rtype)))  // Pointer arithmetic
            {
                if(bin->binaryOp == '+' && (rtype->typeId == Typeid_Ptr || rtype->typeId == Typeid_Arr))
                {
                    // Swap 
                    Swap(TypeInfo*, ltype, rtype);
                    Swap(Ast_Expr*, bin->lhs, bin->rhs);
                }
                
                if(IsTypeDereferenceable(rtype))
                {
                    if(bin->binaryOp == '-')  // Pointer difference is allowed, ...
                    {
                        bin->lhs->castType = bin->lhs->type;
                        bin->rhs->castType = bin->rhs->type;
                        bin->type = ltype;
                    }
                    else  // ... unlike pointer addition
                    {
                        SemanticError(t, bin->where, StrLit("Pointer addition is not allowed, one must be integral"));
                    }
                }
                else  // Ptr + integral
                {
                    bin->lhs->castType = bin->lhs->type;
                    bin->rhs->castType = &primitiveTypes[Typeid_Uint64];
                    
                    // Need to know the size of lhs!!! (and potentially wait until you get it)
                    // Maybe not right now... I just need it later in code generation
                    
                    bin->type = bin->lhs->type;
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
                    b.Append(scratch, 4, StrLit("Cannot apply binary operator to "), ltypeStr, StrLit(" and "), rtypeStr);
                    SemanticError(t, bin->where, b.string);
                    break;
                }
                
                TypeInfo* commonType = GetCommonType(t, ltype, rtype);
                ImplicitConversion(t, bin->lhs, ltype, commonType);
                ImplicitConversion(t, bin->rhs, rtype, commonType);
                
                bin->lhs->castType = commonType;
                bin->rhs->castType = commonType;
                bin->type = commonType;
            }
            
            break;
        }
        case AstKind_NumLiteral:
        {
            break;
        }
        case AstKind_Ident:
        {
            auto decl = IdentResolution(t, block, expr->where->ident);
            
            if(!decl)
                SemanticError(t, expr->where, StrLit("Undeclared identifier"));
            else
                expr->type = decl->type;
            
            break;
        }
    }
}

void CheckBlock(Typer* t, Ast_Block* ast)
{
    ProfileFunc(prof);
    ScratchArena scratch;
    
    for(int i = 0; i < ast->stmts.length; ++i)
    {
        Ast_Node* node = ast->stmts[i];
        if(node->kind == AstKind_Declaration)
        {
            auto decl = (Ast_Declaration*)node;
            AddDeclaration(t, ast, decl);
            if(decl->initExpr)
            {
                CheckExpression(t, decl->initExpr, ast);
                if(decl->initExpr->type)
                {
                    // Check that they're the same type here
                    if(!TypesIdentical(t, decl->type, decl->initExpr->type))
                    {
                        InvalidConversionError(t, decl->where, decl->type, decl->initExpr->type);
                    }
                }
            }
        }
        else if(Ast_IsExpr(node))
        {
            CheckExpression(t, (Ast_Expr*)node, ast);
        }
    }
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

void TypingStage(Typer* t, Ast_Block* fileScope)
{
    for(int i = 0; i < fileScope->stmts.length; ++i)
        TypingStage(t, fileScope->stmts[i], fileScope);
}
