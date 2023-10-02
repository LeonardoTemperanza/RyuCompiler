
#include "base.h"
#include "parser.h"
#include "semantics.h"
#include "dependency_graph.h"

#include "bytecode_builder.h"

// TODO: Include very simple optimizations that reduce the bytecode size

Builder_MiddleInsert Interp_StartMiddleInsertion(Interp_Builder* builder, Arena* tempArena, int insertPoint)
{
    Assert(insertPoint < builder->instrOffsets.length && insertPoint >= 0);
    
    auto ctx = builder->middleInsert;
    builder->middleInsert.arena = tempArena;
    builder->middleInsert.instrs = { 0, 0 };
    builder->middleInsert.insertPoint = insertPoint;
    builder->middleInsert.insertPointOffset = builder->instrOffsets[insertPoint];
    return ctx;
}

void Interp_CommitMiddleInsertion(Interp_Builder* builder, Builder_MiddleInsert ctx)
{
    auto& middleInsert = builder->middleInsert;
    auto proc = builder->proc;
    Assert(middleInsert.arena && "Usage error: attempting to end a middle insertion before starting one");
    
    // Shift the insertion point in case there have been nested middle insertions
    int insertPoint = Interp_UpdateInstrIdx(builder, middleInsert.insertPoint, middleInsert.insertPointOffset);
    
    proc->instrs.InsertAtIdx(middleInsert.instrs, insertPoint);
    builder->instrOffsets.Resize(proc->instrs.length);
    builder->instrOffsets[proc->instrs.length - 1] = 0;
    
    // Revert back to the old middle insertion
    builder->middleInsert = ctx;
}

Interp_Instr* Interp_InsertInstr(Interp_Builder* builder)
{
    auto proc = builder->proc;
    Interp_Instr* res = 0;
    // If there is an available arena, it means that
    // the user is inserting in the middle of the array
    if(builder->middleInsert.arena)
    {
        Arena* arena = builder->middleInsert.arena;
        Array<Interp_Instr>& instrs = builder->middleInsert.instrs;
        instrs.ResizeAndInit(arena, instrs.length - 1);
        res = &instrs[proc->instrs.length - 1];
    }
    else
    {
        proc->instrs.ResizeAndInit(proc->instrs.length + 1);
        
        builder->instrOffsets.Resize(proc->instrs.length);
        builder->instrOffsets[proc->instrs.length-1] = 0;
        res = &proc->instrs[proc->instrs.length-1];
    }
    
    return res;
}

void Interp_AdvanceReg(Interp_Builder* builder)
{
    // Find the next register in the permanentRegs array
    ++builder->regCounter;
    int foundIdx = -1;
    for_array(i, builder->permanentRegs)
    {
        if(builder->permanentRegs[i] == builder->regCounter)
        {
            foundIdx = i;
            break;
        }
    }
    
    if(foundIdx == -1)
        return;
    
    ++builder->regCounter;
    
    for(int i = foundIdx + 1; i < builder->permanentRegs.length; ++i)
    {
        if(builder->permanentRegs[i] != builder->regCounter)
            break;
        
        ++builder->regCounter;
    }
}

RegIdx Interp_GetFirstUnused(Interp_Builder* builder)
{
    if(builder->permanentRegs.length == 0) return 0;
    
    // Get min
    RegIdx minPermanent = builder->permanentRegs[0];
    if(minPermanent > 0) return 0;
    
    RegIdx res = 0;
    for(int i = 0; i < builder->permanentRegs.length; ++i)
    {
        if(builder->permanentRegs[i] != res)
            break;
        
        ++res;
    }
    
    return res;
}

uint32 Interp_AllocateRegIdxArray(Interp_Builder* builder, RegIdx* values, uint16 arrayCount)
{
    auto proc = builder->proc;
    uint32 oldLength = (uint32)proc->regArrays.length;
    proc->regArrays.ResizeAndInit(proc->regArrays.length + arrayCount);
    
    for(int i = oldLength; i < proc->regArrays.length; ++i)
    {
        int j = i - oldLength;
        proc->regArrays[i] = values[j];
    }
    
    return oldLength;
}

