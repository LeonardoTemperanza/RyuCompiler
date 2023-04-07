
#include "debug.h"

// TODO: Ast printing is not done

void PrintAst(Array<Ast_Node*> nodes, int offset)
{
    String str = Str_Literal("prova");
    
    for(int i = 0; i < nodes.length; ++i)
        PrintAst(nodes[i]);
}

void PrintAst(Ast_Node* node, int offset)
{
    String nodeKindStr = AstNodeKindToString(node);
    
    printf("%s", nodeKindStr.ptr);  // It's null terminated so it's fine
    
    switch(node->kind)
    {
        
    }
}

String AstNodeKindToString(Ast_Node* node)
{
    switch(node->kind)
    {
        default: Assert(false);
        
        case AstKind_FunctionDef:
        return Str_Literal("Func Def");
        case AstKind_Declaration:
        return Str_Literal("Decl");
        case AstKind_Declarator:
        return Str_Literal("Declarator");
        case AstKind_DeclaratorIdent:
        return Str_Literal("DeclaratorIdent");
        case AstKind_Block:
        return Str_Literal("Block");
        case AstKind_If:
        return Str_Literal("If");
        case AstKind_For:
        return Str_Literal("For");
        case AstKind_While:
        return Str_Literal("While");
        // TODO: put the rest of these
    };
}