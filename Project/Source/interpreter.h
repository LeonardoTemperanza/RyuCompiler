
#pragma once

#include "base.h"
#include "memory_management.h"

typedef double Value;

enum Interp_OpCode : uint8
{
    Op_Constant,
    
    Op_Add,
    Op_Sub,
    Op_Mul,
    Op_Div,
    Op_Mod,
    Op_EndArithmetic,
    
    Op_CmpLT,
    Op_CmpLE,
    Op_CmpGT,
    Op_CmpGE,
    Op_CmpEq,
    Op_CmpNEq,
    
    Op_Alloca,
    Op_Load,
    Op_Store,
    
    Op_Jump,
    Op_CondJmp,
    Op_Ret
};

#define Interp_GetTypeSize(bitfield) ((bitfield) >> 6)

// @speed Could be possible to store the 64-bit operands as 2 32-bit values, to reduce
// the size of the struct and increase the cache efficiency (if we only need an 8-bit bitfield)
struct Interp_Instr
{
    Interp_OpCode op;
    
    // Here you can specify the size of the values of the primitive types used,
    // signed or unsigned, whether there is a ret afterwards, or a label before,
    // or whether this instruction is used for a function call or not, etc.
    uint8 bitfield;
};

struct Interp
{
    Arena arena;
    uchar* instrData;
};

struct VirtualMachine
{
    Arena stack;
    //GenValue registers[4000];
    uchar* retStack;
    size_t stackFrameAddress;
    size_t programCounter;
};

void Interp_PrintInstr();
