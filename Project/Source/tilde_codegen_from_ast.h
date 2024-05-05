
#pragma once

#include "base.h"
#include "tb.h"

struct CG_Ctx
{
    TB_Module* module;
};

CG_Ctx CG_Init();
//bool Codegen(Interp* interp, Ast_Node* node);

TB_Function* CG_GenProc();
