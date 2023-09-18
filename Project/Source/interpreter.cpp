
#include "base.h"
#include "interpreter.h"

////
// Bytecode generation helper functions
////

// Returns the old register and advances the current one
// Works kind of like post-order ++ operator
void Interp_AdvanceReg(Interp_Proc* proc)
{
    // Find the next register in the permanentRegs array
    ++proc->regCounter;
    int foundIdx = -1;
    for_array(i, proc->permanentRegs)
    {
        if(proc->permanentRegs[i] == proc->regCounter)
        {
            foundIdx = i;
            break;
        }
    }
    
    if(foundIdx == -1)
        return;
    
    ++proc->regCounter;
    
    for(int i = foundIdx + 1; i < proc->permanentRegs.length; ++i)
    {
        if(proc->permanentRegs[i] != proc->regCounter)
            break;
        
        ++proc->regCounter;
    }
}

RegIdx Interp_GetFirstUnused(Interp_Proc* proc)
{
    if(proc->permanentRegs.length == 0) return 0;
    
    // Get min
    RegIdx minPermanent = proc->permanentRegs[0];
    if(minPermanent > 0) return 0;
    
    RegIdx res = 0;
    for(int i = 0; i < proc->permanentRegs.length; ++i)
    {
        if(proc->permanentRegs[i] != res)
            break;
        
        ++res;
    }
    
    return res;
}

Interp_Proc* Interp_MakeProc(Interp* interp)
{
    interp->procs.ResizeAndInit(interp->procs.length + 1);
    auto& newElement = interp->procs[interp->procs.length - 1];
    newElement.module = interp->module;
    return &newElement;
}

Interp_Symbol* Interp_MakeSymbol(Interp* interp)
{
    interp->symbols.ResizeAndInit(interp->symbols.length + 1);
    auto& newElement = interp->symbols[interp->symbols.length - 1];
    return &newElement;
}

uint32 Interp_AllocateRegIdxArray(Interp_Proc* proc, RegIdx* values, uint16 arrayCount)
{
    uint32 oldLength = (uint32)proc->regArrays.length;
    proc->regArrays.ResizeAndInit(proc->regArrays.length + arrayCount);
    
    for(int i = oldLength; i < proc->regArrays.length; ++i)
    {
        int j = i - oldLength;
        proc->regArrays[i] = values[j];
    }
    
    return oldLength;
}

uint32 Interp_AllocateRegIdxArray(Interp_Proc* proc, Array<RegIdx> values)
{
    return Interp_AllocateRegIdxArray(proc, values.ptr, values.length);
}

uint32 Interp_AllocateInstrIdxArray(Interp_Proc* proc, InstrIdx* values, uint32 arrayCount)
{
    uint32 oldLength = (uint32)proc->instrArrays.length;
    proc->instrArrays.ResizeAndInit(proc->instrArrays.length + arrayCount);
    
    for(int i = oldLength; i < proc->instrArrays.length; ++i)
    {
        int j = i - oldLength;
        proc->instrArrays[i] = values[j];
    }
    
    return oldLength;
}

uint32 Interp_AllocateInstrIdxArray(Interp_Proc* proc, Array<InstrIdx> instrs)
{
    return Interp_AllocateInstrIdxArray(proc, instrs.ptr, instrs.length);
}

uint32 Interp_AllocateConstArray(Interp_Proc* proc, int64* values, uint32 count)
{
    uint32 oldLength = (uint32)proc->constArrays.length;
    proc->constArrays.ResizeAndInit(proc->constArrays.length + count);
    
    for(int i = oldLength; i < proc->constArrays.length; ++i)
    {
        int j = i - oldLength;
        proc->constArrays[i] = values[j];
    }
    
    return oldLength;
}

uint32 Interp_AllocateConstArray(Interp_Proc* proc, Array<int64> consts)
{
    return Interp_AllocateConstArray(proc, consts.ptr, consts.length);
}

RegIdx Interp_BuildPreRet(Interp_Proc* proc, RegIdx arg, Interp_Val val, TypeInfo* type, TB_PassingRule rule)
{
    RegIdx res = 0;
    switch(rule)
    {
        default: Assert(false && "TB added new passing rule which is not handled");
        case TB_PASSING_IGNORE: break;
        case TB_PASSING_DIRECT:
        {
            if(val.isLValue)
            {
                // TODO: This is an "Implicit array to pointer" conversion, test this
                if(type->typeId == Typeid_Arr)
                    res = val.reg;
                else
                {
                    uint16 typeSize = GetTypeSize(type);
                    uint16 typeAlign = GetTypeAlign(type);
                    Assert(typeSize <= 8);
                    Interp_Type dt = Interp_ConvertType(type);
                    
                    res = Interp_Load(proc, dt, val.reg, typeAlign, false);
                }
            }
            else
            {
                res = val.reg;
            }
            
            break;
        }
        case TB_PASSING_INDIRECT:
        {
            // Perform a Memcpy with the caller-owned buffer as
            // destination
            RegIdx dstReg  = arg;
            RegIdx sizeReg = Interp_ImmUInt(proc, Interp_Int64, GetTypeSize(type));
            Interp_MemCpy(proc, dstReg, val.reg, sizeReg, GetTypeAlign(type), false);
            res = arg;  // Return the address
            break;
        }
    }
    
    return res;
}

