
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

bool TypesIdentical(Typer* t, TypeInfo* type1, TypeInfo* type2)
{
    ProfileFunc(prof);
    
    if(type1 == type2)
        return true;
    
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

void Semantics_Expr(Typer* t, Ast_Expr* expr, Ast_Block* block)
{
    ProfileFunc(prof);
    ScratchArena scratch;
    
    switch(expr->kind)
    {
        //default: Assert(false); break;
        default: printf("not supported\n"); break;
        case AstKind_BinaryExpr:
        {
            auto bin = (Ast_BinaryExpr*)expr;
            Semantics_Expr(t, bin->lhs, block);
            Semantics_Expr(t, bin->rhs, block);
            auto type1 = bin->lhs->typeInfo;
            auto type2 = bin->rhs->typeInfo;
            if(type1 && type2)
            {
                if(!TypesIdentical(t, type1, type2))
                    InvalidConversionError(t, bin->where, type1, type2);
                else
                    bin->typeInfo = type1;
            }
            else
            {
                if(!type1)
                    printf("type1 null ");
                if(!type2)
                    printf("type2 null ");
                printf("\n");
            }
            
            break;
        }
        case AstKind_NumLiteral:
        {
            break;
        }
        case AstKind_Ident:
        {
            printf("%.*s\n", (int)expr->where->ident.length, expr->where->ident.ptr); 
            auto decl = IdentResolution(t, block, expr->where->ident);
            
            if(!decl)
                SemanticError(t, expr->where, StrLit("Undeclared identifier"));
            else
                expr->typeInfo = decl->typeInfo;
            
            break;
        }
    }
}

void Semantics_Block(Typer* t, Ast_Block* ast)
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
                Semantics_Expr(t, decl->initExpr, ast);
                if(decl->initExpr->typeInfo)
                {
                    // Check that they're the same type here
                    if(!TypesIdentical(t, decl->typeInfo, decl->initExpr->typeInfo))
                    {
                        InvalidConversionError(t, decl->where, decl->typeInfo, decl->initExpr->typeInfo);
                    }
                }
            }
        }
        else if(Ast_IsExpr(node))
        {
            Semantics_Expr(t, (Ast_Expr*)node, ast);
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
            Semantics_Block(t, &func->block);
            
            break;
        }
        case AstKind_Block:
        {
            Semantics_Block(t, (Ast_Block*)ast);
            break;
        }
    }
}

void TypingStage(Typer* t, Ast_Block* fileScope)
{
    for(int i = 0; i < fileScope->stmts.length; ++i)
        TypingStage(t, fileScope->stmts[i], fileScope);
}
