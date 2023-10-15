
#pragma once

#include "interpreter.h"

struct Builder_MiddleInsert
{
    // Used for temporary insertion
    // of instructions. The instructions
    // will then get copied and inserted
    // in the middle of the array
    Arena* arena = 0;
    Slice<Interp_Instr> instrs = { 0, 0 };
    int insertPoint = 0;
    
    // Use offset array to shift the insertion point
    // if there are nested middle insertions 
    int insertPointOffset = 0;
};

struct Interp_Builder
{
    DepGraph* graph;
    // See the dependency graph system
    bool yielded = false;
    
    // Maps declaration indices to registers that contain their
    // addresses.
    Array<RegIdx> declToAddr;
    
    // Registers used for local addresses, argument values
    // and whatnot. These are ordered, by construction.
    Array<RegIdx> permanentRegs;
    
    // Used for updating indices after inserting elements in
    // the middle of the instruction array.
    Array<RegIdx> instrOffsets;
    
    // Tells us which register to currently use.
    // Must be reset after the end of an expression
    RegIdx regCounter = 0;
    
    Interp_Proc* proc;
    Slice<Interp_Symbol> symbols;
    
    // Used for adding a potentially large array of instructions
    // in the middle of the array. These kinds of trasformations
    // are performed in bulk for performance (they're only committed
    // at the end)
    Builder_MiddleInsert middleInsert = { 0, { 0, 0 } };
    
    // Whether to skip next statements in the block or not.
    // Important for correctness, not just code size
    bool genJump = false;
    
    Array<InstrIdx> breaks;
    Array<InstrIdx> continues;
    Array<InstrIdx> fallthroughs;
    Array<Ast_Node*> deferStack;
    int curDeferIdx = 0;
    
    // NOTE: Used for getting the passing rules for arguments and return values
    TB_Module* module;
    
    ~Interp_Builder()
    {
        declToAddr.FreeAll();
        permanentRegs.FreeAll();
        instrOffsets.FreeAll();
        breaks.FreeAll();
        continues.FreeAll();
        fallthroughs.FreeAll();
    }
};

