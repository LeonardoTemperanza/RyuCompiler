
#pragma once

#include "base.h"
#include "memory_management.h"
#include "tb.h"

struct TB_Symbol;
struct TB_Function;

struct Interp_Proc;

typedef double Value;

// Operation enum value, its string representation, and whether it uses dst or not 
#define Interp_OpInfo \
X(Op_Null, "Null", false) \
\
/* Immediates */ \
X(Op_IntegerConst, "IntConst", true) \
X(Op_Float32Const, "Float32Const", true) \
X(Op_Float64Const, "Float64Const", true) \
\
/* Start of basic block */\
X(Op_Region, "Region", false) \
\
X(Op_Call, "Call", true) \
X(Op_SysCall, "Syscall", true) \
\
/* Memory ops */\
X(Op_Store, "Store", false) \
X(Op_MemCpy, "Memcpy", false) \
X(Op_MemSet, "Memset", false) \
\
/* Atomics */\
X(Op_AtomicTestAndSet, "AtomicTestAndSet", true) \
X(Op_AtomicClear, "AtomicClear", true) \
\
X(Op_AtomicLoad, "AtomicLoad", true) \
X(Op_AtomicExchange, "AtomicExchange", true) \
X(Op_AtomicAdd, "AtomicAdd", true) \
X(Op_AtomicSub, "AtomicSub", true) \
X(Op_AtomicAnd, "AtomicAnd", true) \
X(Op_AtomicXor, "AtomicXor", true) \
X(Op_AtomicOr, "AtomicOr", true) \
\
X(Op_AtomicCompareExchange, "AtomicCompareExchange", true) \
X(Op_DebugBreak, "DebugBreak", false) \
\
/* Basic block terminators */ \
\
/* Works like a switch (so, any number of cases), */ \
/* Can also be used as a goto (no condition and one case) */\
X(Op_Branch, "Branch", false) \
X(Op_Ret, "Ret", false) \
\
X(Op_Load, "Load", true) \
\
/* Pointers */\
X(Op_Local, "Local", true) \
X(Op_GetSymbolAddress, "GetSymAddress", true) \
X(Op_MemberAccess, "MemberAccess", true) \
X(Op_ArrayAccess, "ArrayAccess", true) \
X(Op_Truncate, "Truncate", true) \
\
/* Conversions */ \
X(Op_FloatExt, "FloatExt", true) \
X(Op_SignExt, "SignExt", true) \
X(Op_ZeroExt, "ZeroExt", true) \
X(Op_Int2Ptr, "Int2Ptr", true) \
X(Op_Uint2Float, "Uint2Float", true) \
X(Op_Float2Uint, "Float2Uint", true) \
X(Op_Int2Float, "Int2Float", true) \
X(Op_Float2Int, "Float2Int", true) \
X(Op_Bitcast, "Bitcast", true) \
\
X(Op_Select, "Select", true) \
\
/* Unary operators */ \
X(Op_Not, "Not", true) \
X(Op_Negate, "Negate", true) \
\
/* Integer arithmetic */ \
X(Op_And, "And", true) \
X(Op_Or, "Or", true) \
X(Op_Xor, "Xor", true) \
X(Op_Add, "Add", true) \
X(Op_Sub, "Sub", true) \
X(Op_Mul, "Mul", true) \
\
X(Op_ShL, "ShL", true) \
X(Op_ShR, "ShR", true) \
X(Op_Sar, "Sar", true) \
X(Op_Rol, "Rol", true) \
X(Op_Ror, "Ror", true) \
X(Op_UDiv, "UDiv", true) \
X(Op_SDiv, "SDiv", true) \
X(Op_UMod, "UMod", true) \
X(Op_SMod, "SMod", true) \
\
/* Float arithmetic */ \
X(Op_FAdd, "FAdd", true) \
X(Op_FSub, "FSub", true) \
X(Op_FMul, "FMul", true) \
X(Op_FDiv, "FDiv", true) \
\
/* Comparisons */ \
X(Op_CmpEq, "CmpEq", true) \
X(Op_CmpNe, "CmpNe", true) \
X(Op_CmpULT, "CmpULT", true) \
X(Op_CmpULE, "CmpULE", true) \
X(Op_CmpSLT, "CmpSLT", true) \
X(Op_CmpSLE, "CmpSLE", true) \
X(Op_CmpFLT, "CmpFLT", true) \
X(Op_CmpFLE, "CmpFLE", true)

