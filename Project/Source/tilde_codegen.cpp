
#include "base.h"
#include "interpreter.h"

#include "tilde_codegen.h"

Tc_Context Tc_InitCtx(TB_Module* module, bool emitAsm)
{
    Tc_Context result;
    result.module = module;
    result.emitAsm = emitAsm;
    return result;
}

void Tc_TestCode(Ast_FileScope* file, Interp* interp)
{
    // See TB_FeatureSet, this allows us to tell the code generator
    // what extensions are active in the platform, an example is enabling
    // AVX or BF16
    TB_FeatureSet featureSet = { 0 };
    TB_Arch arch  = TB_ARCH_X86_64;
    TB_System sys = TB_SYSTEM_WINDOWS;
    TB_DebugFormat debugFmt = TB_DEBUGFMT_NONE;
    TB_Module* module = tb_module_create(arch, sys, &featureSet, false);
    
    TB_ExecutableType exeType = TB_EXECUTABLE_PE;
    const bool useTbLinker = false;
    
    Tc_Context ctx = Tc_InitCtx(module, true);
    
#if 0
    for_array(i, file->scope.stmts)
    {
        auto stmt = file->scope.stmts[i];
        if(stmt->kind == AstKind_ProcDef)
            Tc_GenProcDef_(&ctx, (Ast_ProcDef*)stmt);
        else if(stmt->kind == AstKind_StructDef)
            Tc_GenStructDef_(&ctx, (Ast_StructDef*)stmt);
        else
            Assert(false);
    }
#else
    Arena regArena = Arena_VirtualMemInit(MB(512), MB(2));
    Arena symArena = Arena_VirtualMemInit(MB(512), MB(2));
    Arena flagArena = Arena_VirtualMemInit(MB(512), MB(2));
    ctx.regArena = &regArena;
    ctx.symArena = &symArena;
    ctx.flagArena = &flagArena;
    
    for_array(i, interp->procs)
    {
        auto proc = &interp->procs[i];
        Tc_GenProc(&ctx, proc);
    }
#endif
    
    int numFuncs = tb_module_get_function_count(module);
    printf("Num funcs: %d\n", numFuncs);
    
    if(!ctx.mainProc)
    {
        printf("No main procedure was found.\n");
        return;
    }
    
    // Linking
#if 1
    if(useTbLinker)
    {
        // TB Linker is still extremely early in development,
        // does not seem to work at this moment. (Function calls
        // seem to break executables)
        
        TB_Linker* linker = tb_linker_create(exeType, arch);
        defer(tb_linker_destroy(linker));
        
        tb_linker_append_module(linker, ctx.module);
        // Append user defined object files to the module
        
        // C run-time library
        //tb_linker_append_library()
        
        tb_linker_set_entrypoint(linker, "main");
        
        TB_ExportBuffer buffer = tb_linker_export(linker);
        defer(tb_export_buffer_free(buffer));
        
        if(!tb_export_buffer_to_file(buffer, "output.exe"))
        {
            printf("Tilde Backend: Failed to export the file!\n");
            return;
        }
        
        printf("Tilde Backend: Successfully created executable file!\n");
    }
    else
    {
        // Export object file
        TB_ExportBuffer buffer = tb_module_object_export(module, debugFmt);
        defer(tb_export_buffer_free(buffer));
        if(!tb_export_buffer_to_file(buffer, "output.o"))
        {
            printf("Tilde Backend: Failed to export the file!\n");
            return;
        }
        
        printf("Tilde Backend: Successfully created object file!\n");
        
        // Pass object file to linker
        //LaunchPlatformSpecificLinker();
    }
#endif
}