// TODO: This will, in the future, return an array of registers
// because systemv can split a single struct into multiple registers.
RegIdx Interp_PassArg(Interp_Proc* proc, Interp_Val arg, TypeInfo* type, TB_PassingRule rule, Arena* allocTo)
{
    RegIdx res = 0;
    
    auto size = GetTypeSize(type);
    auto align = GetTypeAlign(type);
    
    switch(rule)
    {
        default: Assert(false);
        case TB_PASSING_INDIRECT:
        {
            
            // First retrieve the address
            RegIdx addr = RegIdx_Unused;
            
            if(arg.isLValue)
                addr = arg.reg;
            else
            {
                auto dt = Interp_ConvertType(type);
                
                // This means that it was a temporary computation
                // such as 2+2, it's necessary to stack allocate a
                // new variable to hold this value
                auto local = Interp_Local(proc, size, align);
                Interp_Store(proc, dt, local, arg.reg, align, false);
                addr = local;
            }
            
            Assert(addr != RegIdx_Unused);
            
            // Now given the address, create a copy of it and pass it to the proc
            RegIdx local   = Interp_Local(proc, size, align);
            RegIdx sizeReg = Interp_ImmUInt(proc, Interp_Int64, size);
            
            Interp_MemCpy(proc, local, addr, sizeReg, align, false);
            
            res = local;
            break;
        }
        case TB_PASSING_DIRECT:
        {
            if(arg.isLValue)
            {
                Assert(size <= 8);
                Interp_Type dt = { 0, 0, 0 };
                
                if(type->typeId == Typeid_Struct ||
                   type->typeId == Typeid_Ident)
                    dt = { InterpType_Int, 0, uint16(size * 8) };  // Use int type for structs
                else
                    dt = Interp_ConvertType(type);
                
                res = Interp_Load(proc, dt, arg.reg, align, false);
            }
            else
            {
                res = arg.reg;
            }
            
            break;
        }
        case TB_PASSING_IGNORE: break;
    }
    
    return res;
}

Interp_Val Interp_GetRet(Interp_Proc* proc, TB_PassingRule rule, RegIdx retReg, TypeInfo* retType)
{
    // If it's indirect, just retrieve the address
    if (rule == TB_PASSING_INDIRECT) return { retReg, true };
    
    Interp_Val res = { 0, 0 };
    
    auto size = GetTypeSize(retType);
    auto align = GetTypeAlign(retType);
    
    // Structs can only be stack variables for
    // bytecode generation purposes, so we need to
    // create a stack variable
    if(retType->typeId == Typeid_Struct ||
       retType->typeId == Typeid_Ident)
    {
        // Spawn a temporary stack variable
        RegIdx local = Interp_Local(proc, size, align);
        Interp_Type dt = { InterpType_Int, 0, uint16(size * 8) };
        //Interp_Store(proc, dt, local, retReg, align, false);
        res = { local, true };
    }
    else
    {
        res = { retReg, false };
    }
    
    return res;
}

RegIdx Interp_GetRVal(Interp_Proc* proc, Interp_Val val, TypeInfo* type)
{
    Assert(val.reg != RegIdx_Unused);
    
    if(val.isLValue)
    {
        val.reg = Interp_Load(proc, Interp_ConvertType(type), val.reg, GetTypeAlign(type), false);
        val.isLValue = false;
    }
    
    return val.reg;
}

// TODO: This code is very "copy-paste"-y...
// I mean, the OOP way of doing this would be to
// just have a bunch of constructors which is exactly
// what this is... but split into multiple files, which
// is arguably worse

Interp Interp_Init()
{
    Interp interp;
    
    TB_FeatureSet features = { 0 };
    TB_Arch arch = TB_ARCH_X86_64;
    TB_System sys = TB_SYSTEM_WINDOWS;
    TB_DebugFormat debugFmt = TB_DEBUGFMT_NONE;
    interp.module = tb_module_create(arch, sys, &features, false);
    
    return interp;
}

InstrIdx Interp_Region(Interp_Proc* proc)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Region;
    newElement.dst = proc->regCounter;
    return proc->instrs.length - 1;
}

void Interp_DebugBreak(Interp_Proc* proc)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op  = Op_DebugBreak;
    newElement.dst = 0;
}

RegIdx Interp_Bin(Interp_Proc* proc, Interp_OpCode op, RegIdx reg1, RegIdx reg2)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op       = op;
    newElement.dst      = proc->regCounter;
    newElement.bin.src1 = reg1;
    newElement.bin.src2 = reg2;
    Interp_AdvanceReg(proc);
    return newElement.dst;
}

RegIdx Interp_Unary(Interp_Proc* proc, Interp_OpCode op, RegIdx src, Interp_Type type)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op   = op;
    newElement.dst  = proc->regCounter;
    newElement.unary.type = type;
    newElement.unary.src  = src;
    
    proc->permanentRegs.Append(proc->regCounter);
    Interp_AdvanceReg(proc);
    return newElement.dst;
}

RegIdx Interp_Unary(Interp_Proc* proc, Interp_OpCode op, RegIdx src)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op   = op;
    newElement.dst  = proc->regCounter;
    newElement.unary.src  = src;
    
    proc->permanentRegs.Append(proc->regCounter);
    Interp_AdvanceReg(proc);
    return newElement.dst;
}

// void Interp_Param(Interp_Proc* proc) {}

RegIdx Interp_FPExt(Interp_Proc* proc, RegIdx src, Interp_Type type)
{
    return Interp_Unary(proc, Op_FloatExt, src, type);
}

RegIdx Interp_SExt(Interp_Proc* proc, RegIdx src, Interp_Type type)
{
    return Interp_Unary(proc, Op_SignExt, src, type);
}

RegIdx Interp_ZExt(Interp_Proc* proc, RegIdx src, Interp_Type type)
{
    return Interp_Unary(proc, Op_ZeroExt, src, type);
}

RegIdx Interp_Trunc(Interp_Proc* proc, RegIdx src, Interp_Type type)
{
    return Interp_Unary(proc, Op_Truncate, src, type);
}

RegIdx Interp_Int2Ptr(Interp_Proc* proc, RegIdx src, Interp_Type type)
{
    return Interp_Unary(proc, Op_Int2Ptr, src, type);
}

