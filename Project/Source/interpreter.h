
#pragma once

#include "base.h"
#include "memory_management.h"

typedef double Value;

enum Interp_OpCodeEnum
{
    Op_Null = 0,
    
    // Immediates
    Op_IntegerConst,
    Op_Float32Const,
    Op_Float64Const,
    
    // Start of proc (Don't think i need this)
    Op_Start,
    // Start of basic block
    Op_Region,
    
    Op_Call,
    Op_SysCall,
    
    // Memory ops
    Op_Store,
    Op_MemCpy,
    Op_MemSet,
    
    // Atomics
    Op_AtomicTestAndSet,
    Op_AtomicClear,
    
    Op_AtomicLoad,
    Op_AtomicExchange,
    Op_AtomicAdd,
    Op_AtomicSub,
    Op_AtomicAnd,
    Op_AtomicXor,
    Op_AtomicOr,
    
    Op_AtomicCompareExchange,
    Op_DebugBreak,
    
    // Basic block terminators
    
    Op_Branch,  // Works like a switch (so, any number of cases)
    Op_Ret,
    Op_Unreachable,  // Do I need this?
    Op_Trap,
    Op_Poison,  // Don't think I need this
    
    // Load
    Op_Load,
    
    // Pointers
    Op_Local,
    Op_GetSymbolAddress,
    Op_MemberAccess,
    Op_ArrayAccess,
    
    // Conversions
    Op_Truncate,
    Op_FloatExt,
    Op_SignExt,
    Op_ZeroExt,
    Op_Int2Ptr,
    Op_Uint2Float,
    Op_Float2Uint,
    Op_Int2Float,
    Op_Float2Int,
    Op_Bitcast,
    
    // Select
    Op_Select,
    
    // Bitmagic
    Op_BitSwap,
    Op_Clz,
    Op_Ctz,
    Op_PopCnt,
    
    // Unary operations
    Op_Not,
    Op_Negate,
    
    // Integer arithmetic
    Op_And,
    Op_Or,
    Op_Xor,
    Op_Add,
    Op_Sub,
    Op_Mul,
    
    Op_ShL,
    Op_ShR,
    Op_Sar,
    Op_Rol,
    Op_Ror,
    Op_UDiv,
    Op_SDiv,
    Op_UMod,
    Op_SMod,
    
    // Float arithmetic
    Op_FAdd,
    Op_FSub,
    Op_FMul,
    Op_FDiv,
    
    // Comparisons
    Op_CmpEq,
    Op_CmpNe,
    Op_CmpULT,
    Op_CmpULE,
    Op_CmpSLT,
    Op_CmpSLE,
    Op_CmpFLT,
    Op_CmpFLE,
    
    // TB has instructions for full multiplication, phi,
    // variadic stuff, and x86 intrinsics. Phi and variadic
    // stuff is not needed, and the other instructions could
    // simply be added later.
};

typedef uint8 Interp_OpCode;

typedef uint16 RegIdx;
typedef uint8 TypeIdx;
typedef uint32 InstrIdx;

enum Interp_InstrBitfield
{
    // ...
};

struct Interp_Instr
{
    Interp_OpCode op;
    RegIdx dst, src1, src2;
    
    // For some instructions
    Array<RegIdx> array1;
    Array<RegIdx> array2;
    
    uint8 bitfield;
    
    // Type stuff
    uint8 type;
    uint8 width;
    uint16 data;
};

struct Interp_Proc
{
    DynArray<Interp_Instr> instrs;
};

struct Interp
{
    Typer* typer;
    DepGraph* graph;
    
    Arena arena;
    DynArray<Ast_DeclaratorStruct*> structs;
    DynArray<Ast_DeclaratorProc*> procHeaders;
    DynArray<Interp_Proc> procs;
};

struct VirtualMachine
{
    Arena stackArena;
    
    uchar* retStack;
    size_t stackFrameAddress;
    size_t programCounter;
};