#define X(enumVal, string, usesDst) enumVal,
enum Interp_OpCodeEnum
{
    Interp_OpInfo
};
#undef X

#define X(enumVal, string, usesDst) string,
const char* Interp_OpStrings[] =
{
    Interp_OpInfo
};
#undef X

#define X(enumVal, string, usesDst) usesDst,
const bool Interp_OpUsesDst[] =
{
    Interp_OpInfo
};
#undef X

#if 0
enum Interp_OpCodeEnum
{
    Op_Null = 0,
    
    // Immediates
    Op_IntegerConst,
    Op_Float32Const,
    Op_Float64Const,
    
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
#endif

typedef uint8 Interp_OpCode;

// A 16-bit register means that you can use 65536 temporary registers,
// which means that an expression can only be so long. That seams pretty
// reasonable to me, as it means that a sum can only be 65536 operands long,
// for example. If I don't want to impose a limit, I guess in the extreme
// case a form of register spilling could be implemented?
typedef uint16 RegIdx;
typedef uint32 InstrIdx;  // Max of 4 billion instructions per proc
typedef uint32 ProcIdx;   // Max of 4 billion procedures

#define RegIdx_Unused USHRT_MAX

struct Interp_RegInterval
{
    RegIdx minReg;
    RegIdx maxReg;
};

enum Interp_InstrBitfieldEnum
{
    InstrBF_RetVoid   = 1 << 0,
    InstrBF_SetUpArgs = 1 << 1
};

typedef uint8 InstrBitfield;

enum InterpTypeKindEnum
{
    InterpType_Int,
    InterpType_Float,
    InterpType_Ptr
};
typedef uint8 InterpTypeKind;

enum FloatTypeEnum
{
    FType_Flt32,
    FType_Flt64
};
typedef uint8 FloatType;


// Same as tilde type for now...
struct Interp_Type
{
    uint8 type;
    uint8 width;
    uint16 data;
};

#define Interp_Void     Interp_Type{ InterpType_Int,   0, 0 }
#define Interp_Int8     Interp_Type{ InterpType_Int,   0, 8 }
#define Interp_Int16    Interp_Type{ InterpType_Int,   0, 16 }
#define Interp_Int32    Interp_Type{ InterpType_Int,   0, 32 }
#define Interp_Int64    Interp_Type{ InterpType_Int,   0, 64 }
#define Interp_F32      Interp_Type{ InterpType_Float, 0, FType_Flt32 }
#define Interp_F64      Interp_Type{ InterpType_Float, 0, FType_Flt64 }
#define Interp_Bool     Interp_Type{ InterpType_Int,   0, 1 }
#define Interp_Ptr      Interp_Type{ InterpType_Ptr,   0, 0 }
#define Interp_IntN(N)  Interp_Type{ InterpType_Int,   0, (N) }
#define Interp_PtrN(N)  Interp_Type{ InterpType_Ptr,   0, (N) }

struct Interp_Symbol
{
    // Whatever hot data will be used by the interpreter
    // will be put here.
    
    uint8 bitfield;
    
    Ast_Declaration* decl;  // Points to its declaration/definition
    char* name;             // Null terminated
    TypeInfo* type;
    
    // Codegen info, should probably just be in a PtrMap or something
    TB_Symbol* tildeSymbol;
    TB_Function* tildeProc;
    // LLVMValueRef
};

struct Interp_Instr
{
    Interp_OpCode op = 0;
    InstrBitfield bitfield = 0;
    RegIdx dst = 0;  // Handy to have it here, should be set to 0 if the instruction doesn't use it
    
