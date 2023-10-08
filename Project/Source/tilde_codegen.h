
#pragma once

#include "base.h"
#include "tilde_backend/Cuik/common/arena.h"
#include "tb.h"

struct Tc_Region
{
    TB_Node* region = 0;
    TB_Node* phiValue1 = 0;
    TB_Node* phiValue2 = 0;
    RegIdx phiReg = RegIdx_Unused;
};

// TODO: @performance "bbs" should probably be a hashtable
// or something similar, or maybe it should be an interpreter thing

struct Tc_Context
{
    TB_Module* module = 0;
    TB_Function* proc = 0;
    TB_Arena procArena;
    
    TB_Function* mainProc = 0;
    bool emitAsm = false;
    
    // Stuff for generation from bytecode
    Arena* regArena = 0;
    Arena* symArena = 0;
    Arena* flagArena = 0;
    Slice<TB_Node*> regs;
    Slice<bool> isLValue;
    Slice<Interp_Symbol*> syms;
    Slice<Tc_Region> bbs;  // Basic Blocks
    TB_PassingRule retPassingRule;
    Slice<TB_PassingRule> argPassingRules;
    
    InstrIdx lastRegion = InstrIdx_Unused;
    
    // For logical operators (and pretty much nothing else)
    // TODO: Can this be simplified?? I'd say probably, don't
    // know how though, yet
    int toMergeCount = 0;
    RegIdx toMergeIdx = RegIdx_Unused;
    TB_Node* mergeLhs = 0;
    TB_Node* mergeRhs = 0;
};

Tc_Context Tc_InitCtx(TB_Module* module, bool emitAsm);
void Tc_ResetCtx(Tc_Context* ctx);

void Tc_CodegenAndLink(Ast_FileScope* file, Interp* interp, Slice<char*> objFiles);

// From bytecode
TB_DataType Tc_ToTBType(Interp_Type type);
void Tc_ExpandRegs(Tc_Context* ctx, int idx);
void Tc_GenSymbol(Tc_Context* ctx, Interp_Symbol* symbol);
void Tc_GenProc(Tc_Context* ctx, Interp_Proc* proc);
void Tc_GenInstrs(Tc_Context* ctx, TB_Function* tildeProc, Interp_Proc* proc);

TB_DataType Tc_ConvertToTildeType(TypeInfo* type);
TB_DebugType* Tc_ConvertToDebugType(TB_Module* module, TypeInfo* type);
TB_DebugType* Tc_ConvertProcToDebugType(TB_Module* module, TypeInfo* type);
