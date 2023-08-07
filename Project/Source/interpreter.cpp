
#include "base.h"
#include "interpreter.h"

void Interp_Region(Interp_Proc* proc)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Region;
}

void Interp_Unreachable(Interp_Proc* proc)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Unreachable;
}

void Interp_DebugBreak(Interp_Proc* proc)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_DebugBreak;
}

void Interp_Trap(Interp_Proc* proc)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_DebugBreak;
}

void Interp_Poison(Interp_Proc* proc)
{
}

// void Interp_Param(Interp_Proc* proc) {}

void Interp_FPExt(Interp_Proc* proc)
{
}

void Interp_SExt(Interp_Proc* proc)
{
}

void Interp_ZExt(Interp_Proc* proc)
{
}

void Interp_Trunc(Interp_Proc* proc)
{
    
}

void Interp_Int2Ptr(Interp_Proc* proc)
{
    
}

void Interp_Ptr2Int(Interp_Proc* proc) {}

void Interp_Int2Float(Interp_Proc* proc) {}

void Interp_Float2Int(Interp_Proc* proc) {}

void Interp_Bitcast(Interp_Proc* proc) {}

// Local uses reg1 and reg2 to store size and alignment,
// Dest is the resulting address
void Interp_Local(Interp_Proc* proc, uint64 size, uint64 align)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op   = Op_Local;
    newElement.local.dst  = proc->regCounter++;
    newElement.local.size = size;
    newElement.local.align = align;
}

// Ignoring atomics for now
void Interp_Load(Interp_Proc* proc, Interp_Type type, RegIdx addr, uint64 align, bool isVolatile)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op   = Op_Load;
    newElement.load.dst  = proc->regCounter++;
    newElement.load.addr = addr;
    newElement.load.align = align;
    newElement.type = type;
}

void Interp_Store(Interp_Proc* proc, Interp_Type type, RegIdx addr, RegIdx val, uint64 align, bool isVolatile)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op   = Op_Store;
    newElement.store.addr = addr;
    newElement.store.align = align;
    newElement.type = type;
}

void Interp_ImmBool(Interp_Proc* proc) {}

// I don't understand why in tb uint is truncated and zexted
// but sint is not.
void Interp_ImmSInt(Interp_Proc* proc, Interp_Type type, int64 imm)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op   = Op_IntegerConst;
    newElement.imm.dst = proc->regCounter++;
    newElement.imm.val = imm;
    newElement.type = type;
}

// @uncertain
// Should I do something here? I don't think it even matters
// well... maybe it does for execution
void Interp_ImmUInt(Interp_Proc* proc, Interp_Type type, uint64 imm)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op   = Op_IntegerConst;
    newElement.imm.dst  = proc->regCounter++;
    newElement.imm.val = imm;
    newElement.type = type;
}

void Interp_Float32(Interp_Proc* proc) {}

void Interp_Float64(Interp_Proc* proc) {}

// void Interp_CString(Interp_Proc* proc) {}

// void Interp_String(Interp_Proc* proc) {}

void Interp_MemSet(Interp_Proc* proc) {}

void Interp_MemZero(Interp_Proc* proc) {}

void Interp_ArrayAccess(Interp_Proc* proc) {}

void Interp_MemberAccess(Interp_Proc* proc) {}

void Interp_GetSymbolAddress(Interp_Proc* proc) {}

void Interp_Select(Interp_Proc* proc) {}

// @incomplete
// This should support different kinds of arithmetic behavior 
void Interp_Add(Interp_Proc* proc, RegIdx reg1, RegIdx reg2)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op       = Op_Add;
    newElement.bin.dst  = proc->regCounter++;
    newElement.bin.src1 = reg1;
    newElement.bin.src2 = reg2;
}

void Interp_Sub(Interp_Proc* proc, RegIdx reg1, RegIdx reg2)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op       = Op_Sub;
    newElement.bin.dst  = proc->regCounter++;
    newElement.bin.src1 = reg1;
    newElement.bin.src2 = reg2;
}