uint32 Interp_AllocateRegIdxArray(Interp_Builder* builder, Array<RegIdx> values)
{
    return Interp_AllocateRegIdxArray(builder, values.ptr, values.length);
}

uint32 Interp_AllocateInstrIdxArray(Interp_Builder* builder, InstrIdx* values, uint32 arrayCount)
{
    auto proc = builder->proc;
    uint32 oldLength = (uint32)proc->instrArrays.length;
    proc->instrArrays.ResizeAndInit(proc->instrArrays.length + arrayCount);
    
    for(int i = oldLength; i < proc->instrArrays.length; ++i)
    {
        int j = i - oldLength;
        proc->instrArrays[i] = values[j];
    }
    
    return oldLength;
}

uint32 Interp_AllocateInstrIdxArray(Interp_Builder* builder, Array<InstrIdx> instrs)
{
    return Interp_AllocateInstrIdxArray(builder, instrs.ptr, instrs.length);
}

uint32 Interp_AllocateConstArray(Interp_Builder* builder, int64* values, uint32 count)
{
    auto proc = builder->proc;
    uint32 oldLength = (uint32)proc->constArrays.length;
    proc->constArrays.ResizeAndInit(proc->constArrays.length + count);
    
    for(int i = oldLength; i < proc->constArrays.length; ++i)
    {
        int j = i - oldLength;
        proc->constArrays[i] = values[j];
    }
    
    return oldLength;
}

uint32 Interp_AllocateConstArray(Interp_Builder* builder, Array<int64> consts)
{
    return Interp_AllocateConstArray(builder, consts.ptr, consts.length);
}

void Interp_InsertInstrAtMiddle(Interp_Builder* builder, Interp_Instr instr, InstrIdx idx)
{
    auto proc = builder->proc;
    
    proc->instrs.InsertAtIdx(instr, idx);
    builder->instrOffsets.Resize(proc->instrs.length);
    builder->instrOffsets[proc->instrs.length - 1] = 0;
    
    // Update all jumps
    for_array(i, proc->instrArrays)
    {
        if(proc->instrArrays[i] >= idx)
            ++proc->instrArrays[i];
    }
    
    for_array(i, proc->instrs)
    {
        auto& instr = proc->instrs[i];
        if(instr.op == Op_Branch && instr.branch.defaultCase >= idx)
            ++instr.branch.defaultCase;
    }
    
    for(int i = idx; i < proc->instrs.length - 1; ++i)
    {
        ++builder->instrOffsets[i];
    }
}

InstrIdx Interp_UpdateInstrIdx(Interp_Builder* builder, InstrIdx idx, int oldOffset)
{
    return idx + builder->instrOffsets[idx] - oldOffset;
}

InstrIdx Interp_Placeholder(Interp_Builder* builder)
{
    auto proc = builder->proc;
    auto newElement = Interp_InsertInstr(builder);
    
    return proc->instrs.length - 1;
}

InstrIdx Interp_Region(Interp_Builder* builder)
{
    auto proc = builder->proc;
    auto newElement = Interp_InsertInstr(builder);
    
    newElement->op = Op_Region;
    newElement->dst = builder->regCounter;
    return proc->instrs.length - 1;
}

void Interp_DebugBreak(Interp_Builder* builder)
{
    auto newElement = Interp_InsertInstr(builder);
    newElement->op  = Op_DebugBreak;
    newElement->dst = 0;
}

RegIdx Interp_Bin(Interp_Builder* builder, Interp_OpCode op, RegIdx reg1, RegIdx reg2)
{
    auto newElement = Interp_InsertInstr(builder);
    newElement->op       = op;
    newElement->dst      = builder->regCounter;
    newElement->bin.src1 = reg1;
    newElement->bin.src2 = reg2;
    Interp_AdvanceReg(builder);
    return newElement->dst;
}

RegIdx Interp_Unary(Interp_Builder* builder, Interp_OpCode op, RegIdx src, Interp_Type type)
{
    auto newElement = Interp_InsertInstr(builder);
    newElement->op   = op;
    newElement->dst  = builder->regCounter;
    newElement->unary.type = type;
    newElement->unary.src  = src;
    
    builder->permanentRegs.Append(builder->regCounter);
    Interp_AdvanceReg(builder);
    return newElement->dst;
}