Builder_MiddleInsert Interp_StartMiddleInsertion(Interp_Builder* builder, Arena* tempArena, int insertPoint);
void Interp_CommitMiddleInsertion(Interp_Builder* builder, Builder_MiddleInsert ctx);
Interp_Instr* Interp_InsertInstr(Interp_Builder* builder);
void Interp_AdvanceReg(Interp_Builder* builder);
RegIdx Interp_GetFirstUnused(Interp_Builder* builder);
uint32 Interp_AllocateRegIdxArray(Interp_Builder* builder, RegIdx* values, uint16 arrayCount);
uint32 Interp_AllocateRegIdxArray(Interp_Builder* builder, Slice<RegIdx> values);
uint32 Interp_AllocateInstrIdxArray(Interp_Builder* builder, InstrIdx* values, uint32 arrayCount);
uint32 Interp_AllocateInstrIdxArray(Interp_Builder* builder, Slice<InstrIdx> instrs);
uint32 Interp_AllocateConstArray(Interp_Builder* builder, int64* values, uint32 count);
uint32 Interp_AllocateConstArray(Interp_Builder* builder, Slice<int64> consts);
InstrIdx Interp_UpdateInstrIdx(Interp_Builder* builder, InstrIdx idx, int oldOffset);
InstrIdx Interp_Placeholder(Interp_Builder* builder);
InstrIdx Interp_Region(Interp_Builder* builder);
void Interp_DebugBreak(Interp_Builder* builder);
// void Interp_Param(Interp_Builder* builder);
RegIdx Interp_FPExt(Interp_Builder* builder, RegIdx src, Interp_Type type);
RegIdx Interp_SExt(Interp_Builder* builder, RegIdx src, Interp_Type type);
RegIdx Interp_ZExt(Interp_Builder* builder, RegIdx src, Interp_Type type);
RegIdx Interp_Trunc(Interp_Builder* builder, RegIdx src, Interp_Type type);
RegIdx Interp_Int2Ptr(Interp_Builder* builder, RegIdx src, Interp_Type type);
RegIdx Interp_Ptr2Int(Interp_Builder* builder, RegIdx src, Interp_Type type);
RegIdx Interp_Int2Float(Interp_Builder* builder, RegIdx src, Interp_Type type, bool isSigned);
RegIdx Interp_Float2Int(Interp_Builder* builder, RegIdx src, Interp_Type type, bool isSigned);
RegIdx Interp_Bitcast(Interp_Builder* builder, RegIdx src, Interp_Type type);
RegIdx Interp_Local(Interp_Builder* builder, uint64 size, uint64 align);
RegIdx Interp_Load(Interp_Builder* builder, Interp_Type type, RegIdx addr, uint64 align, bool isVolatile);
Interp_Instr* Interp_Store(Interp_Builder* builder, Interp_Type type, RegIdx addr, RegIdx val, uint64 align, bool isVolatile);
RegIdx Interp_ImmBool(Interp_Builder* builder, bool imm);
RegIdx Interp_ImmSInt(Interp_Builder* builder, Interp_Type type, int64 imm);
RegIdx Interp_ImmUInt(Interp_Builder* builder, Interp_Type type, uint64 imm);
RegIdx Interp_ImmFloat32(Interp_Builder* builder, Interp_Type type, float imm);
RegIdx Interp_ImmFloat64(Interp_Builder* builder, Interp_Type type, double imm);
// void Interp_CString(Interp_Builder* builder);
// void Interp_String(Interp_Builder* builder);
void Interp_MemSet(Interp_Builder* builder, RegIdx dst, RegIdx val, RegIdx count, uint64 align);
void Interp_MemZero(Interp_Builder* builder, RegIdx dst, RegIdx count, uint64 align);
void Interp_MemCpy(Interp_Builder* builder, RegIdx dst, RegIdx src, RegIdx count, uint64 align, bool isVolatile);
RegIdx Interp_ArrayAccess(Interp_Builder* builder);
RegIdx Interp_MemberAccess(Interp_Builder* builder, RegIdx base, int64 offset);
RegIdx Interp_GetSymbolAddress(Interp_Builder* builder, SymIdx symbol);
void Interp_Select(Interp_Builder* builder);
RegIdx Interp_Add(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_Sub(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_Mul(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_Div(Interp_Builder* builder, RegIdx reg1, RegIdx reg2, bool signedness);
RegIdx Interp_Mod(Interp_Builder* builder, RegIdx reg1, RegIdx reg2, bool signedness);
void Interp_BSwap(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
//void Interp_CLZ(Interp_Builder* builder);
//void Interp_CTZ(Interp_Builder* builder);
//void Interp_PopCount(Interp_Builder* builder);
RegIdx Interp_Not(Interp_Builder* builder, RegIdx src);
RegIdx Interp_Neg(Interp_Builder* builder, RegIdx src);
RegIdx Interp_And(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_Or(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_Xor(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_Sar(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_ShL(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_ShR(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_RoL(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_RoR(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
void Interp_AtomicLoad(Interp_Builder* builder);
void Interp_AtomicExchange(Interp_Builder* builder);
void Interp_AtomicAdd(Interp_Builder* builder);
void Interp_AtomicSub(Interp_Builder* builder);
void Interp_AtomicAnd(Interp_Builder* builder);
void Interp_AtomicXor(Interp_Builder* builder);
void Interp_AtomicOr(Interp_Builder* builder);
void Interp_AtomicCmpExchange(Interp_Builder* builder);
RegIdx Interp_FAdd(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_FSub(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_FMul(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_FDiv(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_CmpEq(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_CmpNe(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_CmpILT(Interp_Builder* builder, RegIdx reg1, RegIdx reg2, bool isSigned);
RegIdx Interp_CmpILE(Interp_Builder* builder, RegIdx reg1, RegIdx reg2, bool isSigned);
RegIdx Interp_CmpIGT(Interp_Builder* builder, RegIdx reg1, RegIdx reg2, bool isSigned);
RegIdx Interp_CmpIGE(Interp_Builder* builder, RegIdx reg1, RegIdx reg2, bool isSigned);
RegIdx Interp_CmpFLT(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_CmpFLE(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_CmpFGT(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
RegIdx Interp_CmpFGE(Interp_Builder* builder, RegIdx reg1, RegIdx reg2);
// Intrinsics... Not supporting those for now
RegIdx Interp_SysCall(Interp_Builder* builder);
RegIdx Interp_Call(Interp_Builder* builder, RegIdx target, Slice<RegIdx> args);
void Interp_Safepoint(Interp_Builder* builder);
InstrIdx Interp_Goto(Interp_Builder* builder, InstrIdx target);
InstrIdx Interp_Goto(Interp_Builder* builder);
InstrIdx Interp_If(Interp_Builder* builder, RegIdx value, InstrIdx ifInstr, InstrIdx elseInstr);
InstrIdx Interp_If(Interp_Builder* builder, RegIdx value);
void Interp_PatchIf(Interp_Builder* builder, InstrIdx ifInstr, RegIdx value, InstrIdx thenInstr, InstrIdx elseInstr);
void Interp_PatchGoto(Interp_Builder* builder, InstrIdx gotoInstr, InstrIdx target);
void Interp_PatchBranch(Interp_Builder* builder, InstrIdx branchInstr, Slice<int64> values, Slice<InstrIdx> regions, InstrIdx defaultRegion);
InstrIdx Interp_Branch(Interp_Builder* builder);
void Interp_Return(Interp_Builder* builder, RegIdx retValue);

// Print Utilities
void Interp_PrintProc(Interp_Proc* proc, Slice<Interp_Symbol> syms);
void Interp_PrintInstr(Interp_Proc* proc, Interp_Instr* instr, Slice<Interp_Symbol> syms);
void Interp_PrintBin(Interp_Instr* instr);
void Interp_PrintUnary(Interp_Instr* instr);