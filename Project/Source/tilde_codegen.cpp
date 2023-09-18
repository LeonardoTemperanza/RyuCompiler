
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
    
    Tc_Context ctx = Tc_InitCtx(module, false);
    
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
    
    int numFuncs = tb_module_get_function_count(module);
    printf("Num funcs: %d\n", numFuncs);
    
    if(!ctx.mainProc)
    {
        printf("No main procedure was found.\n");
        return;
    }
    
    // Linking
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
        LaunchPlatformSpecificLinker();
    }
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
    
    for(int i = 0; i < proc->instrs.length; ++i)
    {
        auto& instr = proc->instrs[i];
        
        // Ignore the argument locals (and stores) because TB automatically inserts those
        if(instr.bitfield & InstrBF_SetUpArgs) continue;
        
        Tc_ExpandRegs(ctx, instr.dst);
        
        auto& dst = regs[instr.dst];
        auto& unarySrc = regs[instr.unary.src];
        auto& src1 = regs[instr.bin.src1];
        auto& src2 = regs[instr.bin.src2];
        auto unaryType = Tc_ToTBType(instr.unary.type);
        
        switch(instr.op)
        {
            case Op_Null: break;
            case Op_IntegerConst:
            {
                dst = tb_inst_sint(tildeProc,
                                   Tc_ToTBType(instr.imm.type),
                                   instr.imm.intVal);
                break;
            }
            case Op_Float32Const: dst = tb_inst_float32(tildeProc, instr.imm.floatVal); break;
            case Op_Float64Const: dst = tb_inst_float64(tildeProc, instr.imm.doubleVal); break;
            case Op_Region:       tb_inst_set_control(tildeProc, bbs[i]); break;
            case Op_Call:
            {
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
                    dst = outputs.single;
                }
                else  // 0 or >= 2
                    Assert(false);
                
                break;
            }
            case Op_SysCall: break;
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
                    tb_inst_ret(tildeProc, 1, &unarySrc);
                }
                else
                    tb_inst_ret(tildeProc, 0, 0);
                
            }
            case Op_Load:
            {
                dst = tb_inst_load(tildeProc,
                                   Tc_ToTBType(instr.load.type),
                                   regs[instr.load.addr],
                                   instr.load.align,
                                   false);
                break;
            }
            case Op_Local: dst = tb_inst_local(tildeProc, instr.local.size, instr.local.align); break;
            case Op_GetSymbolAddress:
            {
                auto tildeSymbol = (TB_Symbol*)instr.symAddress.symbol->tildeProc;
                dst = tb_inst_get_symbol_address(tildeProc, tildeSymbol);
                ctx->syms[instr.dst] = instr.symAddress.symbol;
                
                break;
            }
            case Op_MemberAccess: break;
            case Op_ArrayAccess: break;
            case Op_Truncate: dst = tb_inst_trunc(tildeProc, unarySrc, unaryType); break;
            case Op_FloatExt: dst = tb_inst_fpxt(tildeProc, unarySrc, unaryType); break;
            case Op_SignExt: dst = tb_inst_sxt(tildeProc, unarySrc, unaryType); break;
            case Op_ZeroExt: dst = tb_inst_zxt(tildeProc, unarySrc, unaryType); break;
            case Op_Int2Ptr: dst = tb_inst_int2ptr(tildeProc, unarySrc); break;
            case Op_Uint2Float: dst = tb_inst_int2float(tildeProc, unarySrc, unaryType, false); break;
            case Op_Float2Uint: dst = tb_inst_float2int(tildeProc, unarySrc, unaryType, false); break;
            case Op_Int2Float: dst = tb_inst_int2float(tildeProc, unarySrc, unaryType, true); break;
            case Op_Float2Int: dst = tb_inst_float2int(tildeProc, unarySrc, unaryType, true); break;
            case Op_Bitcast: dst = tb_inst_bitcast(tildeProc, unarySrc, unaryType); break;
            case Op_Select: break;
            case Op_Not: dst = tb_inst_not(tildeProc, unarySrc); break;
            case Op_Negate: dst = tb_inst_neg(tildeProc, unarySrc); break;
            case Op_And: dst = tb_inst_and(tildeProc, src1, src2); break;
            case Op_Or: dst = tb_inst_or(tildeProc, src1, src2); break;
            case Op_Xor: dst = tb_inst_xor(tildeProc, src1, src2); break;
            case Op_Add: dst = tb_inst_add(tildeProc, src1, src2, TB_ARITHMATIC_NONE); break;
            case Op_Sub: dst = tb_inst_sub(tildeProc, src1, src2, TB_ARITHMATIC_NONE); break;
            case Op_Mul: dst = tb_inst_mul(tildeProc, src1, src2, TB_ARITHMATIC_NONE); break;
            case Op_ShL: dst = tb_inst_shl(tildeProc, src1, src2, TB_ARITHMATIC_NONE); break;
            case Op_ShR: dst = tb_inst_shr(tildeProc, src1, src2); break;
            case Op_Sar: dst = tb_inst_sar(tildeProc, src1, src2); break;
            case Op_Rol: dst = tb_inst_rol(tildeProc, src1, src2); break;
            case Op_Ror: dst = tb_inst_ror(tildeProc, src1, src2); break;
            case Op_UDiv: dst = tb_inst_div(tildeProc, src1, src2, false); break;
            case Op_SDiv: dst = tb_inst_div(tildeProc, src1, src2, true); break;
            case Op_UMod: dst = tb_inst_mod(tildeProc, src1, src2, false); break;
            case Op_SMod: dst = tb_inst_mod(tildeProc, src1, src2, true); break;
            case Op_FAdd: dst = tb_inst_fadd(tildeProc, src1, src2); break;
            case Op_FSub: dst = tb_inst_fsub(tildeProc, src1, src2); break;
            case Op_FMul: dst = tb_inst_fmul(tildeProc, src1, src2); break;
            case Op_FDiv: dst = tb_inst_fdiv(tildeProc, src1, src2); break;
            case Op_CmpEq: dst = tb_inst_cmp_eq(tildeProc, src1, src2); break;
            case Op_CmpNe: dst = tb_inst_cmp_ne(tildeProc, src1, src2); break;
            case Op_CmpULT: dst = tb_inst_cmp_ilt(tildeProc, src1, src2, false); break;
            case Op_CmpULE: dst = tb_inst_cmp_ile(tildeProc, src1, src2, false); break;
            case Op_CmpSLT: dst = tb_inst_cmp_ilt(tildeProc, src1, src2, true); break;
            case Op_CmpSLE: dst = tb_inst_cmp_ile(tildeProc, src1, src2, true); break;
            case Op_CmpFLT: dst = tb_inst_cmp_flt(tildeProc, src1, src2); break;
            case Op_CmpFLE: dst = tb_inst_cmp_fle(tildeProc, src1, src2); break;
        }
    }
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