void Interp_Mul(Interp_Proc* proc, RegIdx reg1, RegIdx reg2)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op       = Op_Mul;
    newElement.bin.dst  = proc->regCounter++;
    newElement.bin.src1 = reg1;
    newElement.bin.src2 = reg2;
}

void Interp_Div(Interp_Proc* proc, RegIdx reg1, RegIdx reg2, bool signedness)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op       = signedness ? Op_SDiv : Op_UDiv;
    newElement.bin.dst  = proc->regCounter++;
    newElement.bin.src1 = reg1;
    newElement.bin.src2 = reg2;
}

void Interp_Mod(Interp_Proc* proc, RegIdx reg1, RegIdx reg2, bool signedness)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op       = signedness ? Op_SMod : Op_UMod;
    newElement.bin.dst  = proc->regCounter++;
    newElement.bin.src1 = reg1;
    newElement.bin.src2 = reg2;
}

void Interp_BSwap(Interp_Proc* proc)
{
    
}

//void Interp_CLZ(Interp_Proc* proc) {}

//void Interp_CTZ(Interp_Proc* proc) {}

//void Interp_PopCount(Interp_Proc* proc) {}

void Interp_Not(Interp_Proc* proc) {}

void Interp_Neg(Interp_Proc* proc) {}

void Interp_And(Interp_Proc* proc) {}

void Interp_Or(Interp_Proc* proc) {}

void Interp_Xor(Interp_Proc* proc) {}

void Interp_Sar(Interp_Proc* proc) {}

void Interp_ShL(Interp_Proc* proc) {}

void Interp_ShR(Interp_Proc* proc) {}

void Interp_RoL(Interp_Proc* proc) {}

void Interp_RoR(Interp_Proc* proc) {}

void Interp_AtomicLoad(Interp_Proc* proc) {}

void Interp_AtomicExchange(Interp_Proc* proc) {}

void Interp_AtomicAdd(Interp_Proc* proc) {}

void Interp_AtomicSub(Interp_Proc* proc) {}

void Interp_AtomicAnd(Interp_Proc* proc) {}

void Interp_AtomicXor(Interp_Proc* proc) {}

void Interp_AtomicOr(Interp_Proc* proc) {}

void Interp_AtomicCmpExchange(Interp_Proc* proc) {}

void Interp_FAdd(Interp_Proc* proc) {}

void Interp_FSub(Interp_Proc* proc) {}

void Interp_FMul(Interp_Proc* proc) {}

void Interp_FDiv(Interp_Proc* proc) {}

void Interp_CmpEq(Interp_Proc* proc) {}

void Interp_CmpNe(Interp_Proc* proc) {}

void Interp_CmpILT(Interp_Proc* proc) {}

void Interp_CmpILE(Interp_Proc* proc) {}

void Interp_CmpIGT(Interp_Proc* proc) {}

void Interp_CmpIGE(Interp_Proc* proc) {}

void Interp_CmpFLT(Interp_Proc* proc) {}

void Interp_CmpFLE(Interp_Proc* proc) {}

void Interp_CmpFGT(Interp_Proc* proc) {}

void Interp_CmpFGE(Interp_Proc* proc) {}

// Intrinsics... Not supporting those for now

void Interp_SysCall(Interp_Proc* proc) {}

void Interp_Call(Interp_Proc* proc) {}

void Interp_Safepoint(Interp_Proc* proc) {}

// TODO: Continue work here
void Interp_Goto(Interp_Proc* proc, InstrIdx target)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Branch;
    newElement.branch.defaultCase = target;
    //newElement.dst  = proc->regCounter++;
    //newElement.a.c;
    //newElement.src1 = reg1;
    //newElement.src2 = reg2;
}

void Interp_If(Interp_Proc* proc) {}

void Interp_Branch(Interp_Proc* proc)
{
    
}

// At this point the registers can be reused
void Interp_EndOfExpression(Interp_Proc* proc)
{
    proc->regCounter = 0;
}