    union
    {
        struct
        {
            RegIdx src;
        } unary;
        struct
        {
            RegIdx src1, src2;
        } bin;
        struct
        {
            Interp_Type type;
            int64 val;
        } imm;  // @performance immediates could just be in the instructions themselves
        struct
        {
            uint32 size, align;
        } local;
        struct
        {
            RegIdx addr;
            Interp_Type type;
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
            uint16 argCount;
            uint32 argStart;
            //uint16 minRetReg;
            //uint16 maxRetReg;  // Exclusive, it's the same as minRetReg if no returns
        } call;
        struct
        {
            RegIdx dst, src, count;
            uint64 align;
        } memcpy;
        struct
        {
            // Constant value array
            uint32 keyStart;
            // Instruction index array
            uint32 caseStart;
            uint16 count;  // If-else only has one
            
            RegIdx value;  // Used to evaluate case, goto doesn't use this
            
            InstrIdx defaultCase;  // Goto only uses this.
        } branch;
        struct
        {
            Interp_Symbol* symbol;
        } symAddress;
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
    DynArray<InstrIdx> instrArrays;
    DynArray<RegIdx> regArrays;
    DynArray<int64> constArrays;
    
    // This will be used even if the tilde codegen is not used.
    // It's for generating ABI-compliant bytecode, which can then
    // be seamlessly (hopefully) converted to any other IR for codegen.
    // This is for bytecode gen, and actually matches the AST arguments
    DynArray<TB_PassingRule> argRules;
    TB_PassingRule retRule;
    
    // Maps declaration indices to registers that contain their
    // addresses.
    DynArray<RegIdx> declToAddr;
    
    // Registers used for local addresses, argument values
    // and whatnot. These are ordered, by construction.
    DynArray<RegIdx> permanentRegs;
    
    // Tells us which register to currently use.
    // Must be reset after the end of an expression
    RegIdx regCounter = 0;
    
    DynArray<Interp_Instr> instrs;
    
    // NOTE: Used for getting the passing rules for arguments and return values
    TB_Module* module;
    
    // Additional information for codegen
    Interp_Symbol* symbol;
    
    // Function description used for codegen
    DynArray<Interp_Type> argTypes;
    Interp_Type retType;
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
    
