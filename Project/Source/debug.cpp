
#include "debug.h"

// TODO: Ast printing is not done

void PrintAst(Array<Ast_Node*> nodes, int offset)
{
    String str = StrLit("prova");
    
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
        return StrLit("Func Def");
        case AstKind_Declaration:
        return StrLit("Decl");
        case AstKind_Declarator:
        return StrLit("Declarator");
        case AstKind_DeclaratorIdent:
        return StrLit("DeclaratorIdent");
        case AstKind_Block:
        return StrLit("Block");
        case AstKind_If:
        return StrLit("If");
        case AstKind_For:
        return StrLit("For");
        case AstKind_While:
        return StrLit("While");
        // TODO: put the rest of these
    };
}