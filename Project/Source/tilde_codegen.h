
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
    Slice<Interp_Symbol> symbols;
    
    TB_Function* mainProc = 0;
    bool emitAsm = false;
    
    // Stuff for generation from bytecode
    TB_Node** regs;
    //Slice<bool> isLValue;
    SymIdx* syms;
    Tc_Region* bbs;  // Basic Blocks
    TB_PassingRule retPassingRule;
    Slice<TB_PassingRule> argPassingRules;
    
    //StringTable<TB_FunctionPrototype*> type2Proto;
    
    InstrIdx lastRegion = InstrIdx_Unused;
    
    // For logical operators (and pretty much nothing else)
    // TODO: Can this be simplified?? I'd say probably, would have
    // to think about how though
    int toMergeCount = 0;
    RegIdx toMergeIdx = RegIdx_Unused;
    TB_Node* mergeLhs = 0;
    TB_Node* mergeRhs = 0;
};

Tc_Context Tc_InitCtx(TB_Module* module, Arena* strArena, bool emitAsm);
void Tc_ResetCtx(Tc_Context* ctx);

void Tc_CodegenAndLink(Ast_FileScope* file, Interp* interp, Slice<char*> objFiles);
void Tc_Link(TB_Module* module, Slice<char*> objFiles, TB_Arch arch);

// From bytecode
void Tc_GenSymbol(Tc_Context* ctx, Interp_Symbol* symbol);
void Tc_GenProc(Tc_Context* ctx, Interp_Proc* proc);
void Tc_BackendGenProc(TB_Function* proc, TB_Arena* arena, bool emitAsm);
void Tc_GenInstrs(Tc_Context* ctx, TB_Function* tildeProc, Interp_Proc* proc);

void Tc_InitRegs(Tc_Context* ctx, uint64 numRegs, uint64 numInstrs);
void Tc_FreeRegs(Tc_Context* ctx);
TB_DataType Tc_ToTBType(Interp_Type type);
TB_Node** Tc_GetNodeArray(Tc_Context* ctx, Interp_Proc* proc, int arrayStart, int arrayCount, Arena* allocTo);
TB_Node** Tc_GetBBArray(Tc_Context* ctx, Interp_Proc* proc, int arrayStart, int arrayCount, Arena* allocTo);
TB_SwitchEntry* Tc_GetSwitchEntries(Tc_Context* ctx, Interp_Proc* proc, Interp_Instr instr, Arena* allocTo);

TB_DataType Tc_ConvertToTildeType(TypeInfo* type);
TB_DebugType* Tc_ConvertToDebugType(TB_Module* module, TypeInfo* type);
TB_DebugType* Tc_ConvertProcToDebugType(TB_Module* module, TypeInfo* type);
String Tc_StringifyDebugType(TB_DebugType* type, Arena* allocTo);