    // NOTE: This is here just because
    // the tb functions that are used to get
    // the ABI-compliant signatures require a module
    TB_Module* module;
    
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
struct Interp_Val
{
    RegIdx reg;
    bool isLValue;
};

// Returns the old register and advances the current one
// Works kind of like post-order ++ operator
void Interp_AdvanceReg(Interp_Proc* proc);
RegIdx Interp_GetFirstUnused(Interp_Proc* proc);
Interp_Proc* Interp_MakeProc(Interp* interp);
Interp_Symbol* Interp_MakeSymbol(Interp* interp);
uint32 Interp_AllocateRegIdxArray(Interp_Proc* proc, RegIdx* values, uint16 arrayCount);
uint32 Interp_AllocateRegIdxArray(Interp_Proc* proc, Array<RegIdx> values);
uint32 Interp_AllocateInstrIdxArray(Interp_Proc* proc, InstrIdx* values, uint32 arrayCount);
uint32 Interp_AllocateInstrIdxArray(Interp_Proc* proc, Array<InstrIdx> instrs);
uint32 Interp_AllocateConstArray(Interp_Proc* proc, int64* values, uint32 count);
uint32 Interp_AllocateConstArray(Interp_Proc* proc, Array<int64> consts);
RegIdx Interp_BuildPreRet(Interp_Proc* proc, RegIdx arg, Interp_Val val, TypeInfo* type, TB_PassingRule rule);
RegIdx Interp_PassArg(Interp_Proc* proc, Interp_Val arg, TypeInfo* type, TB_PassingRule rule, Arena* allocTo);
Interp_Val Interp_GetRet(Interp_Proc* proc, TB_PassingRule rule, RegIdx retReg, TypeInfo* retType);
RegIdx Interp_GetRVal(Interp_Proc* proc, Interp_Val val, TypeInfo* type);
Interp Interp_Init();
InstrIdx Interp_Region(Interp_Proc* proc);
void Interp_DebugBreak(Interp_Proc* proc);
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
RegIdx Interp_Local(Interp_Proc* proc, uint64 size, uint64 align, bool setUpArg = false);
RegIdx Interp_Load(Interp_Proc* proc, Interp_Type type, RegIdx addr, uint64 align, bool isVolatile);
Interp_Instr* Interp_Store(Interp_Proc* proc, Interp_Type type, RegIdx addr, RegIdx val, uint64 align, bool isVolatile, bool setUpArg = false);
void Interp_ImmBool(Interp_Proc* proc);
RegIdx Interp_ImmSInt(Interp_Proc* proc, Interp_Type type, int64 imm);
RegIdx Interp_ImmUInt(Interp_Proc* proc, Interp_Type type, uint64 imm);
void Interp_Float32(Interp_Proc* proc);
void Interp_Float64(Interp_Proc* proc);
// void Interp_CString(Interp_Proc* proc);
// void Interp_String(Interp_Proc* proc);
void Interp_MemSet(Interp_Proc* proc);
void Interp_MemZero(Interp_Proc* proc);
void Interp_MemCpy(Interp_Proc* proc, RegIdx dst, RegIdx src, RegIdx count, uint64 align, bool isVolatile);
void Interp_ArrayAccess(Interp_Proc* proc);
void Interp_MemberAccess(Interp_Proc* proc);
RegIdx Interp_GetSymbolAddress(Interp_Proc* proc, Interp_Symbol* symbol);
void Interp_Select(Interp_Proc* proc);
RegIdx Interp_Add(Interp_Proc* proc, RegIdx reg1, RegIdx reg2);
RegIdx Interp_Sub(Interp_Proc* proc, RegIdx reg1, RegIdx reg2);
RegIdx Interp_Mul(Interp_Proc* proc, RegIdx reg1, RegIdx reg2);
RegIdx Interp_Div(Interp_Proc* proc, RegIdx reg1, RegIdx reg2, bool signedness);
RegIdx Interp_Mod(Interp_Proc* proc, RegIdx reg1, RegIdx reg2, bool signedness);
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
RegIdx Interp_Call(Interp_Proc* proc, RegIdx target, Array<RegIdx> args);
void Interp_Safepoint(Interp_Proc* proc);
Interp_Instr* Interp_Goto(Interp_Proc* proc, InstrIdx target);
Interp_Instr* Interp_Goto(Interp_Proc* proc);
Interp_Instr* Interp_If(Interp_Proc* proc, RegIdx value, InstrIdx ifInstr, InstrIdx elseInstr);
Interp_Instr* Interp_If(Interp_Proc* proc, RegIdx value);
Interp_Instr* Interp_Branch(Interp_Proc* proc);
void Interp_Return(Interp_Proc* proc, RegIdx retValue);

// AST -> bytecode conversion
void Interp_EndOfExpression(Interp_Proc* proc);  // At this point registers can be reused
RegIdx Interp_ConvertNodeRVal(Interp_Proc* proc, Ast_Node* node);
Interp_Val Interp_ConvertNode(Interp_Proc* proc, Ast_Node* node);
Array<Interp_Val> Interp_ConvertCall(Interp_Proc* proc, Ast_FuncCall* call, Arena* allocTo);
Interp_Proc* Interp_ConvertProc(Interp* interp, Ast_ProcDef* astProc);
void Interp_ConvertBlock(Interp_Proc* proc, Ast_Block* block);
Interp_Type Interp_ConvertType(TypeInfo* type);
// ...

// Utilities
void Interp_PrintProc(Interp_Proc* proc);
void Interp_PrintInstr(Interp_Proc* proc, Interp_Instr* instr);
void Interp_PrintBin(Interp_Instr* instr);
void Interp_PrintUnary(Interp_Instr* instr);

// Code execution
void Interp_ExecProc(Interp_Proc* proc);