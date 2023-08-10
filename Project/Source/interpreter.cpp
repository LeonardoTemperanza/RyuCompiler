
#include "base.h"
#include "interpreter.h"

////
// Bytecode generation helper functions
////

Interp_Proc* Interp_MakeProc(Interp* interp)
{
    interp->procs.ResizeAndInit(interp->procs.length + 1);
    auto& newElement = interp->procs[interp->procs.length - 1];
    newElement.instrArrays = { 0, 0 };
    newElement.regArrays = { 0, 0 };
    newElement.instrs = { 0, 0, 0 };
    newElement.locals = { 0, 0, 0 };
    return &newElement;
}

uint32 Interp_AllocateRegIdxArray(Interp_Proc* proc, RegIdx* values, uint16 arrayCount)
{
    uint32 oldLength = (uint32)proc->instrArrays.length;
    proc->regArrays.Resize(proc->regArrays.length + arrayCount);
    
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
    proc->instrArrays.Resize(proc->instrArrays.length + arrayCount);
    
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

InstrIdx Interp_Region(Interp_Proc* proc)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Region;
    return proc->instrs.length - 1;
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

void Interp_Ptr2Int(Interp_Proc* proc)
{
    
}

void Interp_Int2Float(Interp_Proc* proc)
{
    
}

void Interp_Float2Int(Interp_Proc* proc)
{
    
}

void Interp_Bitcast(Interp_Proc* proc)
{
    
}

// Local uses reg1 and reg2 to store size and alignment,
// Dest is the resulting address
RegIdx Interp_Local(Interp_Proc* proc, uint64 size, uint64 align)
{
    proc->locals.Resize(proc->locals.length + 1);
    auto& newElement = proc->locals[proc->locals.length-1];
    
    newElement.op          = Op_Local;
    newElement.local.dst   = proc->regCounter;
    newElement.local.size  = size;
    newElement.local.align = align;
    
    return proc->regCounter++;
}

// Ignoring atomics for now
RegIdx Interp_Load(Interp_Proc* proc, Interp_Type type, RegIdx addr, uint64 align, bool isVolatile)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op         = Op_Load;
    newElement.load.dst   = proc->regCounter;
    newElement.load.addr  = addr;
    newElement.load.align = align;
    newElement.load.type  = type;
    
    return proc->regCounter++;
}

void Interp_Store(Interp_Proc* proc, Interp_Type type, RegIdx addr, RegIdx val, uint64 align, bool isVolatile)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op          = Op_Store;
    newElement.store.addr  = addr;
    newElement.store.align = align;
}

void Interp_ImmBool(Interp_Proc* proc) {}

RegIdx Interp_ImmSInt(Interp_Proc* proc, Interp_Type type, int64 imm)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op       = Op_IntegerConst;
    newElement.imm.dst  = proc->regCounter;
    newElement.imm.val  = imm;
    newElement.imm.type = type;
    return proc->regCounter++;
}

void Interp_ImmUInt(Interp_Proc* proc, Interp_Type type, uint64 imm)
{
    // Zero out all bits which are not supposedly used
    uint64 mask = ~0ULL >> (type.data);
    imm &= mask;
    
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op       = Op_IntegerConst;
    newElement.imm.dst  = proc->regCounter++;
    newElement.imm.val  = imm;
    newElement.imm.type = type;
}

void Interp_Float32(Interp_Proc* proc) {}

void Interp_Float64(Interp_Proc* proc) {}

// void Interp_CString(Interp_Proc* proc) {}

// void Interp_String(Interp_Proc* proc) {}

void Interp_MemSet(Interp_Proc* proc) {}

void Interp_MemZero(Interp_Proc* proc) {}

void Interp_ArrayAccess(Interp_Proc* proc) {}

void Interp_MemberAccess(Interp_Proc* proc) {}

