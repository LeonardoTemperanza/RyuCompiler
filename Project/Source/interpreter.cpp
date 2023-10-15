
// @refactor @cleanup ABI-related stuff is very messy and should be refactored.

#include "base.h"
#include "interpreter.h"
#include "bytecode_builder.h"

bool GenBytecode(Interp* interp, Ast_Node* node)
{
    ProfileFunc(prof);
    
    if(node->kind == AstKind_ProcDecl)
    {
        auto decl = (Ast_ProcDecl*)node;
        
        auto symbol  = Interp_MakeSymbol(interp);
        symbol->type = decl->declSpecs & Decl_Extern ? Interp_ExternSym : Interp_ProcSym;
        symbol->decl = decl;
        symbol->name = decl->name->s;
        symbol->typeInfo = decl->type;
        decl->symIdx = interp->symbols.length - 1;
        return true;
    }
    else if(node->kind == AstKind_ProcDef)
    {
        auto astProc = (Ast_ProcDef*)node;
        bool yielded = false;
        auto proc = Interp_ConvertProc(interp, astProc, &yielded);
        
        if(yielded) return false;
        
        if(cmdLineArgs.emitBytecode)
            Interp_PrintProc(proc, interp->symbols);
        
        return true;
    }
    else if(node->kind == AstKind_VarDecl)
    {
        // TODO: Initialization is missing
        auto decl = (Ast_VarDecl*)node;
        Interp_Symbol* sym = Interp_MakeSymbol(interp);
        sym->type = Interp_GlobalSym;
        sym->decl = decl;
        sym->name = decl->name->s;
        sym->typeInfo = decl->type;
        decl->symIdx = interp->symbols.length - 1;
        
        return true;
    }
    
    return true;
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

RegIdx Interp_BuildPreRet(Interp_Builder* builder, RegIdx arg, Interp_Val val, TypeInfo* type, TB_PassingRule rule)
{
    RegIdx res = 0;
    switch(rule)
    {
        default: Assert(false && "TB added new passing rule which is not handled");
        case TB_PASSING_IGNORE: break;
        case TB_PASSING_DIRECT:
        {
            if(val.type == Interp_LValue)
            {
                // TODO: This is an "Implicit array to pointer" conversion, test this
                if(type->typeId == Typeid_Arr)
                    res = val.reg;
                else
                {
                    Assert(type->size <= 8);
                    Interp_Type dt = Interp_ConvertType(type);
                    res = Interp_Load(builder, dt, val.reg, type->align, false);
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
            RegIdx sizeReg = Interp_ImmUInt(builder, Interp_Int64, type->size);
            Interp_MemCpy(builder, dstReg, val.reg, sizeReg, type->align, false);
            res = arg;  // Return the address
            break;
        }
    }
    
    return res;
}

// TODO: This will, in the future, return an array of registers
// because SystemV can split a single struct into multiple registers.
RegIdx Interp_PassArg(Interp_Builder* builder, Interp_Val arg, TypeInfo* type, TB_PassingRule rule, Arena* allocTo)
{
    RegIdx res = 0;
    switch(rule)
    {
        default: Assert(false);
        case TB_PASSING_INDIRECT:
        {
            // First retrieve the address
            RegIdx addr = RegIdx_Unused;
            
            if(arg.type == Interp_LValue)
                addr = arg.reg;
            else
            {
                auto dt = Interp_ConvertType(type);
                
                // This means that it was a temporary computation
                // such as 2+2, it's necessary to stack allocate a
                // new variable to hold this value
                auto local = Interp_Local(builder, type->size, type->align);
                Interp_Store(builder, dt, local, arg.reg, type->align, false);
                addr = local;
            }
            
            Assert(addr != RegIdx_Unused);
            
            // Now given the address, create a copy of it and pass it to the proc
            RegIdx local   = Interp_Local(builder, type->size, type->align);
            RegIdx sizeReg = Interp_ImmUInt(builder, Interp_Int64, type->size);
            
            Interp_MemCpy(builder, local, addr, sizeReg, type->align, false);
            
            res = local;
            break;
        }
        case TB_PASSING_DIRECT:
        {
            if(arg.type == Interp_LValue)
            {
                Assert(type->size <= 8);
                Interp_Type dt = { 0, 0, 0 };
                
                if(type->typeId == Typeid_Struct ||
                   type->typeId == Typeid_Ident)
                    dt = { InterpType_Int, 0, uint16(type->size * 8) };  // Use int type for structs
                else
                    dt = Interp_ConvertType(type);
                
                res = Interp_Load(builder, dt, arg.reg, type->align, false);
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

Interp_Val Interp_GetRet(Interp_Builder* builder, TB_PassingRule rule, RegIdx retReg, TypeInfo* retType)
{
    Interp_Val res;
    
    // If it's indirect, just retrieve the address
    if (rule == TB_PASSING_INDIRECT)
    {
        res.type = Interp_LValue;
        res.reg  = retReg;
        return res;
    }
    
    // Structs can only be stack variables for
    // bytecode generation purposes, so we need to
    // create a stack variable
    if(retType->typeId == Typeid_Struct ||
       retType->typeId == Typeid_Ident)
    {
        // Spawn a temporary stack variable
        RegIdx local = Interp_Local(builder, retType->size, retType->align);
        Interp_Type dt = { InterpType_Int, 0, uint16(retType->size * 8) };
        //Interp_Store(builder, dt, local, retReg, align, false);
        res = { local, Interp_LValue };
    }
    else
    {
        res = { retReg, Interp_RValue };
    }
    
    return res;
}

RegIdx Interp_GetRVal(Interp_Builder* builder, Interp_Val val, TypeInfo* type)
{
    Assert(Interp_IsValueValid(val));
    auto proc = builder->proc;
    
    if(val.type == Interp_LValue)
    {
        val.reg = Interp_Load(builder, Interp_ConvertType(type), val.reg, type->align, false);
        val.type = Interp_RValue;
    }
    else if(val.type == Interp_RValuePhi)
    {
        // Construct value that is 0 if false, 1 if true.
        // Here, we also merge the true and false region with gotos
        Interp_Instr trueInstr;
        trueInstr.op         = Op_IntegerConst;
        trueInstr.bitfield  |= InstrBF_ViolatesSSA;
        trueInstr.dst        = builder->regCounter;
        trueInstr.imm.type   = Interp_Bool;
        trueInstr.imm.intVal = 1;
        
        Interp_Instr falseInstr;
        falseInstr.op         = Op_IntegerConst;
        falseInstr.bitfield  |= InstrBF_ViolatesSSA;
        falseInstr.dst        = builder->regCounter;
        falseInstr.imm.type   = Interp_Bool;
        falseInstr.imm.intVal = 0;
        
        val.reg = builder->regCounter;
        Interp_AdvanceReg(builder);
        
        proc->instrs[val.phi.trueEndInstr] = trueInstr;
        proc->instrs[val.phi.falseEndInstr] = falseInstr;
        
        // Insert goto instruction directed to 'merge'
        // We reserved a single instruction at the end of
        // the true and false regions, but now we need another
        // one for the jump instruction.
        Interp_Instr gotoMerge;
        gotoMerge.op                 = Op_Branch;
        gotoMerge.branch.count       = 0;
        gotoMerge.branch.keyStart    = 0;
        gotoMerge.branch.caseStart   = 0;
        gotoMerge.branch.defaultCase = val.phi.mergeRegion;
        
        // In practice this will result in very little copying
        Interp_InsertInstrAtMiddle(builder, gotoMerge, val.phi.trueEndInstr+1);
        ++gotoMerge.branch.defaultCase;
        
        if(val.phi.falseEndInstr >= val.phi.trueEndInstr+1)
            ++val.phi.falseEndInstr;
        Interp_InsertInstrAtMiddle(builder, gotoMerge, val.phi.falseEndInstr+1);
        
        ++gotoMerge.branch.defaultCase;
        proc->instrs[gotoMerge.branch.defaultCase].bitfield |= InstrBF_MergeSSA;
    }
    
    return val.reg;
}

Interp Interp_Init(DepGraph* graph)
{
    Interp interp;
    interp.graph = graph;
    
    TB_FeatureSet features = { 0 };
    TB_Arch arch = TB_ARCH_X86_64;
    TB_System sys = TB_SYSTEM_WINDOWS;
    TB_DebugFormat debugFmt = TB_DEBUGFMT_NONE;
    interp.module = tb_module_create(arch, sys, &features, false);
    
    return interp;
}

// At this point the registers can be reused
void Interp_EndOfExpression(Interp_Builder* builder)
{
    builder->regCounter = Interp_GetFirstUnused(builder);
}

RegIdx Interp_ConvertNodeRVal(Interp_Builder* builder, Ast_Node* node)
{
    Assert(Ast_IsExpr(node) || node->kind == AstKind_VarDecl);
    TypeInfo* type = node->kind == AstKind_VarDecl ? ((Ast_VarDecl*)node)->type : ((Ast_Expr*)node)->type;
    
    auto val = Interp_ConvertNode(builder, node);
    return Interp_GetRVal(builder, val, type);
}

// NOTE(Leo): Handles conversion for all nodes, and also takes care of
// type conversions if the node is an expression. Called functions
// should not handle type conversions.
Interp_Val Interp_ConvertNode(Interp_Builder* builder, Ast_Node* node)
{
    ProfileFunc(prof);
    
    Interp_Val res;
    
    switch(node->kind)
    {
        case AstKind_ProcDef:       break;  // Not handled here
        case AstKind_ProcDecl:      break;  // Not handled here
        case AstKind_StructDef:     break;  // Not handled here
        case AstKind_DeclBegin:     break;
        case AstKind_VarDecl:       res = Interp_ConvertVarDecl(builder, (Ast_VarDecl*)node); break;
        case AstKind_EnumDecl:      break;
        case AstKind_DeclEnd:       break;
        case AstKind_StmtBegin:     break;
        case AstKind_Block:         Interp_ConvertBlock(builder, (Ast_Block*)node); break;
        case AstKind_If:            Interp_ConvertIf(builder, (Ast_If*)node); break;
        case AstKind_For:           Interp_ConvertFor(builder, (Ast_For*)node); break;
        case AstKind_While:         Interp_ConvertWhile(builder, (Ast_While*)node); break;
        case AstKind_DoWhile:       Interp_ConvertDoWhile(builder, (Ast_DoWhile*)node); break;
        case AstKind_Switch:        Interp_ConvertSwitch(builder, (Ast_Switch*)node); break;
        case AstKind_Defer:         builder->deferStack.Append(((Ast_Defer*)node)->stmt); break;
        case AstKind_Return:        Interp_ConvertReturn(builder, (Ast_Return*)node); break;
        case AstKind_Break:         Interp_ConvertSimpleJump(builder, &builder->breaks); break;
        case AstKind_Continue:      Interp_ConvertSimpleJump(builder, &builder->continues); break;
        case AstKind_Fallthrough:   Interp_ConvertSimpleJump(builder, &builder->fallthroughs); break;
        case AstKind_MultiAssign:   break;
        case AstKind_EmptyStmt:     break;
        case AstKind_StmtEnd:       break;
        case AstKind_ExprBegin:     break;
        case AstKind_Ident:         res = Interp_ConvertIdent(builder, (Ast_IdentExpr*)node); break;
        case AstKind_FuncCall:
        {
            ScratchArena scratch;
            auto values = Interp_ConvertCall(builder, (Ast_FuncCall*)node, scratch);
            // In this context only 1 return is allowed
            Assert(values.length == 0 || values.length == 1);
            
            if(values.length == 1)
                res = values[0];
            break;
        }
        case AstKind_BinaryExpr:    res = Interp_ConvertBinExpr(builder, (Ast_BinaryExpr*)node); break;
        case AstKind_UnaryExpr:     res = Interp_ConvertUnaryExpr(builder, (Ast_UnaryExpr*)node); break;
        case AstKind_TernaryExpr:   res = Interp_ConvertTernaryExpr(builder, (Ast_TernaryExpr*)node); break;
        case AstKind_Typecast:
        {
            auto expr = (Ast_Typecast*)node;
            auto srcType = Interp_ConvertType(expr->expr->type);
            RegIdx exprReg = Interp_ConvertNodeRVal(builder, expr->expr);
            res.reg = Interp_ConvertTypeConversion(builder, exprReg, srcType, expr->expr->type, expr->type);
            break;
        }
        case AstKind_Subscript:     res = Interp_ConvertSubscript(builder, (Ast_Subscript*)node); break;
        case AstKind_MemberAccess:  res = Interp_ConvertMemberAccess(builder, (Ast_MemberAccess*)node); break;
        case AstKind_ConstValue:    res = Interp_ConvertConstValue(builder, (Ast_ConstValue*)node); break;
        case AstKind_RunDir:        Assert(false && "Run directive codegen not implemented"); break;
        case AstKind_ExprEnd:       break;
        default: Assert(false && "Enum value out of bounds");
    }
    
    if(Ast_IsExpr(node))
    {
        auto expr = (Ast_Expr*)node;
        
        if(expr->type->typeId != Typeid_Struct &&
           expr->type->typeId != Typeid_Ident  &&
           expr->type->typeId != Typeid_Proc)
        {
            // Convert type
            auto convType = Interp_ConvertType(expr->type);
            auto convCastType = Interp_ConvertType(expr->castType);
            if(convType != convCastType)
            {
                res.reg = Interp_GetRVal(builder, res, expr->type);
                res.type = Interp_RValue;
                
                res.reg = Interp_ConvertTypeConversion(builder, res.reg, convType, expr->type, expr->castType);
            }
        }
    }
    
    return res;
}

Interp_Val Interp_ConvertVarDecl(Interp_Builder* builder, Ast_VarDecl* stmt)
{
    auto type = stmt->type;
    
    auto dst = builder->declToAddr[stmt->declIdx];
    Interp_Val dstVal = { dst, Interp_LValue };
    
    if(stmt->initExpr)
    {
        auto src = Interp_ConvertNode(builder, stmt->initExpr);
        Interp_Assign(builder, src, stmt->initExpr->type, dstVal, stmt->type);
    }
    
    return dstVal;
}

void Interp_ConvertBlock(Interp_Builder* builder, Ast_Block* block)
{
    // Save current defer block index
    int deferIdx = builder->deferStack.length - 1;
    builder->curDeferIdx = deferIdx;
    
    for_array(i, block->stmts)
    {
        // Ignore statements
        if(builder->genJump)
            break;
        
        Interp_ConvertNode(builder, block->stmts[i]);
        // No register can persist between statements,
        // by definition.
        Interp_EndOfExpression(builder);
    }
    
    // Generate defer statements
    if(!builder->genJump)
    {
        for(int i = builder->deferStack.length - 1; i > deferIdx; --i)
        {
            Interp_ConvertNode(builder, builder->deferStack[i]);
            Interp_EndOfExpression(builder);
        }
        
        builder->deferStack.Resize(deferIdx + 1);
    }
}

// TODO: the generated bytecode could be improved as it could
// just use the true and false regions from the RValuePhi type.
// Instead it generates a phi and then jumps based on that.
// The same goes for 'for', 'while', 'do while'
void Interp_ConvertIf(Interp_Builder* builder, Ast_If* stmt)
{
    RegIdx value = Interp_ConvertNodeRVal(builder, stmt->condition);
    
    Interp_EndOfExpression(builder);
    
    auto ifInstr = Interp_If(builder, value);
    auto ifRegionIdx = Interp_Region(builder);
    
    Interp_ConvertBlock(builder, stmt->thenBlock);
    bool thenBlockRet = builder->genJump;
    builder->genJump = false;
    
    InstrIdx gotoInstr = InstrIdx_Unused;
    if(!thenBlockRet) gotoInstr = Interp_Goto(builder);
    
    auto elseRegionIdx = Interp_Region(builder);
    
    Interp_PatchIf(builder, ifInstr, value, ifRegionIdx, elseRegionIdx);
    if(!thenBlockRet) Interp_PatchGoto(builder, gotoInstr, elseRegionIdx);
    
    if(stmt->elseStmt)
    {
        Interp_ConvertNode(builder, stmt->elseStmt);
        bool elseBlockRet = builder->genJump;
        builder->genJump = false;
        
        InstrIdx elseGotoInstr = InstrIdx_Unused;
        if(!elseBlockRet) elseGotoInstr = Interp_Goto(builder);
        auto endRegionIdx = Interp_Region(builder);
        if(!elseBlockRet) Interp_PatchGoto(builder, elseGotoInstr, endRegionIdx);
        if(!thenBlockRet) Interp_PatchGoto(builder, gotoInstr, endRegionIdx);
    }
}

void Interp_ConvertFor(Interp_Builder* builder, Ast_For* stmt)
{
    if(stmt->initialization)
        Interp_ConvertNode(builder, stmt->initialization);
    
    InstrIdx loopBackRegion = InstrIdx_Unused;
    InstrIdx body = InstrIdx_Unused;
    if(stmt->condition)
    {
        auto gotoHeader = Interp_Goto(builder);
        auto header = Interp_Region(builder);
        auto continueTarget = header;
        Interp_PatchGoto(builder, gotoHeader, header);
        
        auto condReg = Interp_ConvertNodeRVal(builder, stmt->condition);
        Assert(condReg != RegIdx_Unused);
        
        loopBackRegion = header;
        
        auto headerBranch = Interp_If(builder, condReg);
        body = Interp_Region(builder);
        
        Interp_EndOfExpression(builder);
        Interp_ConvertBlock(builder, stmt->body);
        bool blockGenJump = builder->genJump;
        builder->genJump = false;
        
        Interp_EndOfExpression(builder);
        
        // NOTE: This is potentially an orphan region. Is this a
        // problem for correctness?
        if(stmt->update)
        {
            if(!blockGenJump)
            {
                auto gotoInc = Interp_Goto(builder);
                auto incRegion = Interp_Region(builder);
                continueTarget = incRegion;
                Interp_PatchGoto(builder, gotoInc, incRegion);
            }
            
            Interp_ConvertNode(builder, stmt->update);
        }
        
        if(!blockGenJump)
            Interp_Goto(builder, header);
        
        auto exit = Interp_Region(builder);
        
        Interp_PatchJumps(builder, exit, &builder->breaks);
        Interp_PatchJumps(builder, continueTarget, &builder->continues);
        
        Interp_PatchIf(builder, headerBranch, condReg, body, exit);
    }
    else
    {
        auto gotoBody = Interp_Goto(builder);
        auto body = Interp_Region(builder);
        Interp_PatchGoto(builder, gotoBody, body);
        
        Interp_ConvertBlock(builder, stmt->body);
        Interp_EndOfExpression(builder);
        if(stmt->update)
            Interp_ConvertNode(builder, stmt->update);
        
        Interp_Goto(builder, body);
        // Exit region
        Interp_Region(builder);
    }
}

void Interp_ConvertWhile(Interp_Builder* builder, Ast_While* stmt)
{
    auto gotoHeader = Interp_Goto(builder);
    auto header = Interp_Region(builder);
    Interp_PatchGoto(builder, gotoHeader, header);
    
    auto condReg = Interp_ConvertNodeRVal(builder, stmt->condition);
    Assert(condReg != RegIdx_Unused);
    
    auto headerBranch = Interp_If(builder, condReg);
    auto body = Interp_Region(builder);
    
    Interp_EndOfExpression(builder);
    
    Interp_ConvertBlock(builder, stmt->doBlock);
    bool blockGenJump = builder->genJump;
    builder->genJump = false;
    
    Interp_EndOfExpression(builder);
    
    if(!blockGenJump) Interp_Goto(builder, header);
    auto exit = Interp_Region(builder);
    
    Interp_PatchJumps(builder, exit, &builder->breaks);
    Interp_PatchJumps(builder, header, &builder->continues);
    
    Interp_PatchIf(builder, headerBranch, condReg, body, exit);
}

void Interp_ConvertDoWhile(Interp_Builder* builder, Ast_DoWhile* stmt)
{
    auto gotoBody = Interp_Goto(builder);
    auto body = Interp_Region(builder);
    Interp_PatchGoto(builder, gotoBody, body);
    
    Interp_EndOfExpression(builder);
    Interp_ConvertNode(builder, stmt->doStmt);
    bool blockGenJump = builder->genJump;
    builder->genJump = false;
    
    Interp_EndOfExpression(builder);
    
    auto condReg = Interp_ConvertNodeRVal(builder, stmt->condition);
    Assert(condReg != RegIdx_Unused);
    
    InstrIdx headerBranch = InstrIdx_Unused;
    if(!blockGenJump) headerBranch = Interp_If(builder, condReg);
    auto exit = Interp_Region(builder);
    if(!blockGenJump) Interp_PatchIf(builder, headerBranch, condReg, body, exit);
    
    Interp_PatchJumps(builder, exit, &builder->breaks);
    Interp_PatchJumps(builder, body, &builder->continues);
}

void Interp_ConvertSwitch(Interp_Builder* builder, Ast_Switch* stmt)
{
    ScratchArena scratch;
    
    RegIdx value = Interp_ConvertNodeRVal(builder, stmt->switchExpr);
    Interp_EndOfExpression(builder);
    
    auto branchInstr = Interp_If(builder, value);
    
    Slice<int64> caseVals = { 0, 0 };
    Slice<InstrIdx> caseRegions = { 0, 0 };  // Does not include default case
    InstrIdx defaultCase = InstrIdx_Unused;
    for_array(i, stmt->cases)
    {
        // @temporary
        Assert(stmt->cases[i]->kind == AstKind_ConstValue && "Other node kinds in switch are not yet supported");
        
        auto constValue = (Ast_ConstValue*)stmt->cases[i];
        Assert(constValue->type->typeId == Typeid_Integer);
        
        int64 intValue = *(int64*)constValue->addr;
        caseVals.Append(scratch, intValue);
        Interp_EndOfExpression(builder);
    }
    
    for_array(i, stmt->stmts)
    {
        auto caseRegion = Interp_Region(builder);
        Interp_PatchJumps(builder, caseRegion, &builder->fallthroughs);
        
        if(i == stmt->defaultIdx)
            defaultCase = caseRegion;
        else
            caseRegions.Append(scratch, caseRegion);
        
        Interp_ConvertNode(builder, stmt->stmts[i]);
        Interp_EndOfExpression(builder);
        
        bool caseGenJump = builder->genJump;
        builder->genJump = false;
        
        if(!caseGenJump)
        {
            // If no explicit jump, a break is inserted
            auto breakJump = Interp_Goto(builder);
            builder->breaks.Append(breakJump);
        }
    }
    
    auto exit = Interp_Region(builder);
    
    if(defaultCase == InstrIdx_Unused)
        defaultCase = exit;
    
    Interp_PatchBranch(builder, branchInstr, caseVals, caseRegions, defaultCase);
    Interp_PatchJumps(builder, exit, &builder->breaks);
    Assert(builder->continues.length <= 0);
}

void Interp_ConvertReturn(Interp_Builder* builder, Ast_Return* stmt)
{
    builder->genJump = true;
    
    // Generate all defer statements
    for(int i = builder->deferStack.length - 1; i >= 0; --i)
    {
        Interp_ConvertNode(builder, builder->deferStack[i]);
        Interp_EndOfExpression(builder);
    }
    
    auto proc = builder->proc;
    if(stmt->rets.length <= 0)
        Interp_ReturnVoid(builder);
    else
    {
        auto procType = (Ast_ProcType*)builder->symbols[proc->symIdx].type;
        
        ScratchArena scratch;
        Slice<Interp_Val> regs;
        for_array(i, stmt->rets)
        {
            auto ret = stmt->rets[i];
            auto reg = Interp_ConvertNode(builder, ret);
            regs.Append(scratch, reg);
        }
        
        for(int i = 0; i < stmt->rets.length - 1; ++i)
        {
            RegIdx addr = (proc->retRule == TB_PASSING_INDIRECT) + i;
            Interp_BuildPreRet(builder, addr, regs[i], stmt->rets[i]->type, TB_PASSING_INDIRECT);
        }
        
        int lastRet = stmt->rets.length - 1;
        auto toRet = Interp_BuildPreRet(builder, 0, regs[lastRet], stmt->rets[lastRet]->type, proc->retRule);
        Interp_Return(builder, toRet);
    }
}

void Interp_ConvertSimpleJump(Interp_Builder* builder, Array<InstrIdx>* toPatch)
{
    // Convert defer statements of the current scope only
    for(int i = builder->deferStack.length - 1; i > builder->curDeferIdx; --i)
    {
        Interp_ConvertNode(builder, builder->deferStack[i]);
        Interp_EndOfExpression(builder);
    }
    
    builder->genJump = true;
    auto jump = Interp_Goto(builder);
    toPatch->Append(jump);
}

void Interp_PatchJumps(Interp_Builder* builder, InstrIdx region, Array<InstrIdx>* toPatch)
{
    for_array(i, *toPatch)
    {
        Interp_PatchGoto(builder, (*toPatch)[i], region);
    }
    
    toPatch->FreeAll();
}

Interp_Val Interp_ConvertIdent(Interp_Builder* builder, Ast_IdentExpr* expr)
{
    Interp_Val res;
    
    if(expr->declaration->kind == AstKind_ProcDecl)  // Procedure
    {
        auto procDecl = (Ast_ProcDecl*)expr->declaration;
        
        if(procDecl->phase < CompPhase_Bytecode)
            Dg_Yield(builder->graph, procDecl, CompPhase_Bytecode);
        
        auto address = Interp_GetSymbolAddress(builder, procDecl->symIdx);
        
        res.reg = address;
    }
    else  // Variable
    {
        Assert(expr->declaration->kind == AstKind_VarDecl);
        auto decl = (Ast_VarDecl*)expr->declaration;
        
        bool isGlobalVariable = decl->declIdx == -1;
        if(isGlobalVariable)
        {
            auto address = Interp_GetSymbolAddress(builder, decl->symIdx);
            Assert(address != RegIdx_Unused);
            res.reg = address;
        }
        else
        {
            res.reg = builder->declToAddr[decl->declIdx];
        }
    }
    
    res.type = Interp_LValue;
    return res;
}

Slice<Interp_Val> Interp_ConvertCall(Interp_Builder* builder, Ast_FuncCall* call, Arena* allocTo)
{
    ScratchArena scratch1(allocTo);
    ScratchArena scratch2(scratch1, allocTo);
    Slice<Interp_Val> rets = { 0, 0 };
    Slice<RegIdx> args = { 0, 0 };
    
    // Generate target
    auto proc = builder->proc;
    auto target = Interp_ConvertNode(builder, call->target);
    Assert(target.type == Interp_LValue);
    
    auto procType = (Ast_ProcType*)GetBaseTypeShallow(call->target->type);
    Assert(procType->typeId == Typeid_Proc);
    
    // Implicitly load the pointer to the pointer to the function, to get the
    // pointer to the function.
    if(call->target->type->typeId != Typeid_Proc)
    {
        // Same as dereferencing
        target.reg = Interp_Load(builder, Interp_Ptr, target.reg, 8, false);
    }
    
    auto targetReg = target.reg;
    
    rets.Resize(scratch1, procType->retTypes.length);
    
    TB_PassingRule retRule = TB_PASSING_IGNORE;
    if(procType->retTypes.length > 0)
    {
        auto retDebugType = Tc_ConvertToDebugType(proc->module, procType->retTypes.last());
        retRule = tb_get_passing_rule_from_dbg(proc->module, retDebugType, false);
        
        // Arguments related to return values first (last one is first, the rest are in order)
        if(retRule == TB_PASSING_INDIRECT)
        {
            uint64 size = procType->retTypes.last()->size;
            uint64 align = procType->retTypes.last()->align;
            auto local = Interp_Local(builder, size, align);
            args.Append(scratch2, local);
        }
    }
    
    for(int i = 0; i < rets.length - 1; ++i)
    {
        // The rest of the returns are all indirect
        uint64 size = procType->retTypes[i]->size;
        uint64 align = procType->retTypes[i]->align;
        auto local = Interp_Local(builder, size, align);
        args.Append(scratch2, local);
    }
    
    for_array(i, call->args)
    {
        // Get passing rule
        auto debugType = Tc_ConvertToDebugType(proc->module, procType->args[i]->type);
        TB_PassingRule rule = tb_get_passing_rule_from_dbg(proc->module, debugType, false);
        
        auto val = Interp_ConvertNode(builder, call->args[i]);
        auto passedArg = Interp_PassArg(builder, val, call->args[i]->type, rule, scratch1);
        args.Append(scratch2, passedArg);
    }
    
    RegIdx singleRet = Interp_Call(builder, target.reg, args);
    
    if(procType->retTypes.length <= 0)
        return { 0, 0 };
    
    // Handle returns
    Slice<TB_PassingRule> retRules;
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
        rets[i] = Interp_GetRet(builder, TB_PASSING_INDIRECT, retReg, procType->retTypes[i]);
    }
    
    if(procType->retTypes.length > 0)
        rets[rets.length-1] = Interp_GetRet(builder, retRule, singleRet, procType->retTypes.last());
    
    return rets;
}

Interp_Val Interp_ConvertBinExpr(Interp_Builder* builder, Ast_BinaryExpr* expr)
{
    Interp_Val res;
    
    // TODO: Check if there is a function associated with this operator (operator overloading)
    // if so just call that function
    
    // Logical operators
    if(expr->op == Tok_LogAnd || expr->op == Tok_LogOr)
    {
        // Since logical operators behave very differently
        // from all other operators because they alter the
        // control flow, they reside in a separate function
        res = Interp_ConvertLogicExpr(builder, expr);
    }
    else  // All other operators
    {
        auto lhsFull = Interp_ConvertNode(builder, expr->lhs);
        auto rhsFull = Interp_ConvertNode(builder, expr->rhs);
        
        if(expr->op == '=')
            return Interp_Assign(builder, rhsFull, expr->rhs->type, lhsFull, expr->lhs->type);
        
        // No structs are involved here for sure (+= or any other
        // operator does not work on structs)
        
        bool isAssign = false;
        TokenType op = Ast_GetAssignUnderlyingOp((TokenType)expr->op, &isAssign);
        
        // If operator is '=' lhs rvalue is not needed
        RegIdx lhs = Interp_GetRVal(builder, lhsFull, expr->lhs->type);
        RegIdx rhs = Interp_GetRVal(builder, rhsFull, expr->rhs->type);
        Assert(lhs != RegIdx_Unused && rhs != RegIdx_Unused);
        
        if(IsTypeIntegral(expr->type))
        {
            bool isSigned = expr->type->isSigned;
            switch_nocheck(op)
            {
                case '=':        break;  // Not handled explicitly
                case '+':        res.reg = Interp_Add(builder, lhs, rhs); break;
                case '-':        res.reg = Interp_Sub(builder, lhs, rhs); break;
                case '*':        res.reg = Interp_Mul(builder, lhs, rhs); break;
                case '/':        res.reg = Interp_Div(builder, lhs, rhs, isSigned); break;
                case '%':        res.reg = Interp_Mod(builder, lhs, rhs, isSigned); break;
                case Tok_LShift: res.reg = Interp_ShL(builder, lhs, rhs); break;
                case Tok_RShift: res.reg = expr->type->isSigned? Interp_Sar(builder, lhs, rhs) : Interp_ShR(builder, lhs, rhs); break;
                case '&':        res.reg = Interp_And(builder, lhs, rhs); break;
                case '|':        res.reg = Interp_Or(builder, lhs, rhs); break;
                case '^':        res.reg = Interp_Xor(builder, lhs, rhs); break;
                case Tok_EQ:     res.reg = Interp_CmpEq(builder, lhs, rhs); break;
                case Tok_NEQ:    res.reg = Interp_CmpNe(builder, lhs, rhs); break;
                case '<':        res.reg = Interp_CmpILT(builder, lhs, rhs, isSigned); break;
                case '>':        res.reg = Interp_CmpIGT(builder, lhs, rhs, isSigned); break;
                case Tok_LE:     res.reg = Interp_CmpILE(builder, lhs, rhs, isSigned); break;
                case Tok_GE:     res.reg = Interp_CmpIGE(builder, lhs, rhs, isSigned); break;
                default: Assert(false && "Not implemented"); break;
            } switch_nocheck_end;
        }
        else if(expr->type->typeId == Typeid_Float)
        {
            switch_nocheck(op)
            {
                case '=':     break;  // Not handled explicitly
                case '+':     res.reg = Interp_FAdd(builder, lhs, rhs); break;
                case '-':     res.reg = Interp_FSub(builder, lhs, rhs); break;
                case '*':     res.reg = Interp_FMul(builder, lhs, rhs); break;
                case '/':     res.reg = Interp_FDiv(builder, lhs, rhs); break;
                case Tok_EQ:  res.reg = Interp_CmpEq(builder, lhs, rhs); break;
                case Tok_NEQ: res.reg = Interp_CmpNe(builder, lhs, rhs); break;
                case '<':     res.reg = Interp_CmpFLT(builder, lhs, rhs); break;
                case '>':     res.reg = Interp_CmpFGT(builder, lhs, rhs); break;
                case Tok_LE:  res.reg = Interp_CmpFLE(builder, lhs, rhs); break;
                case Tok_GE:  res.reg = Interp_CmpFGE(builder, lhs, rhs); break;
                default: Assert(false && "Not implemented"); break;
            } switch_nocheck_end;
        }
        
        res.type = Interp_RValue;
        
        if(isAssign)
            res = Interp_Assign(builder, res, expr->rhs->type, lhsFull, expr->lhs->type);
    }
    
    return res;
}

// TODO: Creating things out-of-order is kind of a pain right now.
// It is why this function is so long.
// Things should be modified to the API to permit slower, but more
// convenient out-of-order insertion of operations.
Interp_Val Interp_ConvertLogicExpr(Interp_Builder* builder, Ast_BinaryExpr* expr)
{
    Assert(expr->op == Tok_LogAnd || expr->op == Tok_LogOr);
    Interp_Val res;
    
    auto proc = builder->proc;
    
    // We're adhering to the following scheme:
    // a && b
    //
    //      if (a) { goto rhs  } else { goto false }
    // rhs: if (b) { goto true } else { goto false }
    //
    //
    // a || b
    //
    //      if (a) { goto true } else { goto rhs   }
    // rhs: if (b) { goto true } else { goto false }
    
    bool isAnd = expr->op == Tok_LogAnd;
    
    // Evaluate first operand
    // NOTE(Leo): Logical operators are left-to-right, so this is correct.
    // Short-circuit logic doesn't make very much sense if the operators
    // were right-to-left, and it's not like this will crash the program
    // if we eventually change this. (The short circuit stuff will just not work)
    // NOTE(Leo): We currently leave one empty instruction at the end of each
    // region (true and false), because that's what we'll need at least. At most
    // we need 2, one for an unconditional jump and one for an immediate instruction
    // for a certain register. (Except when adding statements like for an if statement)
    
    auto lhs = Interp_ConvertNode(builder, expr->lhs);
    
    InstrIdx trueEndInstr = InstrIdx_Unused;
    InstrIdx falseEndInstr = InstrIdx_Unused;
    InstrIdx trueRegion = InstrIdx_Unused;
    int trueRegionOffset = 0;
    InstrIdx falseRegion = InstrIdx_Unused;
    int falseRegionOffset = 0;
    InstrIdx rhsRegion = InstrIdx_Unused;
    
    if(lhs.type == Interp_RValuePhi)
    {
        trueRegion = lhs.phi.trueRegion;
        falseRegion = lhs.phi.falseRegion;
        trueEndInstr = lhs.phi.trueEndInstr;
        falseEndInstr = lhs.phi.falseEndInstr;
        
        // This is the new "final" true region for and,
        // and the new "final" false region for or.
        // The merge region is always the last region.
        InstrIdx newRegion = lhs.phi.mergeRegion;
        InstrIdx placeholder = Interp_Placeholder(builder);
        
        // Chain with the previous branch
        // "Or"  chains on false,
        // "And" chains on true
        
        rhsRegion = Interp_Region(builder);
        Interp_Instr* chainWith = isAnd ? &proc->instrs[trueEndInstr] : &proc->instrs[falseEndInstr];
        chainWith->op = Op_Branch;
        chainWith->branch.defaultCase = rhsRegion;
        chainWith->branch.keyStart = 0;
        chainWith->branch.caseStart = 0;
        chainWith->branch.count = 0;
        
        if(isAnd)
        {
            // Set true label to new region
            trueRegion = newRegion;
            trueRegionOffset = builder->instrOffsets[trueRegion];
            trueEndInstr = placeholder;
        }
        else
        {
            // Set false label to new region
            falseRegion = newRegion;
            falseRegionOffset = builder->instrOffsets[falseRegion];
            falseEndInstr = placeholder;
        }
    }
    else
    {
        // There isn't an already existing branch to chain with,
        // so we create the true and false regions here.
        
        auto lhsRValue = Interp_GetRVal(builder, lhs, expr->lhs->type);
        auto ifInstr = Interp_If(builder, lhsRValue);
        
        trueRegion   = Interp_Region(builder);
        trueRegionOffset = builder->instrOffsets[trueRegion];
        trueEndInstr = Interp_Placeholder(builder);
        
        falseRegion   = Interp_Region(builder);
        falseRegionOffset = builder->instrOffsets[falseRegion];
        falseEndInstr = Interp_Placeholder(builder);
        
        rhsRegion = Interp_Region(builder);
        
        // 'And' goes to false directly if lhs is false, otherwise evaluate rhs.
        // 'Or' goes to true directly if lhs is true, otherwise evaluate rhs.
        Interp_PatchIf(builder, ifInstr, lhsRValue, isAnd? rhsRegion : trueRegion, isAnd? falseRegion : rhsRegion);
    }
    
    // Generate the region for evaluating the rhs
    {
        Assert(rhsRegion != InstrIdx_Unused);
        // We're in the rhs region now.
        Assert(proc->instrs.length > 0 && proc->instrs.length-1 == rhsRegion);
        
        // Evaluate second operand
        auto rhs = Interp_ConvertNodeRVal(builder, expr->rhs);
        
        // Rhs could have added instructions and changed the control flow
        trueRegion = Interp_UpdateInstrIdx(builder, trueRegion, trueRegionOffset);
        falseRegion = Interp_UpdateInstrIdx(builder, falseRegion, falseRegionOffset);
        
        auto ifInstr = Interp_If(builder, rhs, trueRegion, falseRegion);
        
        // Create merge region
        auto mergeRegion = Interp_Region(builder);
        
        res.type = Interp_RValuePhi;
        res.phi.trueRegion    = trueRegion;
        res.phi.falseRegion   = falseRegion;
        res.phi.trueEndInstr  = trueEndInstr;
        res.phi.falseEndInstr = falseEndInstr;
        res.phi.mergeRegion   = mergeRegion;
    }
    
    // Set res.phi to be true, false, merge
    return res;
}

// TODO: handle pointer arithmetic
Interp_Val Interp_ConvertUnaryExpr(Interp_Builder* builder, Ast_UnaryExpr* expr)
{
    Interp_Val res;
    Interp_Val applyTo = Interp_ConvertNode(builder, expr->expr);
    
    auto srcType = Interp_ConvertType(expr->expr->type);
    auto srcAlign = expr->expr->type->align;
    auto srcSize = expr->expr->type->size;
    
    RegIdx srcReg = RegIdx_Unused;
    // We can avoid a load instruction here as the '&' operator only needs the address
    if(expr->op != '&') srcReg = Interp_GetRVal(builder, applyTo, expr->expr->type);
    
    // TODO: I don't really understand neg vs not of tilde...
    switch(expr->op)
    {
        case Tok_Increment:
        case Tok_Decrement:
        {
            Assert(applyTo.type == Interp_LValue);
            
            // This gets complicated if we consider atomics... For now we won't
            // support them.
            bool isInc = expr->op == Tok_Increment;
            
            if(srcType.type == InterpType_Ptr) Assert(false && "Not implemented yet");
            
            auto stride = Interp_ImmUInt(builder, srcType, 1);
            auto preOp  = srcReg;
            auto postOp = isInc ? Interp_Add(builder, preOp, stride) : Interp_Sub(builder, preOp, stride);
            
            Interp_Store(builder, srcType, applyTo.reg, postOp, srcAlign, false);
            
            res.reg = expr->isPostfix ? preOp : postOp;
            break;
        }
        case '+': res.reg = srcReg; break;
        case '-':
        {
            if(srcType.type == InterpType_Float)
            {
                RegIdx constant = RegIdx_Unused;
                if(srcType.data == FType_Flt32)
                    constant = Interp_ImmFloat32(builder, srcType, 0);
                else if(srcType.data == FType_Flt64)
                    constant = Interp_ImmFloat64(builder, srcType, 0);
                else Assert(false && "Unreachable code");
                res.reg = Interp_FSub(builder, constant, srcReg);
            }
            else
            {
                res.reg = Interp_Sub(builder, Interp_ImmUInt(builder, srcType, 0), srcReg);
            }
            
            break;
        }
        case '!': res.reg = Interp_CmpEq(builder, srcReg, Interp_ImmUInt(builder, srcType, 0)); break;
        case '~': res.reg = Interp_Not(builder, srcReg); break;
        case '*':
        {
            // Take the r-value and simply turn it into an l-value
            Assert(IsTypeDereferenceable(expr->expr->type));
            res.reg = srcReg;
            res.type = Interp_LValue;
            break;
        }
        case '&':
        {
            // Take the l-value and simply turn it into an r-value
            Assert(applyTo.type == Interp_LValue);
            res.reg = applyTo.reg;
            res.type = Interp_RValue;
            break;
        }
    }
    
    return res;
}

Interp_Val Interp_ConvertTernaryExpr(Interp_Builder* builder, Ast_TernaryExpr* expr)
{
    Interp_Val res;
    return res;
}

Interp_Val Interp_ConvertSubscript(Interp_Builder* builder, Ast_Subscript* expr)
{
    Interp_Val res;
    return res;
}

Interp_Val Interp_ConvertMemberAccess(Interp_Builder* builder, Ast_MemberAccess* expr)
{
    Interp_Val res;
    res.type = Interp_LValue;
    
    auto lhs = Interp_ConvertNode(builder, expr->target);
    Assert(Interp_IsValueValid(lhs) && lhs.type == Interp_LValue);
    
    RegIdx base = lhs.reg;
    // If lhs is of pointer type, do "->" instead of simple "."
    // (In this language that's the same thing)
    if(IsTypeDereferenceable(expr->target->type))
    {
        // Same as dereferencing, you load the pointer to the pointer to the variable
        // to get to the pointer to the variable. (The result is still an LValue)
        base = Interp_Load(builder, Interp_Ptr, base, 8, false);
    }
    
    Assert(expr->structDecl);
    
    int64 offset = expr->structDecl->memberOffsets[expr->memberIdx];
    
    // If the offset is 0, we can just avoid the instruction altogether.
    if(offset != 0)
        res.reg = Interp_MemberAccess(builder, base, offset);
    else
        res.reg = base;
    
    return res;
}

Interp_Val Interp_ConvertConstValue(Interp_Builder* builder, Ast_ConstValue* expr)
{
    Interp_Val res;
    Interp_Type type = Interp_ConvertType(expr->type);
    
    switch_nocheck(expr->type->typeId)
    {
        case Typeid_Integer:
        {
            int64 val;
            memcpy(&val, expr->addr, expr->type->size);
            res.reg = Interp_ImmSInt(builder, type, val);
            break;
        }
        case Typeid_Float:
        {
            if(expr->type->size == 4)
            {
                float val = *(float*)expr->addr;
                res.reg = Interp_ImmFloat32(builder, type, val);
            }
            else if(expr->type->size == 8)
            {
                double val = *(double*)expr->addr;
                res.reg = Interp_ImmFloat64(builder, type, val);
            }
            
            break;
        }
        case Typeid_Bool:
        {
            bool val = *(bool*)expr->addr;
            res.reg = Interp_ImmBool(builder, val);
            break;
        }
        default: Assert(false);
    } switch_nocheck_end;
    
    return res;
}

Interp_Val Interp_Assign(Interp_Builder* builder, Interp_Val src, TypeInfo* srcType, Interp_Val dst, TypeInfo* dstType)
{
    Assert(dst.type == Interp_LValue);
    
    TypeInfo* type = dstType;
    
    if(dstType->typeId == Typeid_Struct ||
       dstType->typeId == Typeid_Ident)
    {
        Assert(src.type == Interp_LValue);
        RegIdx sizeReg = Interp_ImmUInt(builder, Interp_Int64, type->size);
        Interp_MemCpy(builder, dst.reg, src.reg, sizeReg, type->align, false);
    }
    else if(srcType->typeId == Typeid_Proc)
    {
        Assert(src.type == Interp_LValue);
        Interp_Store(builder, Interp_ConvertType(type), dst.reg, src.reg, 8, false);
    }
    else
    {
        auto srcRVal = Interp_GetRVal(builder, src, srcType);
        Interp_Store(builder, Interp_ConvertType(type), dst.reg, srcRVal, type->align, false);
    }
    
    return dst;
}

// @cleanup The code for handling the ABI is still very messy...
Interp_Proc* Interp_ConvertProc(Interp* interp, Ast_ProcDef* astProc, bool* outYielded)
{
    Interp_Builder builderVar;
    auto builder = &builderVar;
    builder->graph = interp->graph;
    
    auto astDecl = Ast_GetProcDefType(astProc);
    
    Interp_Proc* proc = 0;
    if(astProc->procIdx == ProcIdx_Unused)
        proc = Interp_MakeProc(interp);
    else
    {
        proc = &interp->procs[astProc->procIdx];
        
        // Start from a clean slate
        proc->~Interp_Proc();
        Interp_Proc defaultProc;
        *proc = defaultProc;
    }
    
    builder->proc = proc;
    builder->symbols = interp->symbols;
    astProc->procIdx = interp->procs.length - 1;
    
    if(astProc->decl->phase <= CompPhase_Bytecode)
    {
        Dg_Yield(interp->graph, astProc->decl, CompPhase_Bytecode);
        return false;
    }
    
    proc->symIdx = astProc->decl->symIdx;
    
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
    int retArgs = max((int64)0, astDecl->retTypes.length - 1) + (proc->retRule == TB_PASSING_INDIRECT);
    int abiArgsCount = argsCount + retArgs;
    for(int i = 0; i < abiArgsCount; ++i)
        builder->permanentRegs.Append(i);
    
    if(abiArgsCount != -1)
        builder->regCounter = abiArgsCount;
    else
        builder->regCounter = 0;
    
    // Insert local for each declaration (including
    // procedure arguments)
    builder->declToAddr.Resize(astProc->declsFlat.length);
    for_array(i, astProc->declsFlat)
    {
        bool isArg = i < argsCount;
        auto decl = astProc->declsFlat[i];
        if(decl->kind == AstKind_VarDecl)
        {
            if(!isArg || proc->argRules[i] == TB_PASSING_DIRECT)
            {
                auto varDecl = (Ast_VarDecl*)decl;
                auto addr = Interp_Local(builder, varDecl->type->size, varDecl->type->align);
                
                builder->declToAddr[i] = addr;
            }
            else if(isArg && proc->argRules[i] == TB_PASSING_INDIRECT)
            {
                // The register itself contains the address needed
                // (the stack allocation is performed by the caller)
                builder->declToAddr[i] = i;
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
                Interp_Store(builder, Interp_ConvertType(argType),
                             builder->declToAddr[i], retArgs + i,
                             argType->align, false);
                
                break;
            }
            case TB_PASSING_INDIRECT: break;
        }
    }
    
    // Block
    Interp_ConvertBlock(builder, &astProc->block);
    
    // If return instruction was not generated,
    // (which is the only jump that a procedure can have)
    // a fictitious one is generated for correctness
    if(!builder->genJump) Interp_ReturnVoid(builder);
    builder->genJump = true;
    
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
    
    if(astDecl->retTypes.length > 0)
    {
        if(proc->retRule == TB_PASSING_INDIRECT)
            proc->retType = Interp_Ptr;
        else
            proc->retType = Interp_ConvertType(astDecl->retTypes.last());
    }
    
    *outYielded = builder->yielded;
    return proc;
}

Interp_Type Interp_ConvertType(TypeInfo* type)
{
    Interp_Type res = { 0, 0, 0 };
    switch(type->typeId)
    {
        // NOTE(Leo): If it's a struct, and we'd like to put
        // it in register, we use an integer type
        case Typeid_Bool:
        case Typeid_Char:
        case Typeid_Integer:
        case Typeid_Ident:
        case Typeid_Struct:
        case Typeid_None:    res.type = InterpType_Int;   break;
        case Typeid_Float:   res.type = InterpType_Float; break;
        case Typeid_Proc:
        case Typeid_Ptr:
        case Typeid_Arr:     res.type = InterpType_Ptr;   break;
        case Typeid_Raw:     Assert(false && "Raw type is not next to a pointer");
    }
    
    if(type->typeId == Typeid_Float)
    {
        if(type->size == 4)
            res.data = FType_Flt32;
        else if(type->size == 8)
            res.data = FType_Flt64;
    }
    else if(type->typeId == Typeid_Bool)
    {
        res.data = 1;
    }
    else if(type->typeId == Typeid_Proc)
    {
        res.data = 64;
    }
    else
    {
        res.data = type->size * 8;
    }
    
    return res;
}

RegIdx Interp_ConvertTypeConversion(Interp_Builder* builder, RegIdx src, Interp_Type srcType, TypeInfo* srcFullType, TypeInfo* dst)
{
    Interp_Type dstType = Interp_ConvertType(dst);
    return Interp_ConvertTypeConversion(builder, src, srcType, dstType, srcFullType->isSigned, dst->isSigned);
}

// @cleanup Refactor this function to be more readable
RegIdx Interp_ConvertTypeConversion(Interp_Builder* builder, RegIdx src, Interp_Type srcType, Interp_Type dstType,
                                    bool srcIsSigned, bool dstIsSigned)
{
    RegIdx res = src;
    if(srcType.type == dstType.type)
    {
        if(srcType.data != dstType.data)
        {
            if(srcType.data > dstType.data)  // Truncate
            {
                if(dstType.data == 1)
                    res = Interp_CmpNe(builder, src, Interp_ImmUInt(builder, srcType, 0));
                else
                    res = Interp_Trunc(builder, src, dstType);
            }
            else  // Extend
            {
                if(srcType.type == InterpType_Float)
                    res = Interp_FPExt(builder, src, dstType);
                else if(srcType.type == InterpType_Int)
                {
                    if(srcIsSigned)
                        res = Interp_SExt(builder, src, dstType);
                    else
                        res = Interp_ZExt(builder, src, dstType);
                }
            }
        }
    }
    else if(srcType.type == InterpType_Int)
    {
        if(dstType.type == InterpType_Float)
            res = Interp_Int2Float(builder, src, dstType, srcIsSigned);
        else if(dstType.type == InterpType_Ptr)
            res = Interp_Int2Ptr(builder, src, dstType);
    }
    else if(srcType.type == InterpType_Float)
    {
        if(dstType.type == InterpType_Int)
        {
            if(dstType.data == 1)
            {
                auto constant = RegIdx_Unused;
                if(srcType.data == FType_Flt32)
                    constant = Interp_ImmFloat32(builder, Interp_F32, 0);
                else if(srcType.data == FType_Flt64)
                    constant = Interp_ImmFloat64(builder, Interp_F64, 0);
                res = Interp_CmpNe(builder, src, constant);
            }
            else
                res = Interp_Float2Int(builder, src, dstType, dstIsSigned);
        }
    }
    else if(srcType.type == InterpType_Ptr)
    {
        if(dstType.type == InterpType_Int)
            res = Interp_Ptr2Int(builder, src, dstType);
    }
    
    return res;
}

void Interp_ExecProc(Interp_Proc* proc)
{
    
}