RegIdx Interp_Unary(Interp_Builder* builder, Interp_OpCode op, RegIdx src)
{
    auto newElement = Interp_InsertInstr(builder);
    newElement->op   = op;
    newElement->dst  = builder->regCounter;
    newElement->unary.src  = src;
    
    builder->permanentRegs.Append(builder->regCounter);
    Interp_AdvanceReg(builder);
    return newElement->dst;
}

// void Interp_Param(Interp_Proc* proc) {}

RegIdx Interp_FPExt(Interp_Builder* builder, RegIdx src, Interp_Type type)
{
    return Interp_Unary(builder, Op_FloatExt, src, type);
}

RegIdx Interp_SExt(Interp_Builder* builder, RegIdx src, Interp_Type type)
{
    return Interp_Unary(builder, Op_SignExt, src, type);
}

RegIdx Interp_ZExt(Interp_Builder* builder, RegIdx src, Interp_Type type)
{
    return Interp_Unary(builder, Op_ZeroExt, src, type);
}

RegIdx Interp_Trunc(Interp_Builder* builder, RegIdx src, Interp_Type type)
{
    return Interp_Unary(builder, Op_Truncate, src, type);
}

RegIdx Interp_Int2Ptr(Interp_Builder* builder, RegIdx src, Interp_Type type)
{
    return Interp_Unary(builder, Op_Int2Ptr, src, type);
}

RegIdx Interp_Ptr2Int(Interp_Builder* builder, RegIdx src, Interp_Type type)
{
    return Interp_Unary(builder, Op_Ptr2Int, src, type);
}

// TODO: If it's a constant, remove the unnecessary cast and directly convert the constant
RegIdx Interp_Int2Float(Interp_Builder* builder, RegIdx src, Interp_Type type, bool isSigned)
{
    return Interp_Unary(builder, isSigned? Op_Int2Float : Op_Uint2Float, src, type);
}

RegIdx Interp_Float2Int(Interp_Builder* builder, RegIdx src, Interp_Type type, bool isSigned)
{
    return Interp_Unary(builder, isSigned? Op_Float2Int : Op_Float2Uint, src, type);
}

RegIdx Interp_Bitcast(Interp_Builder* builder, RegIdx src, Interp_Type type)
{
    return Interp_Unary(builder, Op_Bitcast, src, type);
}

// Local uses reg1 and reg2 to store size and alignment,
// Dest is the resulting address
RegIdx Interp_Local(Interp_Builder* builder, uint64 size, uint64 align, bool setUpArg)
{
    auto newElement = Interp_InsertInstr(builder);
    newElement->op          = Op_Local;
    newElement->dst         = builder->regCounter;
    newElement->local.size  = size;
    newElement->local.align = align;
    if(setUpArg) newElement->bitfield |= InstrBF_SetUpArgs;
    
    // Addresses for locals are used across different
    // statements, so they need to be added to the
    // permanentRegs array
    builder->permanentRegs.Append(builder->regCounter);
    Interp_AdvanceReg(builder);
    return newElement->dst;
}

// Ignoring atomics for now
RegIdx Interp_Load(Interp_Builder* builder, Interp_Type type, RegIdx addr, uint64 align, bool isVolatile)
{
    auto newElement = Interp_InsertInstr(builder);
    newElement->op         = Op_Load;
    newElement->dst        = builder->regCounter;
    newElement->load.addr  = addr;
    newElement->load.align = align;
    newElement->load.type  = type;
    
    Interp_AdvanceReg(builder);
    return newElement->dst;
}

Interp_Instr* Interp_Store(Interp_Builder* builder, Interp_Type type, RegIdx addr, RegIdx val, uint64 align, bool isVolatile, bool setUpArg)
{
    auto newElement = Interp_InsertInstr(builder);
    newElement->op          = Op_Store;
    newElement->dst         = 0;
    newElement->store.addr  = addr;
    newElement->store.align = align;
    newElement->store.val   = val;
    if(setUpArg) newElement->bitfield |= InstrBF_SetUpArgs;
    
    return newElement;
}