RegIdx Interp_Ptr2Int(Interp_Proc* proc, RegIdx src, Interp_Type type)
{
    return Interp_Unary(proc, Op_Ptr2Int, src, type);
}

// TODO: If it's a constant, remove the unnecessary cast and directly convert the constant
RegIdx Interp_Int2Float(Interp_Proc* proc, RegIdx src, Interp_Type type, bool isSigned)
{
    return Interp_Unary(proc, isSigned? Op_Int2Float : Op_Uint2Float, src, type);
}

RegIdx Interp_Float2Int(Interp_Proc* proc, RegIdx src, Interp_Type type, bool isSigned)
{
    return Interp_Unary(proc, isSigned? Op_Float2Int : Op_Float2Uint, src, type);
}

RegIdx Interp_Bitcast(Interp_Proc* proc, RegIdx src, Interp_Type type)
{
    return Interp_Unary(proc, Op_Bitcast, src, type);
}

// Local uses reg1 and reg2 to store size and alignment,
// Dest is the resulting address
RegIdx Interp_Local(Interp_Proc* proc, uint64 size, uint64 align, bool setUpArg)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op          = Op_Local;
    newElement.dst         = proc->regCounter;
    newElement.local.size  = size;
    newElement.local.align = align;
    if(setUpArg) newElement.bitfield |= InstrBF_SetUpArgs;
    
    // Addresses for locals are used across different
    // statements, so they need to be added to the
    // permanentRegs array
    proc->permanentRegs.Append(proc->regCounter);
    Interp_AdvanceReg(proc);
    return newElement.dst;
}

// Ignoring atomics for now
RegIdx Interp_Load(Interp_Proc* proc, Interp_Type type, RegIdx addr, uint64 align, bool isVolatile)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op         = Op_Load;
    newElement.dst        = proc->regCounter;
    newElement.load.addr  = addr;
    newElement.load.align = align;
    newElement.load.type  = type;
    
    Interp_AdvanceReg(proc);
    return newElement.dst;
}

Interp_Instr* Interp_Store(Interp_Proc* proc, Interp_Type type, RegIdx addr, RegIdx val, uint64 align, bool isVolatile, bool setUpArg)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op          = Op_Store;
    newElement.dst         = 0;
    newElement.store.addr  = addr;
    newElement.store.align = align;
    newElement.store.val   = val;
    if(setUpArg) newElement.bitfield |= InstrBF_SetUpArgs;
    
    return &newElement;
}

void Interp_ImmBool(Interp_Proc* proc) {}

RegIdx Interp_ImmSInt(Interp_Proc* proc, Interp_Type type, int64 imm)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op         = Op_IntegerConst;
    newElement.dst        = proc->regCounter;
    newElement.imm.intVal = imm;
    newElement.imm.type   = type;
    Interp_AdvanceReg(proc);
    return newElement.dst;
}

RegIdx Interp_ImmUInt(Interp_Proc* proc, Interp_Type type, uint64 imm)
{
    // Zero out all bits which are not supposedly used
    uint64 mask = ~0ULL >> (type.data);
    imm &= mask;
    
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op         = Op_IntegerConst;
    newElement.dst        = proc->regCounter;
    newElement.imm.intVal = imm;
    newElement.imm.type   = type;
    Interp_AdvanceReg(proc);
    return newElement.dst;
}

RegIdx Interp_ImmFloat32(Interp_Proc* proc, Interp_Type type, float imm)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op           = Op_Float32Const;
    newElement.dst          = proc->regCounter;
    newElement.imm.floatVal = imm;
    newElement.imm.type     = type;
    Interp_AdvanceReg(proc);
    return newElement.dst;
}

RegIdx Interp_ImmFloat64(Interp_Proc* proc, Interp_Type type, double imm)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op            = Op_Float64Const;
    newElement.dst           = proc->regCounter;
    newElement.imm.doubleVal = imm;
    newElement.imm.type      = type;
    Interp_AdvanceReg(proc);
    return newElement.dst;
}

// void Interp_CString(Interp_Proc* proc) {}

// void Interp_String(Interp_Proc* proc) {}

void Interp_MemSet(Interp_Proc* proc) {}

void Interp_MemZero(Interp_Proc* proc) {}

void Interp_MemCpy(Interp_Proc* proc, RegIdx dst, RegIdx src, RegIdx count, uint64 align, bool isVolatile)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op           = Op_MemCpy;
    newElement.memcpy.dst   = dst;
    newElement.memcpy.src   = src;
    newElement.memcpy.count = count;
    newElement.memcpy.align = align;
}

void Interp_ArrayAccess(Interp_Proc* proc) {}

void Interp_MemberAccess(Interp_Proc* proc) {}

RegIdx Interp_GetSymbolAddress(Interp_Proc* proc, Interp_Symbol* symbol)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op  = Op_GetSymbolAddress;
    newElement.dst = proc->regCounter;
    newElement.symAddress.symbol = symbol;
    Interp_AdvanceReg(proc);
    return newElement.dst;
}

void Interp_Select(Interp_Proc* proc)
{
    
}

// @incomplete
// This should support different kinds of arithmetic behavior 
RegIdx Interp_Add(Interp_Proc* proc, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(proc, Op_Add, reg1, reg2);
}

RegIdx Interp_Sub(Interp_Proc* proc, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(proc, Op_Sub, reg1, reg2);
}

RegIdx Interp_Mul(Interp_Proc* proc, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(proc, Op_Mul, reg1, reg2);
}

RegIdx Interp_Div(Interp_Proc* proc, RegIdx reg1, RegIdx reg2, bool signedness)
{
    return Interp_Bin(proc, signedness? Op_SDiv : Op_UDiv, reg1, reg2);
}