TB_Node* Tc_GenNode_(Tc_Context* ctx, Ast_Node* node)
{
    switch(node->kind)
    {
        case AstKind_ProcDef:       break;  // Not handled here
        case AstKind_StructDef:     break;  // Not handled here
        case AstKind_DeclBegin:     break;
        case AstKind_VarDecl:       return Tc_GenVarDecl_(ctx, (Ast_VarDecl*)node); break;
        case AstKind_EnumDecl:      printf("Enumerator not implemented.\n"); break;
        case AstKind_DeclEnd:       break;
        case AstKind_StmtBegin:     break;
        case AstKind_Block:         Tc_GenBlock_(ctx, (Ast_Block*)node); break;
        case AstKind_If:            Tc_GenIf_(ctx, (Ast_If*)node); break;
        case AstKind_For:           Tc_GenFor_(ctx, (Ast_For*)node); break;
        case AstKind_While:         Tc_GenWhile_(ctx, (Ast_While*)node); break;
        case AstKind_DoWhile:       Tc_GenDoWhile_(ctx, (Ast_DoWhile*)node); break;
        case AstKind_Switch:        Tc_GenSwitch_(ctx, (Ast_Switch*)node); break;
        case AstKind_Defer:         Tc_GenDefer_(ctx, (Ast_Defer*)node); break;
        case AstKind_Return:        Tc_GenReturn_(ctx, (Ast_Return*)node); break;
        case AstKind_Break:         Tc_GenBreak_(ctx, (Ast_Break*)node); break;
        case AstKind_Continue:      Tc_GenContinue_(ctx, (Ast_Continue*)node); break;
        case AstKind_MultiAssign:   break;
        case AstKind_EmptyStmt:     break;
        case AstKind_StmtEnd:       break;
        case AstKind_ExprBegin:     break;
        case AstKind_Literal:       printf("Literal not implemented.\n"); break;
        case AstKind_NumLiteral:    return Tc_GenNumLiteral_(ctx, (Ast_NumLiteral*)node); break;
        case AstKind_Ident:         return Tc_GenIdent_(ctx, (Ast_IdentExpr*)node); break;
        case AstKind_FuncCall:      return Tc_GenFuncCall_(ctx, (Ast_FuncCall*)node); break;
        case AstKind_BinaryExpr:    return Tc_GenBinExpr_(ctx, (Ast_BinaryExpr*)node); break;
        case AstKind_UnaryExpr:     return Tc_GenUnaryExpr_(ctx, (Ast_UnaryExpr*)node); break;
        case AstKind_TernaryExpr:   printf("Ternary expression not implemented.\n"); break;
        case AstKind_Typecast:      return Tc_GenNode_(ctx, ((Ast_Typecast*)node)->expr);
        case AstKind_Subscript:     return Tc_GenSubscript_(ctx, (Ast_Subscript*)node); break;
        case AstKind_MemberAccess:  return Tc_GenMemberAccess_(ctx, (Ast_MemberAccess*)node); break;
        case AstKind_ExprEnd:       break;
        default: Assert(false && "Enum value out of bounds");
    }
    
    return 0;
}

void Tc_GenProc(Tc_Context* ctx, Interp_Proc* proc)
{
    ScratchArena scratch;
    
    // Generate procedure itself
    tb_arena_create(&ctx->procArena, MB(1));
    
    // Debug type is still needed for debugging, but ABI is already handled
    // in the interpreter instructions.
    TB_DebugType* procType = Tc_ConvertProcToDebugType(ctx->module, proc->symbol->type);
    
    TB_Function* curProc = tb_function_create(ctx->module, -1, proc->symbol->name, TB_LINKAGE_PUBLIC, TB_COMDAT_MATCH_ANY);
    
    proc->symbol->tildeProc = curProc;
    
    // @robustness Just doing this for now because it's easier, should
    // generate the prototype directly from the interp procedure
    size_t paramCount = 0;
    TB_Node** paramNodes = tb_function_set_prototype_from_dbg(curProc, procType, &ctx->procArena, &paramCount);
    
    Assert(proc->symbol->type->typeId == Typeid_Proc);
    auto procDecl = (Ast_DeclaratorProc*)proc->symbol->type;
    int abiArgsCount = (proc->retRule == TB_PASSING_INDIRECT) + procDecl->retTypes.length - 1 + procDecl->args.length;
    
    ctx->regs.Resize(ctx->regArena, abiArgsCount);
    for(int i = 0; i < abiArgsCount; ++i)
        ctx->regs[i] = tb_inst_param(curProc, i);
    
    // Populate arg addresses as well
    for(int i = 0; i < paramCount; ++i)
    {
        if(proc->argRules[i] == TB_PASSING_DIRECT)
        {
            Tc_ExpandRegs(ctx, proc->declToAddr[i]);
            ctx->regs[proc->declToAddr[i]] = paramNodes[i];
        }
    }
    
    bool isMain = strcmp(proc->symbol->name, "main") == 0;
    if(isMain)
        ctx->mainProc = curProc;
    ctx->proc = curProc;
    
    // Init arrays
    
    // Get all basic blocks
    ctx->bbs.ptr = Arena_AllocArray(scratch, proc->instrs.length, TB_Node*);
    ctx->bbs.length = proc->instrs.length;
    
    for_array(i, proc->instrs)
    {
        if(proc->instrs[i].op == Op_Region)
            ctx->bbs[i] = tb_inst_region(curProc);
        else
            ctx->bbs[i] = 0;
    }
    
    // Generate its instructions
    Tc_GenInstrs(ctx, curProc, proc);
    
    // Perform optimization and codegen passes
    TB_Passes* passes = tb_pass_enter(curProc, &ctx->procArena);
    tb_pass_print(passes);
    TB_FunctionOutput* output = tb_pass_codegen(passes, ctx->emitAsm);
    tb_pass_exit(passes);
    
    if(ctx->emitAsm)
    {
        tb_output_print_asm(output, stdout);
    }
    
    printf("\n");
}