RegIdx Interp_ImmBool(Interp_Builder* builder, bool imm)
{
    auto newElement = Interp_InsertInstr(builder);
    newElement->op         = Op_IntegerConst;
    newElement->dst        = builder->regCounter;
    newElement->imm.intVal = imm;
    newElement->imm.type   = Interp_Bool;
    Interp_AdvanceReg(builder);
    return newElement->dst;
}

RegIdx Interp_ImmSInt(Interp_Builder* builder, Interp_Type type, int64 imm)
{
    auto newElement = Interp_InsertInstr(builder);
    newElement->op         = Op_IntegerConst;
    newElement->dst        = builder->regCounter;
    newElement->imm.intVal = imm;
    newElement->imm.type   = type;
    Interp_AdvanceReg(builder);
    return newElement->dst;
}

RegIdx Interp_ImmUInt(Interp_Builder* builder, Interp_Type type, uint64 imm)
{
    // Zero out all bits which are not supposedly used
    uint64 mask = ~0ULL >> (type.data);
    imm &= mask;
    
    auto newElement = Interp_InsertInstr(builder);
    newElement->op         = Op_IntegerConst;
    newElement->dst        = builder->regCounter;
    newElement->imm.intVal = imm;
    newElement->imm.type   = type;
    Interp_AdvanceReg(builder);
    return newElement->dst;
}

RegIdx Interp_ImmFloat32(Interp_Builder* builder, Interp_Type type, float imm)
{
    auto newElement = Interp_InsertInstr(builder);
    newElement->op           = Op_Float32Const;
    newElement->dst          = builder->regCounter;
    newElement->imm.floatVal = imm;
    newElement->imm.type     = type;
    Interp_AdvanceReg(builder);
    return newElement->dst;
}

RegIdx Interp_ImmFloat64(Interp_Builder* builder, Interp_Type type, double imm)
{
    auto newElement = Interp_InsertInstr(builder);
    newElement->op            = Op_Float64Const;
    newElement->dst           = builder->regCounter;
    newElement->imm.doubleVal = imm;
    newElement->imm.type      = type;
    Interp_AdvanceReg(builder);
    return newElement->dst;
}

// void Interp_CString(Interp_Proc* proc) {}

// void Interp_String(Interp_Proc* proc) {}

void Interp_MemSet(Interp_Builder* builder, RegIdx dst, RegIdx val, RegIdx count, uint64 align)
{
    auto newElement = Interp_InsertInstr(builder);
    newElement->op           = Op_MemCpy;
    newElement->memset.dst   = dst;
    newElement->memset.val   = val;
    newElement->memset.count = count;
    newElement->memset.align = align;
}

void Interp_MemZero(Interp_Builder* builder, RegIdx dst, RegIdx count, uint64 align)
{
    Interp_MemSet(builder, dst, Interp_ImmUInt(builder, Interp_Int8, 0), count, align);
}

void Interp_MemCpy(Interp_Builder* builder, RegIdx dst, RegIdx src, RegIdx count, uint64 align, bool isVolatile)
{
    auto newElement = Interp_InsertInstr(builder);
    newElement->op           = Op_MemCpy;
    newElement->memcpy.dst   = dst;
    newElement->memcpy.src   = src;
    newElement->memcpy.count = count;
    newElement->memcpy.align = align;
}

RegIdx Interp_ArrayAccess(Interp_Builder* builder)
{
    return RegIdx_Unused;
}

RegIdx Interp_MemberAccess(Interp_Builder* builder, RegIdx base, int64 offset)
{
    auto newElement = Interp_InsertInstr(builder);
    newElement->op            = Op_MemberAccess;
    newElement->dst           = builder->regCounter;
    newElement->memacc.base   = base;
    newElement->memacc.offset = offset;
    Interp_AdvanceReg(builder);
    return newElement->dst;
}

RegIdx Interp_GetSymbolAddress(Interp_Builder* builder, Interp_Symbol* symbol)
{
    auto proc = builder->proc;
    auto newElement = Interp_InsertInstr(builder);
    
    newElement->op  = Op_GetSymbolAddress;
    newElement->dst = builder->regCounter;
    newElement->symAddress.symbol = symbol;
    Interp_AdvanceReg(builder);
    return newElement->dst;
}

