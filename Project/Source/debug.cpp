
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
        case AstKind_FunctionDef:
        {
            
        }
        break;
        case AstKind_Block: break;
        case AstKind_If: break;
        case AstKind_For: break;
        case AstKind_While: break;
        case AstKind_DoWhile: break;
        case AstKind_Switch: break;
        case AstKind_Literal: break;
        case AstKind_Ident: break;
        case AstKind_FuncCall: break;
        case AstKind_BinaryExpr: break;
        case AstKind_UnaryExpr: break;
        case AstKind_TernaryExpr: break;
        case AstKind_Typecast: break;
        case AstKind_Subscript: break;
        case AstKind_MemberAccess: break;
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