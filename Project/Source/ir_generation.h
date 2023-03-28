
#pragma once

#include "base.h"
#include "memory_management.h"

// NOTE(Leo): I'm putting some of my thoughts here, remove later

// This is what I'll need to implement here:
// Stack variable handling
// Function definitions (NOTE this is not really a thing, you can just structure the code with labels for now...?)
// Function calls (even to native code)
// Elementary operations of various types (arithmetic, logic), on 8,16,32,64 bit values
// Control flow constructs (jump)
// Structs are implemented at the frontend, during generation. It's just pointer arithmetic (at least I think)
// Optional:
// Optimization stuff. Maybe if you try to implement some basic optimizations, then it's going to be easier to
// think about this kind of stuff. My opinion at the moment is that most things can be stored in separate data structures,
// but we'll see about that

// All functions that refer to the stack need an index for that. it's pretty much a relative pointer 

// For function calls, how do i specify which registers to use?
// I have to use stack variables to pass stuff to functions. I use a store,
// then the backend can just use registers when appropriate. (for optimization purposes)

// I think stack variables can just be identified with what their address would be at runtime (relative to the procedure)
// That way it remains fast when interpreting (you sum to get the stack address), but you still have a way to identify the
// stack variables when generating assembly.

// ^^ This is the approach generally used. It's basically an offset from the current stack frame address. ^^
// Also, tipically a "stack frame pointer register" is used.

// op Load reg0 destination reg1 none value addr
// the store instruction is literally the only one that i can think of
// that needs 2 8-byte values encoded in the instruction. I mean, i can just make it so you need to use a register
// for that. What about function calls?

// What do you have to do for a function call?
// I think the backend in general does not care about the operations. Except i would have to use a dynamic array so no thanks.
// The operations can just be tagged as "for function call"
// anyway, as we were saying... you need to 1) perform store operations in special stack variables.
// 2) jump to instruction pointer, right? And update the stack frame pointer i guess. And that's it!

// I think for a function the first thing that you want to do is allocate the arguments and return

enum IR_InstrOpCode : uint8
{
    IR_Add,
    IR_Sub,
    IR_Mul,
    IR_Div,
    IR_Mod,
    IR_EndArithmetic,
    
    IR_CmpLT,
    IR_CmpLE,
    IR_CmpGT,
    IR_CmpGE,
    IR_CmpEq,
    IR_CmpNEq,
    
    IR_Alloca,
    IR_Load,
    IR_Store,
    
    IR_Jump,
    IR_CondJmp,
    IR_Ret
};

// Generic 64 bit value that can contain 8/16/32/64-bit value
// and could also be a pointer to a structure in memory.
union GenValue
{
    uint8 uint8val;
    uint16 uint16val;
    uint32 uint32val;
    uint64 uint64val;
    int8 int8val;
    int16 int16val;
    int32 int32val;
    int64 int64val;
    float floatval;
    double doubleval;
    void* ptr;
    
    // NOTE: This makes it so I can painlessly cast to any type.
    // example: GenValue value = { .doubleval = 3.0 };  double copy = value;
    // I don't think there is a better way to do this, unfortunately.
    inline operator uint8()  const { return uint8val;  }
    inline operator uint16() const { return uint16val; }
    inline operator uint32() const { return uint32val; }
    inline operator uint64() const { return uint64val; }
    inline operator int8()   const { return int8val;   }
    inline operator int16()  const { return int16val;  }
    inline operator int32()  const { return int32val;  }
    inline operator int64()  const { return int64val;  }
    inline operator float()  const { return floatval;  }
    inline operator double() const { return doubleval; }
    inline operator void*()  const { return ptr;       }
};

enum IR_Flags : uint8
{
    IR_AllocRet  = 1 << 0,  // Used for allocating on the special return stack var
    IR_AllocCall = 1 << 1,  // Used for allocating on the special arg stack vars
    IR_Label     = 1 << 2,  // I don't know if this needs to be used tbh
    IR_IsSigned  = 1 << 3,  // Signed/unsigned integer types
    IR_IsFloat   = 1 << 4,  // integer/floating point integer types
    IR_IsImmed   = 1 << 5   // Is immediate
};

enum IR_TypeSize : uint8
{
    IR_8b  = 0,
    IR_16b = 1,
    IR_32b = 2,
    IR_64b = 3
};

#define IR_GetTypeSize(bitfield) ((bitfield) >> 6)

// NOTE: How to use instructions:
// For arithmetic operations:
// - dest usually contains the destination registers
// - operand1 usually contains the first source register
// - operand2 usually contains the second source register, or an immediate value
// 
// For load/store instructions:
// Store:   - operand1 contains the stack variable pointer (stack frame offset),
//          - operand2 contains the register or an immediate value
//          - dest is not used
// Load:    - operand1 contains the stack variable pointer (stack frame offset),
//          - operand2 is not used
//          - dest contains the destination register
//
// For stack instructions:
// Alloca:  - operand1, operand2 and dest are not used
//          - the size of the stack variable is contained in the flags.
//
// For jump instructions:
// Jump:    - operand1 contains the address to jump to
//          - operand2 and dest is not used
// CondJmp: - operand1 contains the address to jump to
//          - operand2 contains the register from use for the conditional jump
//          - dest is not used.
// Ret:     - Everything is not used (is this a good idea?)

// @speed Could be possible to store the 64-bit operands as 2 32-bit values, to reduce
// the size of the struct and increase the cache efficiency (if we only need an 8-bit bitfield)
struct IR_Instruction
{
    GenValue operand1;
    GenValue operand2;
    uint16 dest;
    
    IR_InstrOpCode op;
    
    // Here you can specify the size of the values of the primitive types used,
    // signed or unsigned, whether there is a ret afterwards, or a label before,
    // or whether this instruction is used for a function call or not, etc.
    uint8 bitfield;
    
    // 4-byte padding
};

struct IR_Context
{
    // There needs to be type table information here,
    // and some other stuff
    
    Arena instructions;
    // This is for #run directives
    Arena interpOnlyInstructions;
};