RegIdx Interp_GetSymbolAddress(Interp_Proc* proc, Interp_Symbol* symbol)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_GetSymbolAddress;
    newElement.symAddress.dst    = proc->regCounter;
    newElement.symAddress.symbol = symbol;
    return proc->regCounter++;
}

void Interp_Select(Interp_Proc* proc)
{
    
}

// @incomplete
// This should support different kinds of arithmetic behavior 
RegIdx Interp_Add(Interp_Proc* proc, RegIdx reg1, RegIdx reg2)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op       = Op_Add;
    newElement.bin.dst  = proc->regCounter;
    newElement.bin.src1 = reg1;
    newElement.bin.src2 = reg2;
    return proc->regCounter++;
}

RegIdx Interp_Sub(Interp_Proc* proc, RegIdx reg1, RegIdx reg2)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op       = Op_Sub;
    newElement.bin.dst  = proc->regCounter;
    newElement.bin.src1 = reg1;
    newElement.bin.src2 = reg2;
    return proc->regCounter++;
}

RegIdx Interp_Mul(Interp_Proc* proc, RegIdx reg1, RegIdx reg2)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op       = Op_Mul;
    newElement.bin.dst  = proc->regCounter;
    newElement.bin.src1 = reg1;
    newElement.bin.src2 = reg2;
    return proc->regCounter++;
}

RegIdx Interp_Div(Interp_Proc* proc, RegIdx reg1, RegIdx reg2, bool signedness)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op       = signedness ? Op_SDiv : Op_UDiv;
    newElement.bin.dst  = proc->regCounter;
    newElement.bin.src1 = reg1;
    newElement.bin.src2 = reg2;
    return proc->regCounter++;
}

RegIdx Interp_Mod(Interp_Proc* proc, RegIdx reg1, RegIdx reg2, bool signedness)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op       = signedness ? Op_SMod : Op_UMod;
    newElement.bin.dst  = proc->regCounter;
    newElement.bin.src1 = reg1;
    newElement.bin.src2 = reg2;
    return proc->regCounter++;
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

// Do I need the prototype here?
Interp_RegInterval Interp_Call(Interp_Proc* proc, uint32 numRets, RegIdx target, Array<RegIdx> args)
{
    Interp_RegInterval result = { 0, 0 };
    result.minReg = proc->regCounter;
    proc->regCounter += numRets - 1;
    result.maxReg = proc->regCounter;
    
    uint32 argIdx = Interp_AllocateRegIdxArray(proc, args);
    
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Call;
    newElement.call.target = target;
    newElement.call.minRetReg = result.minReg;
    newElement.call.maxRetReg = result.maxReg;
    newElement.call.argArrayCount = args.length;
    newElement.call.argArrayStart = argIdx;
    
    ++proc->regCounter;
    return result;
}

void Interp_Safepoint(Interp_Proc* proc) {}

Interp_Instr* Interp_Goto(Interp_Proc* proc, InstrIdx target)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Branch;
    newElement.branch.defaultCase = target;
    newElement.branch.keyArrayStart = 0;
    newElement.branch.keyArrayCount = 0;
    newElement.branch.caseArrayStart = 0;
    newElement.branch.caseArrayCount = 0;
    return &newElement;
}

Interp_Instr* Interp_Goto(Interp_Proc* proc)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Branch;
    newElement.branch.defaultCase = 0;
    newElement.branch.keyArrayStart = 0;
    newElement.branch.keyArrayCount = 0;
    newElement.branch.caseArrayStart = 0;
    newElement.branch.caseArrayCount = 0;
    return &newElement;
}

Interp_Instr* Interp_If(Interp_Proc* proc, RegIdx value, InstrIdx ifInstr, InstrIdx elseInstr)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    uint32 instrIdx = Interp_AllocateInstrIdxArray(proc, &ifInstr, 1);
    uint32 regIdx = Interp_AllocateRegIdxArray(proc, &value, 1);
    
    newElement.op = Op_Branch;
    newElement.branch.defaultCase = elseInstr;
    newElement.branch.keyArrayStart = value;
    newElement.branch.keyArrayCount = 1;
    newElement.branch.caseArrayStart = regIdx;
    newElement.branch.caseArrayCount = 1;
    return &newElement;
}

