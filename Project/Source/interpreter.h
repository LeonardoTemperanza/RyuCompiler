
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

struct Interp_Type
{
    union
    {
        struct
        {
            uint8 type;
            uint8 width;
            uint16 data;
        };
        uint32 raw;
    };
};

typedef uint16 RegIdx;
typedef uint8 TypeIdx;

struct Interp_Instr
{
    Interp_OpCode op;
    TypeIdx typeIdx;  // To save space only the type is stored here
    
    // How much stuff can be crammed here?
    uint8 bitfield;
    
    uint8 type;  // Is this not just included in the instruction?
    uint8 width;
    uint16 data;  // Bitwidth? Does that need to be
};

struct Interp
{
    Arena arena;
    uchar* instrData;
};

struct VirtualMachine
{
    Arena stackArena;
    
    uchar* retStack;
    size_t stackFrameAddress;
    size_t programCounter;
    
    Interp_Type types[256];
};

void Interp_PrintInstr();