TB_DataType Tc_ToTBType(Interp_Type type)
{
    TB_DataType result;
    result.type = type.type;
    result.width = type.width;
    result.data = type.data;
    return result;
}

void Tc_ExpandRegs(Tc_Context* ctx, int idx)
{
    if(idx+1 > ctx->regs.length)
    {
        ctx->regs.Resize(ctx->regArena, idx + 1);
        ctx->syms.Resize(ctx->symArena, idx + 1);
        ctx->isLValue.Resize(ctx->flagArena, idx + 1);
        for(int i = ctx->regs.length; i < idx+1; ++i)
            ctx->isLValue[i] = false;
    }
}

TB_Node** Tc_GetNodeArray(Tc_Context* ctx, Interp_Proc* proc, int arrayStart, int arrayCount, Arena* allocTo)
{
    auto nodes = Arena_AllocArray(allocTo, arrayCount, TB_Node*);
    for(int i = 0; i < arrayCount; ++i)
        nodes[i] = ctx->regs[proc->regArrays[arrayStart + i]];
    
    return nodes;
}

TB_Node** Tc_GetBBArray(Tc_Context* ctx, Interp_Proc* proc, int arrayStart, int arrayCount, Arena* allocTo)
{
    auto nodes = Arena_AllocArray(allocTo, arrayCount, TB_Node*);
    for(int i = 0; i < arrayCount; ++i)
        nodes[i] = ctx->bbs[proc->instrArrays[arrayStart + i]];
    
    return nodes;
}

