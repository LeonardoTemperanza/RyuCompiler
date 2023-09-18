
#pragma once

#include "base.h"
#include "tilde_backend/Cuik/common/arena.h"
#include "tb.h"

struct Tc_Context
{
    TB_Module* module = 0;
    TB_Function* proc = 0;
    TB_Arena procArena;
    
    TB_Function* mainProc = 0;
    bool emitAsm = false;
    
    // Need address if false
    bool needValue = true;
    
    // Stuff for generation from bytecode
    Arena* regArena = 0;
    Arena* symArena = 0;
    Arena* flagArena = 0;
    Array<TB_Node*> regs;
    Array<bool> isLValue;
    Array<Interp_Symbol*> syms;
    Array<TB_Node*> bbs;  // Basic Blocks
    TB_PassingRule retPassingRule;
    Array<TB_PassingRule> argPassingRules;
};

Tc_Context Tc_InitCtx(TB_Module* module, bool emitAsm);
void Tc_ResetCtx(Tc_Context* ctx);

void Tc_TestCode(Ast_FileScope* file, Interp* interp);

// From bytecode
TB_DataType Tc_ToTBType(Interp_Type type);
void Tc_ExpandRegs(Tc_Context* ctx, int idx);
void Tc_GenProc(Tc_Context* ctx, Interp_Proc* proc);
void Tc_GenInstrs(Tc_Context* ctx, TB_Function* tildeProc, Interp_Proc* proc);

TB_DataType Tc_ConvertToTildeType(TypeInfo* type);
TB_DebugType* Tc_ConvertToDebugType(TB_Module* module, TypeInfo* type);
TB_DebugType* Tc_ConvertProcToDebugType(TB_Module* module, TypeInfo* type);