// Patch if and else instructions index later
Interp_Instr* Interp_If(Interp_Proc* proc, RegIdx value)
{
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Branch;
    newElement.branch.defaultCase = 0;
    newElement.branch.keyArrayStart = 0;
    newElement.branch.keyArrayCount = 0;
    newElement.branch.caseArrayStart = 0;
    newElement.branch.caseArrayCount = 0;
    return &newElement;
}

void Interp_Branch(Interp_Proc* proc)
{
    
}

void Interp_Return(Interp_Proc* proc, RegIdx* retValues, uint16 count)
{
    uint32 arrayStart = Interp_AllocateRegIdxArray(proc, retValues, count);
    
    proc->instrs.Resize(proc->instrs.length + 1);
    auto& newElement = proc->instrs[proc->instrs.length-1];
    
    newElement.op = Op_Ret;
    newElement.ret.valArrayStart = arrayStart;
    newElement.ret.valArrayCount = count;
}

////
// AST -> bytecode conversion
////

// At this point the registers can be reused
void Interp_EndOfExpression(Interp_Proc* proc)
{
    proc->regCounter = 0;
}

// @temporary @temporary
bool needValue = false;

RegIdx Interp_ConvertNode(Interp_Proc* proc, Ast_Node* node)
{
    RegIdx result = 0;
    
    switch(node->kind)
    {
        case AstKind_ProcDef:       break;  // Not handled here
        case AstKind_StructDef:     break;  // Not handled here
        case AstKind_DeclBegin:     break;
        case AstKind_VarDecl:
        {
            auto decl = (Ast_VarDecl*)node;
            RegIdx reg = Interp_Local(proc, GetTypeSize(decl->type), GetTypeAlign(decl->type));
            //decl->tildeNode = node;
            decl->interpReg = reg;
            
            if(decl->initExpr)
            {
                auto exprReg = Interp_ConvertNode(proc, decl->initExpr);
                Interp_EndOfExpression(proc);
                Interp_Type interpType = { 0, 0, 0 };  // @incomplete
                Interp_Store(proc, interpType, reg, exprReg, 0, false);
            }
            
            result = reg;
            break;
        }
        case AstKind_EnumDecl:      break;
        case AstKind_DeclEnd:       break;
        case AstKind_StmtBegin:     break;
        case AstKind_Block:         Interp_ConvertBlock(proc, (Ast_Block*)node); break;
        case AstKind_If:
        {
            auto stmt = (Ast_If*)node;
            RegIdx cond = Interp_ConvertNode(proc, stmt->condition);
            Interp_EndOfExpression(proc);
            
            auto ifInstr = Interp_If(proc, cond);
            auto ifRegionIdx = Interp_Region(proc);
            
            uint32 regArrayStart = Interp_AllocateRegIdxArray(proc, &cond, 1);
            uint32 instrArrayStart = Interp_AllocateInstrIdxArray(proc, &ifRegionIdx, 1);
            
            ifInstr->branch.keyArrayStart = regArrayStart;
            ifInstr->branch.caseArrayStart = instrArrayStart;
            
            Interp_ConvertBlock(proc, stmt->thenBlock);
            auto gotoInstr = Interp_Goto(proc);
            
            auto elseRegionIdx = Interp_Region(proc);
            gotoInstr->branch.defaultCase = elseRegionIdx;
            
            if(stmt->elseStmt)
            {
                Interp_ConvertNode(proc, stmt->elseStmt);
                auto elseGotoInstr = Interp_Goto(proc);
                auto endRegionIdx = Interp_Region(proc);
                elseGotoInstr->branch.defaultCase = endRegionIdx;
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
            auto exprReg = Interp_ConvertNode(proc, stmt->expr);
            Interp_Return(proc, &exprReg, 1);
            break;
        }
        case AstKind_Break:         break;
        case AstKind_Continue:      break;
        case AstKind_EmptyStmt:     break;
        case AstKind_StmtEnd:       break;
        case AstKind_ExprBegin:     break;
        case AstKind_Literal:       break;
        case AstKind_NumLiteral:
        {
            auto expr = (Ast_Expr*)node;
            Interp_Type type = Interp_ConvertType(expr->type);
            // @temporary All literals are 2 for now
            result = Interp_ImmSInt(proc, type, 2);
            break;
        }
        case AstKind_Ident:
        {
            auto expr = (Ast_IdentExpr*)node;
            
            if(expr->declaration->kind == AstKind_ProcDef)  // Procedure
            {
                auto procDef = (Ast_ProcDef*)expr->declaration;
                auto address = Interp_GetSymbolAddress(proc, procDef->symbol);
                
                result = address;
            }
            else  // Local variable
            {
                if(needValue) result = Interp_Load(proc, Interp_ConvertType(expr->type), expr->declaration->interpReg, 4, false);
                else          result = expr->declaration->interpReg;
            }
            
            break;
        }
        case AstKind_FuncCall:
        {
            auto call = (Ast_FuncCall*)node;
            auto targetReg = Interp_ConvertNode(proc, call->target);
            
            ScratchArena scratch;
            Array<RegIdx> argRegs = { 0, 0 };
            
            for_array(i, call->args)
            {
                auto reg = Interp_ConvertNode(proc, call->args[i]);
                argRegs.Append(scratch, reg);
            }
            
            auto procDecl = Ast_GetDeclCallTarget(call);
            
            auto regs = Interp_Call(proc, procDecl->retTypes.length, targetReg, argRegs);
            // Only 1 element works for now
            result = regs.minReg;
            
            break;
        }
        case AstKind_BinaryExpr:
        {
            auto expr = (Ast_BinaryExpr*)node;
            needValue = false;
            RegIdx lhs = Interp_ConvertNode(proc, expr->lhs);
            needValue = true;
            RegIdx rhs = Interp_ConvertNode(proc, expr->rhs);
            
            //Assert(lhs && rhs);
            
            // @temporary assuming integer operands
            RegIdx result = 0;
            switch(expr->op)
            {
                case '=':
                {
                    Interp_Store(proc, Interp_ConvertType(expr->lhs->type), lhs, rhs, GetTypeAlign(expr->lhs->type), false);
                    result = lhs;
                    break;
                }
                case '+': result = Interp_Add(proc, lhs, rhs); break;
                case '-': result = Interp_Sub(proc, lhs, rhs); break;
            }
            
            break;
        }
        case AstKind_UnaryExpr:     break;
        case AstKind_TernaryExpr:   break;
        case AstKind_Typecast:      break;
        case AstKind_Subscript:     break;
        case AstKind_MemberAccess:  break;
        case AstKind_ExprEnd:       break;
        default: Assert(false && "Enum value out of bounds");
    }
    
    return result;
}

Interp_Proc* Interp_ConvertProc(Interp* interp, Ast_ProcDef* astProc)
{
    printf("Converting proc...\n");
    
    auto proc = Interp_MakeProc(interp);
    
    // Store some info about the procedure, argument types perhaps, etc.
    
    // Block
    Interp_ConvertBlock(proc, &astProc->block);
    
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
    Interp_Type dt = { 0, 0, 0 };
    
    if(IsTypeIntegral(type) || type->typeId == Typeid_None)
    {
        dt.type = TB_INT;
    }
    else if(IsTypeFloat(type))
    {
        dt.type = TB_FLOAT;
    }
    else if(type->typeId == Typeid_Ptr)
    {
        dt.type = TB_PTR;
    }
    else if(type->typeId == Typeid_Proc)
    {
        dt.type = TB_PTR;
    }
    else
        Assert(false);
    
    if(type->typeId == Typeid_Bool ||
       type->typeId == Typeid_Char ||
       type->typeId == Typeid_Uint8 ||
       type->typeId == Typeid_Int8)
    {
        dt.width = 1;
        dt.data = 8;
    }
    else if(type->typeId == Typeid_Uint16 ||
            type->typeId == Typeid_Int16)
    {
        dt.width = 2;
        dt.data = 16;
    }
    else if(type->typeId == Typeid_Uint32 ||
            type->typeId == Typeid_Int32 ||
            type->typeId == Typeid_Float)
    {
        dt.width = 4;
        dt.data = 32;
    }
    else if(type->typeId == Typeid_Uint64 ||
            type->typeId == Typeid_Int64 ||
            type->typeId == Typeid_Double ||
            type->typeId == Typeid_Ptr ||
            type->typeId == Typeid_Proc)
    {
        dt.width = 8;
        dt.data = 64;
    }
    else if(type->typeId == Typeid_None)
    {
        dt.width = 1;
        dt.data = 0;
    }
    else Assert(false);
    
    if(type->typeId == Typeid_Bool)
        dt.data = 1;
    
    return dt;
}

void Interp_PrintProc(Interp_Proc* proc)
{
    // Print proc name, etc.
    printf("Procedure:\n");
    
    int counter = 0;
    const int numSpaces = 5;
    
    for_array(i, proc->locals)
    {
        int numChars = printf("%d:", counter++);
        for(int i = numChars; i < numSpaces; ++i)
            printf(" ");
        
        Interp_PrintInstr(proc, &proc->locals[i]);
    }
    
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
    switch(instr->op)
    {
        case Op_Null: printf("Null"); break;
        case Op_IntegerConst:
        {
            printf("IntConst %d: %llu", instr->imm.dst, instr->imm.val);
            break;
        }
        case Op_Float32Const:
        {
            printf("Float32Const ");
            break;
        }
        case Op_Float64Const:
        {
            printf("Float64Const ");
            break;
        }
        case Op_Start: break;
        case Op_Region:
        {
            printf("Region ");
            break;
        }
        case Op_Call:
        {
            printf("Call %d (", instr->call.target);
            
            uint32 start = instr->call.argArrayStart;
            uint32 end = start + instr->call.argArrayCount;
            for(int i = start; i < end; ++i)
            {
                printf("%d", proc->regArrays[i]);
                
                if(i < end-1)
                    printf(", ");
            }
            
            if(instr->call.minRetReg == instr->call.maxRetReg)
                printf(") -> %d", instr->call.minRetReg);
            else
                printf(") -> [%d, %d]", instr->call.minRetReg, instr->call.maxRetReg);
            
            break;
        }
        case Op_SysCall:
        {
            printf("SysCall ");
            break;
        }
        case Op_Store:
        {
            printf("Store %d: ", instr->store.addr);
            printf("%d %I64u", instr->store.val, instr->store.align);
            break;
        }
        case Op_MemCpy:
        {
            printf("MemCpy ");
            break;
        }
        case Op_MemSet:
        {
            printf("MemSet ");
            break;
        }
        case Op_AtomicTestAndSet:
        {
            printf("AtomicTestAndSet ");
            break;
        }
        case Op_AtomicClear:
        {
            printf("AtomicClear ");
            break;
        }
        case Op_AtomicLoad:
        {
            printf("AtomicLoad ");
            break;
        }
        case Op_AtomicExchange:
        {
            printf("AtomicExchange ");
            break;
        }
        case Op_AtomicAdd:
        {
            printf("AtomicAdd ");
            break;
        }
        case Op_AtomicSub:
        {
            printf("AtomicSub ");
            break;
        }
        case Op_AtomicAnd:
        {
            printf("AtomicAnd ");
            break;
        }
        case Op_AtomicXor:
        {
            printf("AtomicXor ");
            break;
        }
        case Op_AtomicOr:
        {
            printf("AtomicOr ");
            break;
        }
        case Op_AtomicCompareExchange: break;
        case Op_DebugBreak: break;
        case Op_Branch:
        {
            if(instr->branch.keyArrayCount == 0)
            {
                printf("Goto %d", instr->branch.defaultCase);
            }
            else
            {
                printf("Branch (");
                auto start = instr->branch.keyArrayStart;
                auto end = start + instr->branch.keyArrayCount;
                for(int i = start; i < end; ++i)
                {
                    printf("%d", proc->regArrays[i]);
                    
                    if(i < end - 1)
                        printf(" ");
                }
                
                printf(") -> (");
                start = instr->branch.caseArrayStart;
                end = start + instr->branch.caseArrayCount;
                for(int i = start; i < end; ++i)
                {
                    printf("%d", proc->instrArrays[i]);
                    
                    if(i < end - 1)
                        printf(", ");
                }
                
                printf("), default: %d", instr->branch.defaultCase);
            }
            
            break;
        }
        case Op_Ret:
        {
            printf("Ret ");
            auto start = instr->ret.valArrayStart;
            auto end = start + instr->ret.valArrayCount;
            for(int i = start; i < end; ++i)
            {
                printf("%d", proc->regArrays[i]);
            }
            
            break;
        }
        case Op_Unreachable: break;
        case Op_Trap: break;
        case Op_Poison: break;
        case Op_Load:
        {
            printf("Load %d: %d %llu", instr->load.dst, instr->load.addr, instr->load.align);
            break;
        }
        case Op_Local:
        {
            printf("Local %d: ", instr->local.dst);
            printf("%d, %d", instr->local.size, instr->local.align);
            break;
        }
        case Op_GetSymbolAddress:
        {
            printf("GetSymbolAddress ");
            break;
        }
        case Op_MemberAccess: break;
        case Op_ArrayAccess: break;
        case Op_Truncate: break;
        case Op_FloatExt: break;
        case Op_SignExt: break;
        case Op_ZeroExt: break;
        case Op_Int2Ptr: break;
        case Op_Uint2Float: break;
        case Op_Float2Uint: break;
        case Op_Int2Float: break;
        case Op_Float2Int: break;
        case Op_Bitcast: break;
        case Op_Select: break;
        case Op_BitSwap: break;
        case Op_Clz: break;
        case Op_Ctz: break;
        case Op_PopCnt: break;
        case Op_Not: break;
        case Op_Negate: break;
        case Op_And: break;
        case Op_Or: break;
        case Op_Xor: break;
        case Op_Add:
        {
            printf("Add %d: ", instr->bin.dst);
            printf("%d %d", instr->bin.src1, instr->bin.src2);
            break;
        }
        case Op_Sub:
        {
            printf("Sub %d: ", instr->bin.dst);
            printf("%d %d", instr->bin.src1, instr->bin.src2);
            break;
        }
        case Op_Mul: break;
        case Op_ShL: break;
        case Op_ShR: break;
        case Op_Sar: break;
        case Op_Rol: break;
        case Op_Ror: break;
        case Op_UDiv: break;
        case Op_SDiv: break;
        case Op_UMod: break;
        case Op_SMod: break;
        case Op_FAdd: break;
        case Op_FSub: break;
        case Op_FMul: break;
        case Op_FDiv: break;
        case Op_CmpEq: break;
        case Op_CmpNe: break;
        case Op_CmpULT: break;
        case Op_CmpULE: break;
        case Op_CmpSLT: break;
        case Op_CmpSLE: break;
        case Op_CmpFLT: break;
        case Op_CmpFLE: break;
        default: printf("Unknown operation");
    }
    
    printf("\n");
}

void Interp_ExecProc(Interp_Proc* proc)
{
    
}