void Interp_Select(Interp_Builder* builder)
{
    
}

// @incomplete
// This should support different kinds of arithmetic behavior 
RegIdx Interp_Add(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_Add, reg1, reg2);
}

RegIdx Interp_Sub(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_Sub, reg1, reg2);
}

RegIdx Interp_Mul(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_Mul, reg1, reg2);
}

RegIdx Interp_Div(Interp_Builder* builder, RegIdx reg1, RegIdx reg2, bool signedness)
{
    return Interp_Bin(builder, signedness? Op_SDiv : Op_UDiv, reg1, reg2);
}

RegIdx Interp_Mod(Interp_Builder* builder, RegIdx reg1, RegIdx reg2, bool signedness)
{
    return Interp_Bin(builder, signedness? Op_SMod : Op_UMod, reg1, reg2);
}

RegIdx Interp_BSwap(Interp_Builder* builder)
{
    return 0;
}

//void Interp_CLZ(Interp_Builder* builder) {}

//void Interp_CTZ(Interp_Builder* builder) {}

//void Interp_PopCount(Interp_Builder* builder) {}

RegIdx Interp_Not(Interp_Builder* builder, RegIdx src)
{
    return Interp_Unary(builder, Op_Not, src, { 0, 0, 0 });
}

RegIdx Interp_Neg(Interp_Builder* builder, RegIdx src)
{
    return Interp_Unary(builder, Op_Negate, src, { 0, 0, 0 });
}

RegIdx Interp_And(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_And, reg1, reg2);
}

RegIdx Interp_Or(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_And, reg1, reg2);
}

RegIdx Interp_Xor(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_And, reg1, reg2);
}

RegIdx Interp_Sar(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_And, reg1, reg2);
}

RegIdx Interp_ShL(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_And, reg1, reg2);
}

RegIdx Interp_ShR(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_And, reg1, reg2);
}

RegIdx Interp_RoL(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_Rol, reg1, reg2);
}

RegIdx Interp_RoR(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_Ror, reg1, reg2);
}

RegIdx Interp_AtomicLoad(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return RegIdx_Unused;
}

RegIdx Interp_AtomicExchange(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return RegIdx_Unused;
}

RegIdx Interp_AtomicAdd(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return RegIdx_Unused;
}

RegIdx Interp_AtomicSub(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return RegIdx_Unused;
}

RegIdx Interp_AtomicAnd(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return RegIdx_Unused;
}

RegIdx Interp_AtomicXor(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return RegIdx_Unused;
}

RegIdx Interp_AtomicOr(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return RegIdx_Unused;
}

RegIdx Interp_AtomicCmpExchange(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return RegIdx_Unused;
}

RegIdx Interp_FAdd(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_FAdd, reg1, reg2);
}

RegIdx Interp_FSub(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_FSub, reg1, reg2);
}

RegIdx Interp_FMul(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_FMul, reg1, reg2);
}

RegIdx Interp_FDiv(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_FDiv, reg1, reg2);
}

RegIdx Interp_CmpEq(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_CmpEq, reg1, reg2);
}

RegIdx Interp_CmpNe(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_CmpNe, reg1, reg2);
}

RegIdx Interp_CmpILT(Interp_Builder* builder, RegIdx reg1, RegIdx reg2, bool isSigned)
{
    return Interp_Bin(builder, isSigned ? Op_CmpSLT : Op_CmpULT, reg1, reg2);
}

RegIdx Interp_CmpILE(Interp_Builder* builder, RegIdx reg1, RegIdx reg2, bool isSigned) 
{
    return Interp_Bin(builder, isSigned ? Op_CmpSLE : Op_CmpULE, reg1, reg2);
}

RegIdx Interp_CmpIGT(Interp_Builder* builder, RegIdx reg1, RegIdx reg2, bool isSigned)
{
    return Interp_Bin(builder, isSigned ? Op_CmpSLT : Op_CmpULT, reg2, reg1);
}

RegIdx Interp_CmpIGE(Interp_Builder* builder, RegIdx reg1, RegIdx reg2, bool isSigned)
{
    return Interp_Bin(builder, isSigned ? Op_CmpSLE : Op_CmpULE, reg2, reg1);
}