RegIdx Interp_Mod(Interp_Proc* proc, RegIdx reg1, RegIdx reg2, bool signedness)
{
    return Interp_Bin(proc, signedness? Op_SMod : Op_UMod, reg1, reg2);
}

void Interp_BSwap(Interp_Proc* proc)
{
    
}

//void Interp_CLZ(Interp_Proc* proc) {}

//void Interp_CTZ(Interp_Proc* proc) {}

//void Interp_PopCount(Interp_Proc* proc) {}

void Interp_Not(Interp_Proc* proc)
{
    
}

void Interp_Neg(Interp_Proc* proc) {}

void Interp_And(Interp_Proc* proc)
{
    
}

void Interp_Or(Interp_Proc* proc)
{
    
}

void Interp_Xor(Interp_Proc* proc)
{
    
}

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

RegIdx Interp_FAdd(Interp_Proc* proc, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(proc, Op_FAdd, reg1, reg2);
}

RegIdx Interp_FSub(Interp_Proc* proc, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(proc, Op_FSub, reg1, reg2);
}

RegIdx Interp_FMul(Interp_Proc* proc, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(proc, Op_FMul, reg1, reg2);
}

RegIdx Interp_FDiv(Interp_Proc* proc, RegIdx reg1, RegIdx reg2)
{
    return Interp_Bin(proc, Op_FDiv, reg1, reg2);
}

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

// Do I need the prototype here?
RegIdx Interp_Call(Interp_Proc* proc, RegIdx target, Array<RegIdx> args)
{
    uint32 argIdx = Interp_AllocateRegIdxArray(proc, args);
    
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Call;
    newElement.dst         = proc->regCounter;
    newElement.call.target = target;
    newElement.call.argCount = args.length;
    newElement.call.argStart = argIdx;
    
    Interp_AdvanceReg(proc);
    return newElement.dst;
}

void Interp_Safepoint(Interp_Proc* proc) {}

Interp_Instr* Interp_Goto(Interp_Proc* proc, InstrIdx target)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Branch;
    newElement.branch.defaultCase = target;
    newElement.branch.keyStart  = 0;
    newElement.branch.caseStart = 0;
    newElement.branch.count     = 0;
    return &newElement;
}

Interp_Instr* Interp_Goto(Interp_Proc* proc)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Branch;
    newElement.branch.defaultCase = 0;
    newElement.branch.keyStart  = 0;
    newElement.branch.caseStart = 0;
    newElement.branch.count     = 0;
    return &newElement;
}

Interp_Instr* Interp_If(Interp_Proc* proc, RegIdx value, InstrIdx ifInstr, InstrIdx elseInstr)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    uint32 instrIdx = Interp_AllocateInstrIdxArray(proc, &ifInstr, 1);
    int64 val = 0;
    uint32 valIdx = Interp_AllocateConstArray(proc, &val, 1);
    
    newElement.op = Op_Branch;
    newElement.branch.value       = value;
    newElement.branch.defaultCase = elseInstr;
    newElement.branch.keyStart    = valIdx;
    newElement.branch.caseStart   = instrIdx;
    newElement.branch.count = 1;
    return &newElement;
}

// Patch if and else instructions index later
Interp_Instr* Interp_If(Interp_Proc* proc, RegIdx value)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Branch;
    newElement.branch.value = value;
    newElement.branch.defaultCase = 0;
    newElement.branch.keyStart    = 0;
    newElement.branch.caseStart   = 0;
    newElement.branch.count       = 0;
    return &newElement;
}

Interp_Instr* Interp_Branch(Interp_Proc* proc)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Branch;
    newElement.branch.value       = 0;
    newElement.branch.defaultCase = 0;
    newElement.branch.keyStart    = 0;
    newElement.branch.caseStart   = 0;
    newElement.branch.count       = 0;
    return &newElement;
}

void Interp_Return(Interp_Proc* proc, RegIdx retValue)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Ret;
    newElement.unary.src = retValue;
}

