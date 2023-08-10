
#pragma once

#include "base.h"
#include "memory_management.h"

struct Interp_Proc;

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
    
    // Works like a switch (so, any number of cases),
    // Can also be used as a goto (no condition and one case)
    Op_Branch,
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

// A 16-bit register means that you can use 65536 temporary registers,
// which means that an expression can only be so long. That seams pretty
// reasonable to me, as it means that a sum can only be 65536 operands long,
// for example. If I don't want to impose a limit, I guess in the extreme
// case a form of register spilling could be implemented?
typedef uint16 RegIdx;
typedef uint8 TypeIdx;
typedef uint32 InstrIdx;  // Max of 4 billion instructions per proc
typedef uint32 ProcIdx;   // Max of 4 billion procedures

struct Interp_RegInterval
{
    RegIdx minReg;
    RegIdx maxReg;
};

enum Interp_InstrBitfield
{
    // ...
};

// Same as tilde type for now...
struct Interp_Type
{
    uint8 type;
    uint8 width;
    uint16 data;
};

struct Interp_Symbol
{
    uint8 bitfield;
    Ast_Node* node;  // Points to its definition
    Dg_Entity* entity;
    Interp_Proc* proc;
    // What about global variables?
};

struct Interp_Instr
{
    Interp_OpCode op;
    uint8 bitfield;
    
    union
    {
        struct
        {
            RegIdx dst, src1, src2;
        } bin;
        struct
        {
            Interp_Type type;
            RegIdx dst;
            int64 val;
        } imm;
        struct
        {
            RegIdx dst;
            uint32 size, align;
        } local;
        struct
        {
            Interp_Type type;
            RegIdx dst;
            RegIdx addr;
            uint64 align;
        } load;
        struct
        {
            RegIdx addr;
            RegIdx val;
            uint64 align;
        } store;
        struct
        {
            RegIdx target;
            uint16 argArrayCount;
            uint32 argArrayStart;
            uint16 minRetReg;
            uint16 maxRetReg;
        } call;
        struct
        {
            uint32 keyArrayStart;
            uint32 caseArrayStart;
            uint32 caseArrayCount;
            uint16 keyArrayCount;  // If-else only has one
            
            InstrIdx defaultCase;  // Goto only has this.
        } branch;
        struct
        {
            RegIdx dst;
            Interp_Symbol* symbol;
        } symAddress;
        struct
        {
            uint32 valArrayStart;
            uint16 valArrayCount;
        } ret;
    };
};

#ifndef for_interparray
#define for_interparray(index, start, count) for(int i = start; i < start+count; ++i)
#endif

// @performance Instead of using dynamic arrays a new linear allocator
// that doesn't use virtual memory could be implemented...
struct Interp_Proc
{
    // These are per-proc for parallelism
    // and also so that indices can be smaller
    //Arena instrArena;
    DynArray<InstrIdx> instrArrays;
    //Arena regArena;
    DynArray<RegIdx> regArrays;
    
    // Tells us which register to currently use.
    // Must be reset after the end of an expression
    int64 regCounter = 0;
    
    DynArray<Interp_Instr> locals;
    DynArray<Interp_Instr> instrs;
};

// Registers are only used for temporaries,
// for anything else stack operations are used
struct Interp_Register
{
    union
    {
        int8 int8Value;
        int16 int16Value;
        int32 int32Value;
        int64 int64Value;
        int64 value;
    };
    
    Interp_Type type;
};

struct Interp
{
    Typer* typer;
    DepGraph* graph;
    
    // @remove
    DynArray<Ast_DeclaratorStruct*> structs;
    DynArray<Ast_DeclaratorProc*> procHeaders;
    
    DynArray<Interp_Symbol> symbols;
    DynArray<Interp_Proc> procs;
    
    Array<Interp_Instr> globalInstrs;
};

struct VirtualMachine
{
    Arena stackArena;
    
    DynArray<Interp_Register> registers;
    
    uchar* retStack;
    size_t stackFrameAddress;
    size_t programCounter;
};

// Bytecode instruction generation
InstrIdx Interp_Region(Interp_Proc* proc);
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
RegIdx Interp_Local(Interp_Proc* proc, uint64 size, uint64 align);
RegIdx Interp_Load(Interp_Proc* proc, Interp_Type type, RegIdx addr, uint64 align, bool isVolatile);
void Interp_Store(Interp_Proc* proc, Interp_Type type, RegIdx addr, RegIdx val, uint64 align, bool isVolatile);
void Interp_ImmBool(Interp_Proc* proc);
RegIdx Interp_ImmSInt(Interp_Proc* proc, Interp_Type type, int64 imm);
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
RegIdx Interp_GetSymbolAddress(Interp_Proc* proc, Interp_Symbol* symbol);
void Interp_Select(Interp_Proc* proc);
RegIdx Interp_Add(Interp_Proc* proc, RegIdx reg1, RegIdx reg2);
RegIdx Interp_Sub(Interp_Proc* proc, RegIdx reg1, RegIdx reg2);
RegIdx Interp_Mul(Interp_Proc* proc, RegIdx reg1, RegIdx reg2);
RegIdx Interp_Div(Interp_Proc* proc, RegIdx reg1, RegIdx reg2);
RegIdx Interp_Mod(Interp_Proc* proc, RegIdx reg1, RegIdx reg2);
void Interp_BSwap(Interp_Proc* proc, RegIdx reg1, RegIdx reg2);
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
Interp_RegInterval Interp_Call(Interp_Proc* proc, uint32 numRets, RegIdx target, Array<RegIdx> args);
void Interp_Safepoint(Interp_Proc* proc);
Interp_Instr* Interp_Goto(Interp_Proc* proc, InstrIdx target);
Interp_Instr* Interp_Goto(Interp_Proc* proc);
Interp_Instr* Interp_If(Interp_Proc* proc, RegIdx value, InstrIdx ifInstr, InstrIdx elseInstr);
Interp_Instr* Interp_If(Interp_Proc* proc, RegIdx value);
void Interp_Branch(Interp_Proc* proc);
void Interp_Return(Interp_Proc* proc, RegIdx* retValues, uint16 count);

// AST -> bytecode conversion
void Interp_EndOfExpression(Interp_Proc* proc);  // At this point registers can be reused
RegIdx Interp_ConvertNode(Interp_Proc* proc, Ast_Node* node);
Interp_Proc* Interp_ConvertProc(Interp* interp, Ast_ProcDef* astProc);
void Interp_ConvertBlock(Interp_Proc* proc, Ast_Block* block);
Interp_Type Interp_ConvertType(TypeInfo* type);
// ...

// Utilities
void Interp_PrintProc(Interp_Proc* proc);
void Interp_PrintInstr(Interp_Proc* proc, Interp_Instr* instr);

// Code execution
void Interp_ExecProc(Interp_Proc* proc);