RegIdx Interp_CmpFLT(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_CmpFLT, reg1, reg2);
}

RegIdx Interp_CmpFLE(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_CmpFLE, reg1, reg2);
}

RegIdx Interp_CmpFGT(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_CmpFLT, reg2, reg1);
}

RegIdx Interp_CmpFGE(Interp_Builder* builder, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(builder, Op_CmpFLE, reg2, reg1);
}

// Intrinsics... Not supporting those for now

RegIdx Interp_SysCall(Interp_Builder* builder) {return RegIdx_Unused;}

// Do I need the prototype here?
RegIdx Interp_Call(Interp_Builder* builder, RegIdx target, Array<RegIdx> args)
{
    auto proc = builder->proc;
    uint32 argIdx = Interp_AllocateRegIdxArray(builder, args);
    
    auto newElement = Interp_InsertInstr(builder);
    
    newElement->op = Op_Call;
    newElement->dst         = builder->regCounter;
    newElement->call.target = target;
    newElement->call.argCount = args.length;
    newElement->call.argStart = argIdx;
    
    Interp_AdvanceReg(builder);
    return newElement->dst;
}

void Interp_Safepoint(Interp_Builder* builder) {}

InstrIdx Interp_Goto(Interp_Builder* builder, InstrIdx target)
{
    auto proc = builder->proc;
    auto newElement = Interp_InsertInstr(builder);
    
    newElement->op = Op_Branch;
    newElement->branch.defaultCase = target;
    newElement->branch.keyStart  = 0;
    newElement->branch.caseStart = 0;
    newElement->branch.count     = 0;
    return newElement - proc->instrs.ptr;
}

InstrIdx Interp_Goto(Interp_Builder* builder)
{
    auto proc = builder->proc;
    auto newElement = Interp_InsertInstr(builder);
    
    newElement->op = Op_Branch;
    newElement->branch.defaultCase = 0;
    newElement->branch.keyStart  = 0;
    newElement->branch.caseStart = 0;
    newElement->branch.count     = 0;
    return newElement - proc->instrs.ptr;
}

InstrIdx Interp_If(Interp_Builder* builder, RegIdx value, InstrIdx ifInstr, InstrIdx elseInstr)
{
    auto proc = builder->proc;
    auto newElement = Interp_InsertInstr(builder);
    
    uint32 instrIdx = Interp_AllocateInstrIdxArray(builder, &ifInstr, 1);
    int64 val = 0;
    uint32 valIdx = Interp_AllocateConstArray(builder, &val, 1);
    
    newElement->op = Op_Branch;
    newElement->branch.value       = value;
    newElement->branch.defaultCase = elseInstr;
    newElement->branch.keyStart    = valIdx;
    newElement->branch.caseStart   = instrIdx;
    newElement->branch.count = 1;
    return newElement - proc->instrs.ptr;
}

void Interp_PatchIf(Interp_Builder* builder, InstrIdx ifInstr, RegIdx value, InstrIdx thenInstr, InstrIdx elseInstr)
{
    uint32 instrIdx = Interp_AllocateInstrIdxArray(builder, &thenInstr, 1);
    int64 val = 0;
    uint32 valIdx = Interp_AllocateConstArray(builder, &val, 1);
    
    auto& instr = builder->proc->instrs[ifInstr];
    instr.op = Op_Branch;
    instr.branch.value       = value;
    instr.branch.defaultCase = elseInstr;
    instr.branch.keyStart    = valIdx;
    instr.branch.caseStart   = instrIdx;
    instr.branch.count = 1;
}

void Interp_PatchGoto(Interp_Builder* builder, InstrIdx gotoInstr, InstrIdx target)
{
    auto& instr = builder->proc->instrs[gotoInstr];
    instr.op = Op_Branch;
    instr.branch.defaultCase = target;
}

// Patch if and else instructions index later
InstrIdx Interp_If(Interp_Builder* builder, RegIdx value)
{
    auto proc = builder->proc;
    auto newElement = Interp_InsertInstr(builder);
    
    newElement->op = Op_Branch;
    newElement->branch.value = value;
    newElement->branch.defaultCase = 0;
    newElement->branch.keyStart    = 0;
    newElement->branch.caseStart   = 0;
    newElement->branch.count       = 0;
    return newElement - proc->instrs.ptr;
}