void Tc_GenInstrs(Tc_Context* ctx, TB_Function* tildeProc, Interp_Proc* proc)
{
    ScratchArena scratch;
    auto& bbs = ctx->bbs;
    auto& regs = ctx->regs;
    auto& syms = ctx->syms;
    
    // @performance , can unroll this loop
    for(int i = 0; i < proc->instrs.length; ++i)
    {
        auto& instr = proc->instrs[i];
        
        // Ignore the argument locals (and stores) because TB automatically inserts those
        if(instr.bitfield & InstrBF_SetUpArgs) continue;
        
        Tc_ExpandRegs(ctx, instr.dst);
        switch(instr.op)
        {
            case Op_Null:         break;
            case Op_IntegerConst:
            {
                regs[instr.dst] = tb_inst_sint(tildeProc,
                                               Tc_ToTBType(instr.imm.type),
                                               instr.imm.val);
                break;
            }
            case Op_Float32Const: break;
            case Op_Float64Const: break;
            case Op_Region:       tb_inst_set_control(tildeProc, bbs[i]); break;
            case Op_Call:
            {
                // Calls are assumed to be semantically correct, thus the
                // prototype can just be constructed here
                // Yeah but how do structs even work???
                // Maybe just include the symbol for now
                
                auto symbol = syms[instr.call.target];
                
                auto debugProto = Tc_ConvertProcToDebugType(ctx->module, symbol->type);
                auto proto = tb_prototype_from_dbg(ctx->module, debugProto);
                
                auto nodes = Tc_GetNodeArray(ctx, proc, instr.call.argStart, instr.call.argCount, scratch);
                
                TB_MultiOutput outputs = tb_inst_call(tildeProc,
                                                      proto,
                                                      regs[instr.call.target],
                                                      instr.call.argCount,
                                                      nodes);
                if(outputs.count == 1)
                {
                    regs[instr.dst] = outputs.single;
                }
                else  // 0 or >= 2
                    Assert(false);
                
                break;
            }
            case Op_SysCall:      break;
            case Op_Store:
            {
                tb_inst_store(tildeProc, regs[instr.store.val]->dt,
                              regs[instr.store.addr], regs[instr.store.val],
                              instr.store.align, false);
                break;
            }
            case Op_MemCpy:
            {
                tb_inst_memcpy(tildeProc, regs[instr.memcpy.dst],
                               regs[instr.memcpy.src], regs[instr.memcpy.count],
                               instr.memcpy.align, false);
                break;
            }
            case Op_MemSet:       break;
            case Op_AtomicTestAndSet: break;
            case Op_AtomicClear: break;
            case Op_AtomicLoad: break;
            case Op_AtomicExchange: break;
            case Op_AtomicAdd: break;
            case Op_AtomicSub: break;
            case Op_AtomicAnd: break;
            case Op_AtomicXor: break;
            case Op_AtomicOr: break;
            case Op_AtomicCompareExchange: break;
            case Op_DebugBreak: break;
            case Op_Branch:
            {
                // I think this could just be general and it would be fine.
                
                if(instr.branch.count == 0)
                    tb_inst_goto(tildeProc, bbs[instr.branch.defaultCase]);
                else if(instr.branch.count == 1)
                {
                    auto value = regs[instr.branch.value];
                    auto conds = proc->constArrays[instr.branch.keyStart];
                    auto caseBB = bbs[proc->instrArrays[instr.branch.caseStart]];
                    tb_inst_if(tildeProc, value, caseBB, bbs[instr.branch.defaultCase]);
                }
                else
                {
                    Assert(false && "Switch codegen not implemented");
#if 0
                    auto keys = Tc_GetNodeArray(ctx, proc, instr.branch.keyStart, instr.branch.count, scratch);
                    auto cases = Tc_GetBBArray(ctx, proc, instr.branch.caseStart, instr.branch.count, scratch);
                    
                    tb_inst_branch(tildeProc, keys[0]->dt, regs[instr.branch.value], regs[instr.branch.defaultCase], instr.branch.count, keys);
#endif
                }
                
                break;
            }
            case Op_Ret:
            {
                if(!(instr.bitfield & InstrBF_RetVoid))
                {
                    tb_inst_ret(tildeProc, 1, &regs[instr.unary.src]);
                }
                else
                    tb_inst_ret(tildeProc, 0, 0);
                
            }
            case Op_Load:
            {
                regs[instr.dst] = tb_inst_load(tildeProc,
                                               Tc_ToTBType(instr.load.type),
                                               regs[instr.load.addr],
                                               instr.load.align,
                                               false);
                break;
            }
            case Op_Local:
            {
                regs[instr.dst] = tb_inst_local(tildeProc, instr.local.size, instr.local.align);
                break;
            }
            case Op_GetSymbolAddress:
            {
                auto tildeSymbol = (TB_Symbol*)instr.symAddress.symbol->tildeProc;
                regs[instr.dst] = tb_inst_get_symbol_address(tildeProc, tildeSymbol);
                ctx->syms[instr.dst] = instr.symAddress.symbol;
                
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
            case Op_Not: break;
            case Op_Negate: break;
            case Op_And: break;
            case Op_Or: break;
            case Op_Xor: break;
            case Op_Add:
            {
                regs[instr.dst] = tb_inst_add(tildeProc, regs[instr.bin.src1], regs[instr.bin.src2], TB_ARITHMATIC_NONE);
                break;
            }
            case Op_Sub:
            {
                regs[instr.dst] = tb_inst_sub(tildeProc, regs[instr.bin.src1], regs[instr.bin.src2], TB_ARITHMATIC_NONE);
                break;
            }
            case Op_Mul:
            {
                regs[instr.dst] = tb_inst_mul(tildeProc, regs[instr.bin.src1], regs[instr.bin.src2], TB_ARITHMATIC_NONE);
                
                break;
            }
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
        }
    }
}

void Tc_GenProcDef_(Tc_Context* ctx, Ast_ProcDef* proc)
{
    // Create the debug type, for debugging but also so that tilde can handle the ABI
    // Automatically
    tb_arena_create(&ctx->procArena, MB(1));
    
    auto procDecl = Ast_GetDeclProc(proc);
    TB_DebugType* procType = Tc_ConvertProcToDebugType(ctx->module, procDecl);
    
    TB_Function* curProc = tb_function_create(ctx->module, strlen(proc->name->string),
                                              proc->name->string, TB_LINKAGE_PUBLIC, TB_COMDAT_MATCH_ANY);
    
    proc->tildeProc = curProc;
    
    size_t paramCount = 0;
    TB_Node** paramNodes = tb_function_set_prototype_from_dbg(curProc, procType, &ctx->procArena, &paramCount);
    for(int i = 0; i < paramCount; ++i)
        procDecl->args[i]->tildeNode = tb_inst_param(curProc, i);
    
    bool isMain = strcmp(proc->name->string, "main") == 0;
    if(isMain)
        ctx->mainProc = curProc;
    ctx->proc = curProc;
    
    auto block = (Ast_Node*)&proc->block;
    Tc_GenBlock_(ctx, (Ast_Block*)block);
    
    // Code Passes
    
    TB_Passes* passes = tb_pass_enter(ctx->proc, &ctx->procArena);
    tb_pass_print(passes);
    //tb_pass_peephole(passes);
    //tb_pass_mem2reg(passes);
    //tb_pass_peephole(passes);
    //tb_pass_loop(passes);
    //tb_pass_peephole(passes);
    TB_FunctionOutput* output = tb_pass_codegen(passes, ctx->emitAsm);
    tb_pass_exit(passes);
    
    if(ctx->emitAsm)
    {
        tb_output_print_asm(output, stdout);
    }
}

void Tc_GenStructDef_(Tc_Context* ctx, Ast_StructDef* structDef)
{
    
}

TB_Node* Tc_GenVarDecl_(Tc_Context* ctx, Ast_VarDecl* decl)
{
    TB_Node* node = tb_inst_local(ctx->proc, GetTypeSize(decl->type), GetTypeAlign(decl->type));
    decl->tildeNode = node;
    
    if(decl->initExpr)
    {
        auto exprNode = Tc_GenNode_(ctx, decl->initExpr);
        tb_inst_store(ctx->proc, Tc_ConvertToTildeType(decl->type), node, exprNode, GetTypeAlign(decl->type), false);
    }
    
    return node;
}

// This will also handle defer stuff i think
void Tc_GenBlock_(Tc_Context* ctx, Ast_Block* block)
{
    for_array(i, block->stmts)
        Tc_GenNode_(ctx, block->stmts[i]);
    
    // If there was a defer in the block, ...
}

void Tc_GenIf_(Tc_Context* ctx, Ast_If* stmt)
{
    TB_Node* thenNode = tb_inst_region(ctx->proc);
    TB_Node* endNode  = tb_inst_region(ctx->proc);
    TB_Node* elseNode = endNode;
    if(stmt->elseStmt)
        elseNode = tb_inst_region(ctx->proc);
    
    auto condNode = Tc_GenNode_(ctx, stmt->condition);
    tb_inst_if(ctx->proc, condNode, thenNode, elseNode);
    
    // Then
    tb_inst_set_control(ctx->proc, thenNode);
    Tc_GenBlock_(ctx, stmt->thenBlock);
    tb_inst_goto(ctx->proc, endNode);
    
    // Else
    if(stmt->elseStmt)
    {
        tb_inst_set_control(ctx->proc, elseNode);
        Tc_GenNode_(ctx, stmt->elseStmt);
        tb_inst_goto(ctx->proc, endNode);
    }
    
    tb_inst_set_control(ctx->proc, endNode);
}

void Tc_GenFor_(Tc_Context* ctx, Ast_For* stmt) {}
void Tc_GenWhile_(Tc_Context* ctx, Ast_While* stmt) {}
void Tc_GenDoWhile_(Tc_Context* ctx, Ast_DoWhile* smt) {}
void Tc_GenCheckDoWhile_(Tc_Context* ctx, Ast_DoWhile* stmt) {}
void Tc_GenSwitch_(Tc_Context* ctx, Ast_Switch* stmt) {}
void Tc_GenDefer_(Tc_Context* ctx, Ast_Defer* stmt) {}

void Tc_GenReturn_(Tc_Context* ctx, Ast_Return* stmt)
{
    // TODO: only doing first return
    if(stmt->rets.length > 0)
    {
        TB_Node* node = Tc_GenNode_(ctx, stmt->rets[0]);
        Assert(node);
        
        tb_inst_ret(ctx->proc, 1, &node);
    }
    else
        tb_inst_ret(ctx->proc, 0, 0);
}

void Tc_GenBreak_(Tc_Context* ctx, Ast_Break* stmt) {}
void Tc_GenContinue_(Tc_Context* ctx, Ast_Continue* stmt) {}
TB_Node* Tc_GenNumLiteral_(Tc_Context* ctx, Ast_NumLiteral* expr)
{
    TB_DataType dataType = { { TB_INT, 4, 32 } };
    auto node = tb_inst_sint(ctx->proc, dataType, 2);
    return node;
}

TB_Node* Tc_GenIdent_(Tc_Context* ctx, Ast_IdentExpr* expr)
{
    // Procedure
    if(expr->declaration->kind == AstKind_ProcDef)
    {
        auto procDef = (Ast_ProcDef*)expr->declaration;
        auto address = tb_inst_get_symbol_address(ctx->proc, (TB_Symbol*)procDef->tildeProc);
        
        return address;
    }
    
    // Variable declaration (global or local)
    if(ctx->needValue)
        return tb_inst_load(ctx->proc, Tc_ConvertToTildeType(expr->type), expr->declaration->tildeNode, GetTypeAlign(expr->type), false);
    else
        return expr->declaration->tildeNode;
}

// Only works for single return value
TB_Node* Tc_GenFuncCall_(Tc_Context* ctx, Ast_FuncCall* call)
{
    auto targetNode = Tc_GenNode_(ctx, call->target);
    
    ScratchArena scratch;
    auto argNodes = Arena_AllocArray(scratch, call->args.length, TB_Node*);
    for_array(i, call->args)
    {
        argNodes[i] = Tc_GenNode_(ctx, call->args[i]);
    }
    
    Assert(call->target->type->typeId == Typeid_Proc);
    
    auto debugProto = Tc_ConvertProcToDebugType(ctx->module, call->target->type);
    auto proto = tb_prototype_from_dbg(ctx->module, debugProto);
    
    TB_MultiOutput output = tb_inst_call(ctx->proc, proto, targetNode, call->args.length, argNodes);
    
    if(output.count > 1) return output.multiple[0];
    else return output.single;
    
    //TB_DataType dataType = { { TB_INT, 4, 32 } };
    //auto node = tb_inst_sint(ctx->proc, dataType, 2);
    //return node;
}

TB_Node* Tc_GenBinExpr_(Tc_Context* ctx, Ast_BinaryExpr* expr)
{
    if(expr->op == '=')
        ctx->needValue = false;
    TB_Node* lhs = Tc_GenNode_(ctx, expr->lhs);
    ctx->needValue = true;
    TB_Node* rhs = Tc_GenNode_(ctx, expr->rhs);
    ctx->needValue = true;
    
    Assert(lhs && rhs);
    
    // @temporary assuming integer operands
    TB_Node* result = 0;
    switch(expr->op)
    {
        case '=':
        {
            tb_inst_store(ctx->proc, Tc_ConvertToTildeType(expr->lhs->type), lhs, rhs, GetTypeAlign(expr->lhs->type), false);
            result = lhs;
            break;
        }
        case '+': result = tb_inst_add(ctx->proc, lhs, rhs, TB_ARITHMATIC_NONE); break;
        case '-': result = tb_inst_sub(ctx->proc, lhs, rhs, TB_ARITHMATIC_NONE); break;
        case '*': result = tb_inst_mul(ctx->proc, lhs, rhs, TB_ARITHMATIC_NONE); break;
        case '/': result = tb_inst_div(ctx->proc, lhs, rhs, true);               break;
    }
    
    return result;
}

TB_Node* Tc_GenUnaryExpr_(Tc_Context* ctx, Ast_UnaryExpr* expr) {return 0;}
TB_Node* Tc_GenTypecast_(Tc_Context* ctx, Ast_Typecast* expr) { return Tc_GenNode_(ctx, expr); }
TB_Node* Tc_GenSubscript_(Tc_Context* ctx, Ast_Subscript* expr) {return 0;}
TB_Node* Tc_GenMemberAccess_(Tc_Context* ctx, Ast_MemberAccess* expr) {return 0;}

TB_DataType Tc_ConvertToTildeType(TypeInfo* type)
{
    TB_DataType dt = { { 0, 0, 0 } };
    
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

TB_DebugType* Tc_ConvertToDebugType(TB_Module* module, TypeInfo* type)
{
    // Primitive types
    
    if(type->typeId == Typeid_None) return tb_debug_get_void(module);
    if(type->typeId == Typeid_Bool) return tb_debug_get_bool(module);
    
    if(IsTypeIntegral(type))
    {
        bool isSigned = IsTypeSigned(type);
        int bits = GetTypeSizeBits(type);
        return tb_debug_get_integer(module, isSigned, bits);
    }
    
    if(type->typeId == Typeid_Float)  return tb_debug_get_float(module, TB_FLT_32);
    if(type->typeId == Typeid_Double) return tb_debug_get_float(module, TB_FLT_64);
    
    // Compound types
    
    if(type->typeId == Typeid_Ptr)
    {
        auto base = Tc_ConvertToDebugType(module, ((Ast_DeclaratorPtr*)type)->baseType);
        return tb_debug_create_ptr(module, base);
    }
    
    if(type->typeId == Typeid_Arr)
    {
        // @temporary All arrays are size 5 for now
        auto base = Tc_ConvertToDebugType(module, ((Ast_DeclaratorArr*)type)->baseType);
        return tb_debug_create_array(module, base, 5);
    }
    
    TB_DebugType* structType = 0;
    Ast_DeclaratorStruct* astType = 0;
    
    // TODO: Support unions
    if(type->typeId == Typeid_Struct)
    {
        astType = (Ast_DeclaratorStruct*)type;
        structType = tb_debug_create_struct(module, -1, "");
    }
    else if(type->typeId == Typeid_Ident)
    {
        auto identType = (Ast_DeclaratorIdent*)type;
        auto structDef = identType->structDef;
        astType = Ast_GetDeclStruct(structDef);
        structType = tb_debug_create_struct(module, -1, structDef->name->string);
    }
    
    if(structType)
    {
        // Fill in the struct type
        TB_DebugType** types = tb_debug_record_begin(module, structType, astType->memberTypes.length);
        for_array(i, astType->memberTypes)
        {
            auto fillWith = Tc_ConvertToDebugType(module, astType->memberTypes[i]); 
            types[i] = tb_debug_create_field(module, fillWith, -1, astType->memberNames[i]->string, astType->memberOffsets[i]);
        }
        
        tb_debug_record_end(structType, GetTypeSize(astType), GetTypeAlign(astType));
        return structType;
    }
    
    if(type->typeId == Typeid_Proc) return Tc_ConvertProcToDebugType(module, type);
    
    Assert(false && "Not implemented");
    return 0;
}

TB_DebugType* Tc_ConvertProcToDebugType(TB_Module* module, TypeInfo* type)
{
    Assert(type->typeId == Typeid_Proc);
    ScratchArena scratch;
    
    auto procDecl = (Ast_DeclaratorProc*)type;
    int numArgs = procDecl->args.length + procDecl->retTypes.length - 1;
    TB_DebugType* procType = tb_debug_create_func(module, TB_CDECL, numArgs, 1, false);
    TB_DebugType** paramTypesToFill = tb_debug_func_params(procType);
    TB_DebugType** returnTypesToFill = tb_debug_func_returns(procType);
    
    // Pass all returns except for the last one as args
    for(int i = 0; i < procDecl->retTypes.length - 1; ++i)
    {
        // Make temporary pointer type for passing indirectly
        auto type = Typer_MakePtr(scratch, procDecl->retTypes[i]);
        
        auto debugType = Tc_ConvertToDebugType(module, type);
        // Return arguments don't have names
        paramTypesToFill[i] = tb_debug_create_field(module, debugType, -1, "ret", 0);
    }
    
    for_array(i, procDecl->args)
    {
        int j = i + procDecl->retTypes.length - 1;
        
        TypeInfo* curType = procDecl->args[i]->type;
        auto debugType = Tc_ConvertToDebugType(module, curType);
        // Proc field offset is ignored, so it can be 0
        paramTypesToFill[j] = tb_debug_create_field(module, debugType, -1,
                                                    procDecl->args[i]->name->string, 0);
    }
    
    auto retDebugType = Tc_ConvertToDebugType(module, procDecl->retTypes.last());
    returnTypesToFill[0] = retDebugType;
    
    return procType;
}