// Bytecode instruction generation
void Interp_Region(Interp_Proc* proc);
void Interp_Unreachable(Interp_Proc* proc);
void Interp_DebugBreak(Interp_Proc* proc);
void Interp_Trap(Interp_Proc* proc);
void Interp_Poison(Interp_Proc* proc);
// void Interp_Param(Interp_Proc* proc);
void Interp_FPExt(Interp_Proc* proc);
void Interp_SExt(Interp_Proc* proc);
void Interp_ZExt(Interp_Proc* proc);
void Interp_Trunc(Interp_Proc* proc);
void Interp_Int2Ptr(Interp_Proc* proc);
void Interp_Ptr2Int(Interp_Proc* proc);
void Interp_Int2Float(Interp_Proc* proc);
void Interp_Float2Int(Interp_Proc* proc);
void Interp_Bitcast(Interp_Proc* proc);
void Interp_Local(Interp_Proc* proc);
void Interp_Load(Interp_Proc* proc);
void Interp_Store(Interp_Proc* proc);
void Interp_ImmBool(Interp_Proc* proc);
void Interp_ImmSInt(Interp_Proc* proc);
void Interp_ImmUInt(Interp_Proc* proc);
void Interp_Float32(Interp_Proc* proc);
void Interp_Float64(Interp_Proc* proc);
// void Interp_CString(Interp_Proc* proc);
// void Interp_String(Interp_Proc* proc);
void Interp_MemSet(Interp_Proc* proc);
void Interp_MemZero(Interp_Proc* proc);
void Interp_MemZero(Interp_Proc* proc);
void Interp_ArrayAccess(Interp_Proc* proc);
void Interp_MemberAccess(Interp_Proc* proc);
void Interp_GetSymbolAddress(Interp_Proc* proc);
void Interp_Select(Interp_Proc* proc);
void Interp_Add(Interp_Proc* proc);
void Interp_Sub(Interp_Proc* proc);
void Interp_Mul(Interp_Proc* proc);
void Interp_Div(Interp_Proc* proc);
void Interp_Mod(Interp_Proc* proc);
void Interp_BSwap(Interp_Proc* proc);
//void Interp_CLZ(Interp_Proc* proc);
//void Interp_CTZ(Interp_Proc* proc);
//void Interp_PopCount(Interp_Proc* proc);
void Interp_Not(Interp_Proc* proc);
void Interp_Neg(Interp_Proc* proc);
void Interp_And(Interp_Proc* proc);
void Interp_Or(Interp_Proc* proc);
void Interp_Xor(Interp_Proc* proc);
void Interp_Sar(Interp_Proc* proc);
void Interp_ShL(Interp_Proc* proc);
void Interp_ShR(Interp_Proc* proc);
void Interp_RoL(Interp_Proc* proc);
void Interp_RoR(Interp_Proc* proc);
void Interp_AtomicLoad(Interp_Proc* proc);
void Interp_AtomicExchange(Interp_Proc* proc);
void Interp_AtomicAdd(Interp_Proc* proc);
void Interp_AtomicSub(Interp_Proc* proc);
void Interp_AtomicAnd(Interp_Proc* proc);
void Interp_AtomicXor(Interp_Proc* proc);
void Interp_AtomicOr(Interp_Proc* proc);
void Interp_AtomicCmpExchange(Interp_Proc* proc);
void Interp_FAdd(Interp_Proc* proc);
void Interp_FSub(Interp_Proc* proc);
void Interp_FMul(Interp_Proc* proc);
void Interp_FDiv(Interp_Proc* proc);
void Interp_CmpEq(Interp_Proc* proc);
void Interp_CmpNe(Interp_Proc* proc);
void Interp_CmpILT(Interp_Proc* proc);
void Interp_CmpILE(Interp_Proc* proc);
void Interp_CmpIGT(Interp_Proc* proc);
void Interp_CmpIGE(Interp_Proc* proc);
void Interp_CmpFLT(Interp_Proc* proc);
void Interp_CmpFLE(Interp_Proc* proc);
void Interp_CmpFGT(Interp_Proc* proc);
void Interp_CmpFGE(Interp_Proc* proc);
// Intrinsics... Not supporting those for now
void Interp_SysCall(Interp_Proc* proc);
void Interp_Call(Interp_Proc* proc);
void Interp_Safepoint(Interp_Proc* proc);
void Interp_Goto(Interp_Proc* proc);
void Interp_If(Interp_Proc* proc);
void Interp_Branch(Interp_Proc* proc);

// AST -> bytecode conversion
void Interp_ConvertNode(Interp_Proc* proc, Ast_Node* node);
void Interp_ConvertProc(Interp_Proc* proc, Ast_Node* node);
// ...

// Code execution
void Interp_ExecProc(Interp_Proc* proc);