InstrIdx Interp_Branch(Interp_Builder* builder)
{
    auto proc = builder->proc;
    auto newElement = Interp_InsertInstr(builder);
    
    newElement->op = Op_Branch;
    newElement->branch.value       = 0;
    newElement->branch.defaultCase = 0;
    newElement->branch.keyStart    = 0;
    newElement->branch.caseStart   = 0;
    newElement->branch.count       = 0;
    return newElement - proc->instrs.ptr;
}

void Interp_Return(Interp_Builder* builder, RegIdx retValue)
{
    auto proc = builder->proc;
    auto newElement = Interp_InsertInstr(builder);
    
    
    newElement->op = Op_Ret;
    newElement->unary.src = retValue;
}

void Interp_ReturnVoid(Interp_Builder* builder)
{
    auto proc = builder->proc;
    auto newElement = Interp_InsertInstr(builder);
    
    
    newElement->op = Op_Ret;
    newElement->bitfield |= InstrBF_RetVoid;
    newElement->unary.src = 0;
}

void Interp_PrintInstr(Interp_Proc* proc, Interp_Instr* instr)
{
    if(instr->bitfield & InstrBF_ViolatesSSA)
        printf("(Violates SSA) ");
    
    if(instr->bitfield & InstrBF_MergeSSA)
        printf("(Merge SSA) ");
    
    if(Interp_OpUsesDst[instr->op])
    {
        printf("%%%d = ", instr->dst);
    }
    
    if(instr->op != Op_Branch)  // Will have custom print depending on situation
        printf("%s ", Interp_OpStrings[instr->op]);
    
    switch(instr->op)
    {
        case Op_Null:         break;
        case Op_IntegerConst: printf("%lld", instr->imm.intVal); break;
        case Op_Float32Const: printf("%f", instr->imm.floatVal); break;
        case Op_Float64Const: printf("%lf", instr->imm.doubleVal); break;
        case Op_Region:       break;
        case Op_Call:
        {
            printf("%%%d (", instr->call.target);
            
            uint32 start = instr->call.argStart;
            uint32 end = start + instr->call.argCount;
            for(int i = start; i < end; ++i)
            {
                printf("%%%d", proc->regArrays[i]);
                
                if(i < end-1)
                    printf(", ");
            }
            
            printf(")");
            
            break;
        }
        case Op_SysCall:     break;
        case Op_Store:
        {
            printf("%%%d ", instr->store.addr);
            printf("(val: %%%d, align: %I64u)", instr->store.val, instr->store.align);
            break;
        }
        case Op_MemCpy:
        {
            printf("(dst: %%%d, src: %%%d, count: %%%d, align: %lld)", instr->memcpy.dst, instr->memcpy.src, instr->memcpy.count, instr->memcpy.align);
            break;
        }
        case Op_MemSet:
        {
            break;
        }
        case Op_AtomicTestAndSet:  break;
        case Op_AtomicClear:       break;
        case Op_AtomicLoad:        break;
        case Op_AtomicExchange:    break;
        case Op_AtomicAdd:         break;
        case Op_AtomicSub:         break;
        case Op_AtomicAnd:         break;
        case Op_AtomicXor:         break;
        case Op_AtomicOr:          break;
        case Op_AtomicCompareExchange: break;
        case Op_DebugBreak: break;
        case Op_Branch:
        {
            if(instr->branch.count == 0)
            {
                printf("Goto @%d", instr->branch.defaultCase);
            }
            else
            {
                printf("Branch %%%d (", instr->branch.value);
                auto start = instr->branch.keyStart;
                auto end = start + instr->branch.count;
                for(int i = start; i < end; ++i)
                {
                    printf("%lld", proc->constArrays[i]);
                    
                    if(i < end - 1)
                        printf(" ");
                }
                
                printf(") -> (");
                start = instr->branch.caseStart;
                end = start + instr->branch.count;
                for(int i = start; i < end; ++i)
                {
                    printf("@%d", proc->instrArrays[i]);
                    
                    if(i < end - 1)
                        printf(", ");
                }
                
                printf("), default: @%d", instr->branch.defaultCase);
            }
            
            break;
        }
        case Op_Ret:
        {
            if(!(instr->bitfield & InstrBF_RetVoid))
                Interp_PrintUnary(instr);
            break;
        }
        case Op_Load:
        {
            printf("%%%d, %llu", instr->load.addr, instr->load.align);
            break;
        }
        case Op_Local:
        {
            printf("(idx: %d, size: %d, align: %d)", instr->dst, instr->local.size, instr->local.align);
            break;
        }
        case Op_GetSymbolAddress:
        {
            auto symbol = instr->symAddress.symbol;
            if(!symbol)
                printf("NULL");
            else
            {
                printf("%s",  instr->symAddress.symbol->decl->name->string);
            }
            break;
        }
        case Op_MemberAccess:
        {
            printf("(base: %%%d, offset: %lld)", instr->memacc.base, instr->memacc.offset);
            break;
        }
        case Op_ArrayAccess:
        {
            break;
        }
        
        case Op_Truncate:
        case Op_FloatExt:
        case Op_SignExt:
        case Op_ZeroExt:
        case Op_Int2Ptr:
        case Op_Ptr2Int:
        case Op_Uint2Float:
        case Op_Float2Uint:
        case Op_Int2Float:
        case Op_Float2Int:
        case Op_Bitcast:
        case Op_Select:
        case Op_Not:
        case Op_Negate:
        Interp_PrintUnary(instr); break;
        
        case Op_And:
        case Op_Or:
        case Op_Xor:
        case Op_Add:
        case Op_Sub:
        case Op_Mul:
        case Op_ShL:
        case Op_ShR:
        case Op_Sar:
        case Op_Rol:
        case Op_Ror:
        case Op_UDiv:
        case Op_SDiv:
        case Op_UMod:
        case Op_SMod:
        case Op_FAdd:
        case Op_FSub:
        case Op_FMul:
        case Op_FDiv:
        case Op_CmpEq:
        case Op_CmpNe:
        case Op_CmpULT:
        case Op_CmpULE:
        case Op_CmpSLT:
        case Op_CmpSLE:
        case Op_CmpFLT:
        case Op_CmpFLE:
        Interp_PrintBin(instr); break;
        
        default: printf("Unknown operation");
    }
    
    printf("\n");
}

