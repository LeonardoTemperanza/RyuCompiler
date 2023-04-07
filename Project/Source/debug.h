
#pragma once

#include "base.h"
#include "lexer.h"
#include "parser.h"
#include "semantics.h"

void PrintAst(Array<Ast_Node*> nodes, int offset = 0);
void PrintAst(Ast_Node* node, int offset = 0);
String AstNodeKindToString(Ast_Node* node);