void Interp_ReturnVoid(Interp_Proc* proc)
{
    proc->instrs.ResizeAndInit(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Ret;
    newElement.bitfield |= InstrBF_RetVoid;
    newElement.unary.src = 0;
}

////
// AST -> bytecode conversion
////

// At this point the registers can be reused
void Interp_EndOfExpression(Interp_Proc* proc)
{
    proc->regCounter = Interp_GetFirstUnused(proc);
}

RegIdx Interp_ConvertNodeRVal(Interp_Proc* proc, Ast_Node* node)
{
    Assert(Ast_IsExpr(node));
    auto expr = (Ast_Expr*)node;
    
    auto val = Interp_ConvertNode(proc, node);
    return Interp_GetRVal(proc, val, expr->type);
}

Interp_Val Interp_ConvertNode(Interp_Proc* proc, Ast_Node* node)
{
    Interp_Val res = { RegIdx_Unused, false };
    
    switch(node->kind)
    {
        case AstKind_ProcDef:       break;  // Not handled here
        case AstKind_StructDef:     break;  // Not handled here
        case AstKind_DeclBegin:     break;
        case AstKind_VarDecl:
        {
            auto varDecl = (Ast_VarDecl*)node;
            auto type = varDecl->type;
            
            if(varDecl->initExpr)
            {
                auto dst = proc->declToAddr[varDecl->declIdx];
                Interp_Val dstVal = { dst, true };
                auto src = Interp_ConvertNode(proc, varDecl->initExpr);
                Interp_Assign(proc, src, varDecl->initExpr->type, dstVal, varDecl->type);
                
                //auto exprReg = Interp_ConvertNodeRVal(proc, varDecl->initExpr);
                //Interp_EndOfExpression(proc);
                //Interp_Store(proc, Interp_ConvertType(varDecl->type), addrReg, exprReg, GetTypeAlign(type), false);
            }
            
            break;
        }
        case AstKind_EnumDecl:      break;
        case AstKind_DeclEnd:       break;
        case AstKind_StmtBegin:     break;
        case AstKind_Block:         Interp_ConvertBlock(proc, (Ast_Block*)node); break;
        case AstKind_If:
        {
            auto stmt = (Ast_If*)node;
            RegIdx value = Interp_ConvertNodeRVal(proc, stmt->condition);
            Interp_EndOfExpression(proc);
            
            auto ifInstr = Interp_If(proc, value);
            auto ifRegionIdx = Interp_Region(proc);
            
            uint32 instrArrayStart = Interp_AllocateInstrIdxArray(proc, &ifRegionIdx, 1);
            int64 val = 0;
            uint32 constArrayStart = Interp_AllocateConstArray(proc, &val, 1);
            
            ifInstr->branch.keyStart  = constArrayStart;
            ifInstr->branch.caseStart = instrArrayStart;
            ifInstr->branch.count     = 1;
            
            Interp_ConvertBlock(proc, stmt->thenBlock);
            auto gotoInstr = Interp_Goto(proc);
            
            auto elseRegionIdx = Interp_Region(proc);
            ifInstr->branch.defaultCase = elseRegionIdx;
            gotoInstr->branch.defaultCase = elseRegionIdx;
            
            if(stmt->elseStmt)
            {
                Interp_ConvertNode(proc, stmt->elseStmt);
                auto elseGotoInstr = Interp_Goto(proc);
                auto endRegionIdx = Interp_Region(proc);
                elseGotoInstr->branch.defaultCase = endRegionIdx;
                gotoInstr->branch.defaultCase = endRegionIdx;
            }
            
            break;
        }
        case AstKind_For:           break;
        case AstKind_While:         break;
        case AstKind_DoWhile:       break;
        case AstKind_Switch:        break;
        case AstKind_Defer:         break;
        case AstKind_Return:
        {
            auto stmt = (Ast_Return*)node;
            if(stmt->rets.length <= 0)
                Interp_ReturnVoid(proc);
            else
            {
                auto procType = (Ast_DeclaratorProc*)proc->symbol->type;
                
                ScratchArena scratch;
                Array<Interp_Val> regs = { 0, 0 };
                for_array(i, stmt->rets)
                {
                    auto ret = stmt->rets[i];
                    auto reg = Interp_ConvertNode(proc, ret);
                    regs.Append(scratch, reg);
                }
                
                for(int i = 0; i < stmt->rets.length - 1; ++i)
                {
                    RegIdx addr = (proc->retRule == TB_PASSING_INDIRECT) + i;
                    Interp_BuildPreRet(proc, addr, regs[i], stmt->rets[i]->type, TB_PASSING_INDIRECT);
                }
                
                int lastRet = stmt->rets.length - 1;
                auto toRet = Interp_BuildPreRet(proc, 0, regs[lastRet], stmt->rets[lastRet]->type, proc->retRule);
                Interp_Return(proc, toRet);
            }
            
            break;
        }
        case AstKind_Break:         break;
        case AstKind_Continue:      break;
        case AstKind_MultiAssign:   break;
        case AstKind_EmptyStmt:     break;
        case AstKind_StmtEnd:       break;
        case AstKind_ExprBegin:     break;
        case AstKind_Ident:
        {
            auto expr = (Ast_IdentExpr*)node;
            
            if(expr->declaration->kind == AstKind_ProcDef)  // Procedure
            {
                auto procDef = (Ast_ProcDef*)expr->declaration;
                auto address = Interp_GetSymbolAddress(proc, procDef->symbol);
                
                res.reg = address;
            }
            else  // Variable
            {
                Assert(expr->declaration->kind == AstKind_VarDecl);
                auto decl = (Ast_VarDecl*)expr->declaration;
                
                bool isGlobalVariable = decl->declIdx == -1;
                if(isGlobalVariable)
                {
                    Assert(false);
                }
                else
                {
                    res.reg = proc->declToAddr[decl->declIdx];
                }
            }
            
            res.isLValue = true;
            break;
        }
        case AstKind_FuncCall:
        {
            ScratchArena scratch;
            auto values = Interp_ConvertCall(proc, (Ast_FuncCall*)node, scratch);
            // In this context only 1 return is allowed
            Assert(values.length == 0 || values.length == 1);
            
            if(values.length == 1)
                res = values[0];
            break;
        }
        case AstKind_BinaryExpr:
        {
            auto expr = (Ast_BinaryExpr*)node;
            
            // TODO: put other assignments here 
            if(expr->op == '=')
            {
                auto lhs = Interp_ConvertNode(proc, expr->lhs);
                auto rhs = Interp_ConvertNode(proc, expr->rhs);
                auto val = Interp_Assign(proc, rhs, expr->rhs->type, lhs, expr->lhs->type);
                res = val;
            }
            else
            {
                RegIdx lhs = Interp_ConvertNodeRVal(proc, expr->lhs);
                RegIdx rhs = Interp_ConvertNodeRVal(proc, expr->rhs);
                Assert(lhs != RegIdx_Unused && rhs != RegIdx_Unused);
                
                if(IsTypeIntegral(expr->type))
                {
                    bool isSigned = IsTypeSigned(expr->type);
                    switch(expr->op)
                    {
                        case '=': break;  // Already handled before
                        case '+': res.reg = Interp_Add(proc, lhs, rhs); break;
                        case '-': res.reg = Interp_Sub(proc, lhs, rhs); break;
                        case '*': res.reg = Interp_Mul(proc, lhs, rhs); break;
                        case '/': res.reg = Interp_Div(proc, lhs, rhs, isSigned); break;
                        case '%': res.reg = Interp_Mod(proc, lhs, rhs, isSigned); break;
                    }
                }
                else if(IsTypeFloat(expr->type))
                {
                    switch(expr->op)
                    {
                        case '=': break;  // Already handled before
                        case '+': res.reg = Interp_FAdd(proc, lhs, rhs); break;
                        case '-': res.reg = Interp_FSub(proc, lhs, rhs); break;
                        case '*': res.reg = Interp_FMul(proc, lhs, rhs); break;
                        case '/': res.reg = Interp_FDiv(proc, lhs, rhs); break;
                        case '%': Assert(false);
                    }
                }
            }
            
            break;
        }
        case AstKind_UnaryExpr:     break;
        case AstKind_TernaryExpr:   break;
        case AstKind_Typecast:
        {
            auto expr = (Ast_Typecast*)node;
            auto srcType = Interp_ConvertType(expr->expr->type);
            RegIdx exprReg = Interp_ConvertNodeRVal(proc, expr->expr);
            res.reg = Interp_ConvertTypeConversion(proc, exprReg, srcType, expr->expr->type, expr->type);
            break;
        }
        case AstKind_Subscript:     break;
        case AstKind_MemberAccess:  break;
        case AstKind_ConstValue:
        {
            auto expr = (Ast_ConstValue*)node;
            Interp_Type type = Interp_ConvertType(expr->type);
            
            switch_nocheck(expr->type->typeId)
            {
                case Typeid_Int64:
                {
                    int64 val = *(int64*)expr->addr;
                    res.reg = Interp_ImmSInt(proc, type, val);
                    break;
                }
                case Typeid_Float:
                {
                    float val = *(float*)expr->addr;
                    res.reg = Interp_ImmFloat32(proc, type, val);
                    break;
                }
                case Typeid_Double:
                {
                    double val = *(double*)expr->addr;
                    res.reg = Interp_ImmFloat64(proc, type, val);
                    break;
                }
                default: Assert(false);
            } switch_nocheck_end;
            
            break;
        }
        case AstKind_ExprEnd:       break;
        default: Assert(false && "Enum value out of bounds");
    }
    
    if(Ast_IsExpr(node))
    {
        auto expr = (Ast_Expr*)node;
        // Convert type
        if(expr->castType->typeId != expr->type->typeId)
        {
            res.reg = Interp_GetRVal(proc, res, expr->type);
            res.isLValue = false;
            
            auto srcType = Interp_ConvertType(expr->type);
            res.reg = Interp_ConvertTypeConversion(proc, res.reg, srcType, expr->type, expr->castType);
        }
    }
    
    return res;
}

Interp_Val Interp_Assign(Interp_Proc* proc, Interp_Val src, TypeInfo* srcType, Interp_Val dst, TypeInfo* dstType)
{
    Assert(dst.isLValue);
    
    TypeInfo* type = dstType;
    auto size = GetTypeSize(type);
    auto align = GetTypeAlign(type);
    
    if(dstType->typeId == Typeid_Struct ||
       dstType->typeId == Typeid_Ident)
    {
        Assert(src.isLValue);
        RegIdx sizeReg = Interp_ImmUInt(proc, Interp_Int64, size);
        Interp_MemCpy(proc, dst.reg, src.reg, sizeReg, align, false);
    }
    else
    {
        auto srcRVal = Interp_GetRVal(proc, src, srcType);
        Interp_Store(proc, Interp_ConvertType(type), dst.reg, srcRVal, align, false);
    }
    
    return dst;
}

Array<Interp_Val> Interp_ConvertCall(Interp_Proc* proc, Ast_FuncCall* call, Arena* allocTo)
{
    ScratchArena scratch1(allocTo);
    ScratchArena scratch2(scratch1, allocTo);
    Array<Interp_Val> rets = { 0, 0 };
    Array<RegIdx> args = { 0, 0 };
    
    // Generate target
    auto target = Interp_ConvertNode(proc, call->target);
    Assert(target.isLValue);
    auto targetReg = target.reg;
    
    auto procDecl = Ast_GetDeclCallTarget(call);
    rets.Resize(scratch1, procDecl->retTypes.length);
    
    auto retDebugType = Tc_ConvertToDebugType(proc->module, procDecl->retTypes.last());
    TB_PassingRule retRule = tb_get_passing_rule_from_dbg(proc->module, retDebugType, false);
    
    // Arguments related to return values first (last one is first, the rest are in order)
    if(retRule == TB_PASSING_INDIRECT)
    {
        uint64 size = GetTypeSize(procDecl->retTypes.last());
        uint64 align = GetTypeAlign(procDecl->retTypes.last());
        auto local = Interp_Local(proc, size, align);
        args.Append(scratch2, local);
    }
    
    for(int i = 0; i < rets.length - 1; ++i)
    {
        // The rest of the returns are all indirect
        uint64 size = GetTypeSize(procDecl->retTypes[i]);
        uint64 align = GetTypeAlign(procDecl->retTypes[i]);
        auto local = Interp_Local(proc, size, align);
        args.Append(scratch2, local);
    }
    
    for_array(i, call->args)
    {
        // Get passing rule
        Assert(call->target->type->typeId == Typeid_Proc);
        auto debugType = Tc_ConvertToDebugType(proc->module, procDecl->args[i]->type);
        TB_PassingRule rule = tb_get_passing_rule_from_dbg(proc->module, debugType, false);
        
        auto val = Interp_ConvertNode(proc, call->args[i]);
        auto passedArg = Interp_PassArg(proc, val, call->args[i]->type, rule, scratch1);
        args.Append(scratch2, passedArg);
    }
    
    RegIdx singleRet = Interp_Call(proc, target.reg, args);
    
    // Handle returns
    Array<TB_PassingRule> retRules;
    retRules.Resize(scratch1, rets.length);
    
    // They're all indirect returns
    if(rets.length == 0)
    {
        rets = { 0, 0 };
        return rets;
    }
    
    for(int i = 0; i < rets.length - 1; ++i)
    {
        auto retReg = (retRule == TB_PASSING_INDIRECT) + i;
        rets[i] = Interp_GetRet(proc, TB_PASSING_INDIRECT, retReg, procDecl->retTypes[i]);
    }
    
    rets[rets.length-1] = Interp_GetRet(proc, retRule, singleRet, procDecl->retTypes.last());
    return rets;
}

Interp_Proc* Interp_ConvertProc(Interp* interp, Ast_ProcDef* astProc)
{
    auto astDecl = Ast_GetDeclProc(astProc);
    
    auto proc    = Interp_MakeProc(interp);
    auto symbol  = Interp_MakeSymbol(interp);
    symbol->decl = astProc;
    symbol->name = astProc->name->string;
    symbol->type = astProc->type;
    
    astProc->symbol = symbol;
    proc->symbol = symbol;
    
    proc->argRules.Resize(astDecl->args.length);
    // NOTE: We also have all returns except for the last one. Those are just pointers.
    for_array(i, proc->argRules)
    {
        auto debugType = Tc_ConvertToDebugType(interp->module, astDecl->args[i]->type);
        proc->argRules[i] = tb_get_passing_rule_from_dbg(interp->module, debugType, false);
    }
    
    // NOTE: We're simply picking the last return value as an actual return.
    // All the other returns are passed by pointer. This way it's possible
    // to call Ryu procedures from C functions, if following this standard.
    if(astDecl->retTypes.length == 0)
        proc->retRule = TB_PASSING_IGNORE;
    else
    {
        auto debugRetType = Tc_ConvertToDebugType(interp->module, astDecl->retTypes[astDecl->retTypes.length-1]);
        proc->retRule = tb_get_passing_rule_from_dbg(interp->module, debugRetType, true);
    }
    
    // Reserve registers for proc arguments (including those needed for ABI)
    int argsCount = astDecl->args.length;
    // Args, the last return only if it's indirect, and the rest of the returns
    int retArgs = (astDecl->retTypes.length - 1) + (proc->retRule == TB_PASSING_INDIRECT);
    int abiArgsCount = argsCount + retArgs;
    for(int i = 0; i < abiArgsCount; ++i)
        proc->permanentRegs.Append(i);
    
    proc->regCounter = abiArgsCount;
    
    // Insert local for each declaration (including
    // procedure arguments)
    proc->declToAddr.Resize(astProc->block.decls.length);
    for_array(i, astProc->block.decls)
    {
        bool isArg = i < argsCount;
        auto decl = astProc->block.decls[i];
        if(decl->kind == AstKind_VarDecl)
        {
            if(!isArg || proc->argRules[i] == TB_PASSING_DIRECT)
            {
                auto varDecl = (Ast_VarDecl*)decl;
                auto addr = Interp_Local(proc, GetTypeSize(varDecl->type), GetTypeAlign(varDecl->type), isArg);
                
                proc->declToAddr[i] = addr;
            }
            else if(isArg && proc->argRules[i] == TB_PASSING_INDIRECT)
            {
                // The register itself contains the address needed
                // (the stack allocation is performed by the caller)
                proc->declToAddr[i] = i;
            }
        }
    }
    
    // Save the procedure arguments into stack variables, this
    // depends on the passing rule
    // NOTE: Assuming first n declarations are procedure arguments
    for_array(i, astDecl->args)
    {
        auto argType = astDecl->args[i]->type;
        switch(proc->argRules[i])
        {
            default: Assert(false && "TB added new passing rule and it's not handled");
            case TB_PASSING_IGNORE: break;
            case TB_PASSING_DIRECT:
            {
                Interp_Store(proc, Interp_ConvertType(argType),
                             proc->declToAddr[i], retArgs + i,
                             GetTypeAlign(argType), false, true);
                
                break;
            }
            case TB_PASSING_INDIRECT: break;
        }
    }
    
    // Block
    Interp_ConvertBlock(proc, &astProc->block);
    
    // Fill in these helper arrays for codegen
    if(proc->retRule == TB_PASSING_INDIRECT)
        proc->argTypes.Append(Interp_Ptr);
    
    for(int i = 0; i < astDecl->args.length - 1; ++i)
        proc->argTypes.Append(Interp_Ptr);
    
    for_array(i, astDecl->args)
    {
        Interp_Type dt;
        if(proc->argRules[i] == TB_PASSING_INDIRECT)
            proc->argTypes.Append(Interp_Ptr);
        else
            proc->argTypes.Append(Interp_ConvertType(astDecl->args[i]->type));
        
    }
    
    if(proc->retRule == TB_PASSING_INDIRECT)
        proc->retType = Interp_Ptr;
    else
        proc->retType = Interp_ConvertType(astDecl->retTypes.last());
    
    return proc;
}

void Interp_ConvertStruct(Interp_Proc* proc, Ast_StructDef* astStruct)
{
    
}

void Interp_ConvertBlock(Interp_Proc* proc, Ast_Block* block)
{
    for_array(i, block->stmts)
    {
        Interp_ConvertNode(proc, block->stmts[i]);
        // No register can persist between statements,
        // by definition.
        Interp_EndOfExpression(proc);
    }
    
    // Handle scope stuff here
    
}

Interp_Type Interp_ConvertType(TypeInfo* type)
{
    switch(type->typeId)
    {
        case Typeid_Bool:   return Interp_Bool;
        case Typeid_Char:
        case Typeid_Uint8:
        case Typeid_Int8:   return Interp_Int8;
        case Typeid_Uint16:
        case Typeid_Int16:  return Interp_Int16;
        case Typeid_Uint32:
        case Typeid_Int32:  return Interp_Int32;
        case Typeid_Uint64:
        case Typeid_Int64:  return Interp_Int64;
        case Typeid_Float:  return Interp_F32;
        case Typeid_Double: return Interp_F64;
        case Typeid_Ptr:    return Interp_Ptr;
        case Typeid_Arr:    return Interp_Ptr;
        case Typeid_None:   return Interp_Void;
        case Typeid_Proc:
        // NOTE(Leo): If it's a struct, and we'd like to put
        // it in register, we use an integer type
        case Typeid_Struct:
        case Typeid_Ident:  return { InterpType_Int, 0, uint16(GetTypeSize(type) * 8) };
    }
    
    return { 0, 0, 0 };
}

RegIdx Interp_ConvertTypeConversion(Interp_Proc* proc, RegIdx src, Interp_Type srcType, TypeInfo* srcFullType, TypeInfo* dst)
{
    Interp_Type dstType = Interp_ConvertType(dst);
    bool srcIsSigned = IsTypeSigned(srcFullType);
    bool dstIsSigned = IsTypeSigned(dst);
    return Interp_ConvertTypeConversion(proc, src, srcType, dstType, srcIsSigned, dstIsSigned);
}

RegIdx Interp_ConvertTypeConversion(Interp_Proc* proc, RegIdx src, Interp_Type srcType, Interp_Type dstType,
                                    bool srcIsSigned, bool dstIsSigned)
{
    RegIdx res = src;
    if(srcType.type == dstType.type)
    {
        if(srcType.data != dstType.data)
        {
            if(srcType.width > dstType.width)  // Truncate
                res = Interp_Trunc(proc, src, dstType);
            else  // Extend
            {
                if(srcType.type == InterpType_Float)
                    res = Interp_FPExt(proc, src, dstType);
                else if(srcType.type == InterpType_Int)
                {
                    if(srcIsSigned)
                        res = Interp_SExt(proc, src, dstType);
                    else
                        res = Interp_ZExt(proc, src, dstType);
                }
            }
        }
    }
    else if(srcType.type == InterpType_Int)
    {
        if(dstType.type == InterpType_Float)
            res = Interp_Int2Float(proc, src, dstType, srcIsSigned);
        else if(dstType.type == InterpType_Ptr)
            res = Interp_Int2Ptr(proc, src, dstType);
    }
    else if(srcType.type == InterpType_Float)
    {
        if(dstType.type == InterpType_Int)
            res = Interp_Float2Int(proc, src, dstType, dstIsSigned);
    }
    else if(srcType.type == InterpType_Ptr)
    {
        if(dstType.type == InterpType_Int)
            res = Interp_Ptr2Int(proc, src, dstType);
    }
    
    return res;
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

void Interp_PrintInstr(Interp_Proc* proc, Interp_Instr* instr)
{
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
            printf("MemberAccess ");
            break;
        }
        case Op_ArrayAccess:
        {
            printf("ArrayAccess ");
            break;
        }
        case Op_Truncate: Interp_PrintUnary(instr); break;
        case Op_FloatExt: Interp_PrintUnary(instr); break;
        case Op_SignExt: Interp_PrintUnary(instr); break;
        case Op_ZeroExt: Interp_PrintUnary(instr); break;
        case Op_Int2Ptr: Interp_PrintUnary(instr); break;
        case Op_Uint2Float: Interp_PrintUnary(instr); break;
        case Op_Float2Uint: Interp_PrintUnary(instr); break;
        case Op_Int2Float: Interp_PrintUnary(instr); break;
        case Op_Float2Int: Interp_PrintUnary(instr); break;
        case Op_Bitcast: Interp_PrintUnary(instr); break;
        case Op_Select: Interp_PrintBin(instr); break;
        case Op_Not: Interp_PrintUnary(instr); break;
        case Op_Negate: Interp_PrintUnary(instr); break;
        case Op_And: Interp_PrintBin(instr); break;
        case Op_Or: Interp_PrintBin(instr); break;
        case Op_Xor: Interp_PrintBin(instr); break;
        case Op_Add: Interp_PrintBin(instr); break;
        case Op_Sub: Interp_PrintBin(instr); break;
        case Op_Mul: Interp_PrintBin(instr); break;
        case Op_ShL: Interp_PrintBin(instr); break;
        case Op_ShR: Interp_PrintBin(instr); break;
        case Op_Sar: Interp_PrintBin(instr); break;
        case Op_Rol: Interp_PrintBin(instr); break;
        case Op_Ror: Interp_PrintBin(instr); break;
        case Op_UDiv: Interp_PrintBin(instr); break;
        case Op_SDiv: Interp_PrintBin(instr); break;
        case Op_UMod: Interp_PrintBin(instr); break;
        case Op_SMod: Interp_PrintBin(instr); break;
        case Op_FAdd: Interp_PrintBin(instr); break;
        case Op_FSub: Interp_PrintBin(instr); break;
        case Op_FMul: Interp_PrintBin(instr); break;
        case Op_FDiv: Interp_PrintBin(instr); break;
        case Op_CmpEq: Interp_PrintBin(instr); break;
        case Op_CmpNe: Interp_PrintBin(instr); break;
        case Op_CmpULT: Interp_PrintBin(instr); break;
        case Op_CmpULE: Interp_PrintBin(instr); break;
        case Op_CmpSLT: Interp_PrintBin(instr); break;
        case Op_CmpSLE: Interp_PrintBin(instr); break;
        case Op_CmpFLT: Interp_PrintBin(instr); break;
        case Op_CmpFLE: Interp_PrintBin(instr); break;
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

void Interp_ExecProc(Interp_Proc* proc)
{
    
}