void Interp_PrintBin(Interp_Instr* instr)
{
    printf("%%%d, %%%d", instr->bin.src1, instr->bin.src2);
}

void Interp_PrintUnary(Interp_Instr* instr)
{
    printf("%%%d", instr->unary.src);
}

void Interp_PrintPassRule(TB_PassingRule rule)
{
    switch(rule)
    {
        case TB_PASSING_DIRECT:   printf("direct");   break;
        case TB_PASSING_INDIRECT: printf("indirect"); break;
        case TB_PASSING_IGNORE:   printf("ignore");   break;
        default: Assert(false);
    }
}

void Interp_PrintProc(Interp_Proc* proc)
{
    // Print proc name, etc.
    printf("%s:\n", proc->symbol->name);
    
    // Print passing rules
    printf("Arg passing rules: ");
    for_array(i, proc->argRules)
    {
        RegIdx curReg = (proc->retRule == TB_PASSING_INDIRECT) + i;
        printf("%%%d ", curReg);
        Interp_PrintPassRule(proc->argRules[i]);
        
        if(i < proc->argRules.length - 1)
            printf(", ");
    }
    
    printf("\n");
    printf("Ret passing rule: ");
    if(proc->retRule == TB_PASSING_INDIRECT)
        printf("%%%d ", 0);
    Interp_PrintPassRule(proc->retRule);
    printf("\n");
    
    int counter = 0;
    const int numSpaces = 5;
    
    for_array(i, proc->instrs)
    {
        int numChars = printf("%d:", counter++);
        for(int i = numChars; i < numSpaces; ++i)
            printf(" ");
        
        Interp_PrintInstr(proc, &proc->instrs[i]);
    }
    
    printf("\n");
}
