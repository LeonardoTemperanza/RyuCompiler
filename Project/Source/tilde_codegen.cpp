
#include "base.h"
#include "interpreter.h"
#include "cmdline_args.h"

#include "tilde_codegen.h"

#ifndef UnityBuild
extern CmdLineArgs cmdLineArgs;
#endif

Tc_Context Tc_InitCtx(TB_Module* module, bool emitAsm)
{
    Tc_Context result;
    result.module = module;
    result.emitAsm = emitAsm;
    return result;
}

void Tc_CodegenAndLink(Ast_FileScope* file, Interp* interp, Slice<char*> objFiles)
{
    ScratchArena scratch;
    objFiles = objFiles.CopyToArena(scratch);
    
    // See TB_FeatureSet, this allows us to tell the code generator
    // what extensions are active in the platform, an example is enabling
    // AVX or BF16
    TB_FeatureSet featureSet = { 0 };
    TB_Arch arch  = TB_ARCH_X86_64;
    TB_System sys = TB_SYSTEM_WINDOWS;
    TB_DebugFormat debugFmt = TB_DEBUGFMT_NONE;
    TB_Module* module = tb_module_create(arch, sys, &featureSet, false);
    
    TB_ExecutableType exeType = TB_EXECUTABLE_PE;
    
    Tc_Context ctx = Tc_InitCtx(module, cmdLineArgs.emitAsm);
    
    Arena regArena = Arena_VirtualMemInit(MB(512), MB(2));
    Arena symArena = Arena_VirtualMemInit(MB(512), MB(2));
    Arena flagArena = Arena_VirtualMemInit(MB(512), MB(2));
    ctx.regArena = &regArena;
    ctx.symArena = &symArena;
    ctx.flagArena = &flagArena;
    
    // Generate symbols
    for_array(i, interp->symbols)
    {
        Tc_GenSymbol(&ctx, &interp->symbols[i]);
    }
    
    // Generate implementations
    for_array(i, interp->procs)
    {
        Tc_GenProc(&ctx, &interp->procs[i]);
    }
    
    if(!ctx.mainProc)
    {
        SetErrorColor();
        fprintf(stderr, "Error");
        ResetColor();
        fprintf(stderr, ": No main procedure was found.\n");
        return;
    }
    
    // Linking
    if(cmdLineArgs.useTildeLinker)  // Tilde backend linker
    {
        TB_Linker* linker = tb_linker_create(exeType, arch);
        defer(tb_linker_destroy(linker));
        
        tb_linker_append_module(linker, ctx.module);
        
        for_array(i, objFiles)
        {
            // Load object file into memory and feed it to the linker
            FILE* objContent = fopen(objFiles[i], "rb");
            if(!objContent)
            {
                SetErrorColor();
                fprintf(stderr, "Error");
                ResetColor();
                fprintf(stderr, ": Failed to open object file '%s', will be ignored.\n", objFiles[i]);
            }
            
            TB_Slice name = { strlen(objFiles[i]), (uint8*)objFiles[i] };
            TB_Slice content = { GetFileSize(objContent), (uint8*)objContent };
            tb_linker_append_object(linker, name, content);
        }
        
        // Append user defined object files to the module
        
        // C run-time library
        //tb_linker_append_library()
        
        tb_linker_set_entrypoint(linker, "main");
        
        TB_ExportBuffer buffer = tb_linker_export(linker);
        defer(tb_export_buffer_free(buffer));
        
        if(!tb_export_buffer_to_file(buffer, cmdLineArgs.outputFile))
        {
            SetErrorColor();
            fprintf(stderr, "Error");
            ResetColor();
            fprintf(stderr, ": Failed to export executable file.\n");
            return;
        }
    }
    else  // Platform specific linker
    {
        // Export object file
        TB_ExportBuffer buffer = tb_module_object_export(module, debugFmt);
        defer(tb_export_buffer_free(buffer));
        if(!tb_export_buffer_to_file(buffer, "output.o"))
        {
            SetErrorColor();
            fprintf(stderr, "Error");
            ResetColor();
            fprintf(stderr, ": Failed to export object file.\n");
            return;
        }
        
        // Add newly created object file
        objFiles.Append(scratch, "output.o");
        
        // Pass object file to linker
        RunPlatformSpecificLinker(cmdLineArgs.outputFile, objFiles.ptr, objFiles.length);
    }
}

void Tc_GenSymbol(Tc_Context* ctx, Interp_Symbol* symbol)
{
    TB_Symbol* res = 0;
    switch(symbol->type)
    {
        case Interp_ProcSym:
        {
            auto proc = (Ast_ProcDecl*)symbol->decl;
            
            TB_Function* tbProc = tb_function_create(ctx->module, -1, proc->symbol->name, TB_LINKAGE_PUBLIC, TB_COMDAT_NONE);
            
            res = (TB_Symbol*)tbProc;
            break;
        }
        case Interp_ExternSym:
        {
            auto decl = (Ast_Declaration*)symbol->decl;
            TB_External* tbExtern = tb_extern_create(ctx->module, -1, decl->name->string, TB_EXTERNAL_SO_EXPORT);
            
            res = (TB_Symbol*)tbExtern;
            break;
        }
        case Interp_GlobalSym:
        {
            auto global = (Ast_VarDecl*)symbol->decl;
            
            auto debugType = Tc_ConvertToDebugType(ctx->module, global->type);
            TB_Global* tbGlobal = tb_global_create(ctx->module, -1, global->symbol->name, debugType, TB_LINKAGE_PUBLIC);
            tb_global_set_storage(ctx->module, tb_module_get_data(ctx->module), tbGlobal, global->type->size, global->type->align, 1);
            
            res = (TB_Symbol*)tbGlobal;
            break;
        }
    }
    
    symbol->tildeSymbol = res;
}

void Tc_GenProc(Tc_Context* ctx, Interp_Proc* proc)
{
    ScratchArena scratch;
    
    // Generate procedure itself
    tb_arena_create(&ctx->procArena, MB(1));
    
    auto curProc = (TB_Function*)proc->symbol->tildeSymbol;
    
    // Debug type is still needed for debugging, but ABI is already handled
    // in the interpreter instructions.
    TB_DebugType* procType = Tc_ConvertProcToDebugType(ctx->module, proc->symbol->typeInfo);
    
    // @robustness Just doing this for now because it's easier, should
    // generate the prototype directly from the interp procedure
    size_t paramCount = 0;
    TB_Node** paramNodes = tb_function_set_prototype_from_dbg(curProc, procType, &ctx->procArena, &paramCount);
    
    Assert(proc->symbol->typeInfo->typeId == Typeid_Proc);
    auto procDecl = (Ast_ProcType*)proc->symbol->typeInfo;
    int abiArgsCount = (proc->retRule == TB_PASSING_INDIRECT) + max((int64)0, procDecl->retTypes.length - 1) + procDecl->args.length;
    if(procDecl->retTypes.length <= 0)
        abiArgsCount = procDecl->args.length;
    
    ctx->regs.Resize(ctx->regArena, abiArgsCount);
    for(int i = 0; i < abiArgsCount; ++i)
        ctx->regs[i] = tb_inst_param(curProc, i);
    
    // Populate arg addresses as well
    for(int i = 0; i < paramCount; ++i)
    {
        if(proc->argRules[i] == TB_PASSING_DIRECT)
        {
            Tc_ExpandRegs(ctx, i);
            ctx->regs[i + abiArgsCount] = paramNodes[i];
        }
    }
    
    bool isMain = strcmp(proc->symbol->name, "main") == 0;
    if(isMain)
        ctx->mainProc = curProc;
    ctx->proc = curProc;
    
    // Init arrays
    
    // Get all basic blocks
    ctx->bbs.ptr = Arena_AllocArray(scratch, proc->instrs.length, Tc_Region);
    ctx->bbs.length = proc->instrs.length;
    
    for_array(i, proc->instrs)
    {
        if(proc->instrs[i].op == Op_Region)
        {
            ctx->bbs[i].region = tb_inst_region(curProc);
            ctx->bbs[i].phiValue1 = 0;
            ctx->bbs[i].phiValue2 = 0;
            ctx->bbs[i].phiReg = RegIdx_Unused;
        }
        else
        {
            ctx->bbs[i] = { 0, 0, 0, RegIdx_Unused };
        }
    }
    
    // Generate its instructions
    Tc_GenInstrs(ctx, curProc, proc);
    
    // Perform optimization and codegen passes
    
    TB_Passes* p = tb_pass_enter(curProc, &ctx->procArena);
    if(cmdLineArgs.emitIr)
    {
        tb_pass_print(p);
        fflush(stdout);
    }
    
    if(cmdLineArgs.optLevel >= 1)
    {
        // @bug Peephole optimizations break tilde at the moment.
        //tb_pass_optimize(p);
        
        tb_pass_sroa(p);
        tb_pass_mem2reg(p);
    }
    
    TB_FunctionOutput* output = tb_pass_codegen(p, ctx->emitAsm);
    tb_pass_exit(p);
    
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
        nodes[i] = ctx->bbs[proc->instrArrays[arrayStart + i]].region;
    
    return nodes;
}

TB_SwitchEntry* Tc_GetSwitchEntries(Tc_Context* ctx, Interp_Proc* proc, Interp_Instr instr, Arena* allocTo)
{
    Assert(instr.op == Op_Branch);
    
    auto res = Arena_AllocArray(allocTo, instr.branch.count, TB_SwitchEntry);
    for(int i = 0; i < instr.branch.count; ++i)
    {
        res[i].key   = proc->constArrays[instr.branch.keyStart + i];
        res[i].value = ctx->bbs[proc->instrArrays[instr.branch.caseStart + i]].region;
    }
    
    return res;
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
            case Op_Null: Assert(false && "Unable to convert null operations to tb op"); break;
            case Op_IntegerConst:
            {
                dst = tb_inst_sint(tildeProc, Tc_ToTBType(instr.imm.type), instr.imm.intVal);
                break;
            }
            case Op_Float32Const: dst = tb_inst_float32(tildeProc, instr.imm.floatVal); break;
            case Op_Float64Const: dst = tb_inst_float64(tildeProc, instr.imm.doubleVal); break;
            case Op_Region:       tb_inst_set_control(tildeProc, bbs[i].region); ctx->lastRegion = i; break;
            case Op_Call:
            {
                auto symbol = syms[instr.call.target];
                
                auto debugProto = Tc_ConvertProcToDebugType(ctx->module, symbol->typeInfo);
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
                else if(outputs.count == 0)
                {
                    dst = 0;
                }
                else  // >= 2
                {
                    Assert(false);
                }
                
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
                               instr.memcpy.align);
                break;
            }
            case Op_MemSet:
            {
                tb_inst_memset(tildeProc, regs[instr.memset.dst],
                               regs[instr.memset.val], regs[instr.memset.count],
                               instr.memset.align);
                
                break;
            }
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
                    tb_inst_goto(tildeProc, bbs[instr.branch.defaultCase].region);
                else if(instr.branch.count == 1)
                {
                    auto value = regs[instr.branch.value];
                    auto conds = proc->constArrays[instr.branch.keyStart];
                    auto caseBB = bbs[proc->instrArrays[instr.branch.caseStart]].region;
                    tb_inst_if(tildeProc, value, caseBB, bbs[instr.branch.defaultCase].region);
                }
                else
                {
                    //Assert(false && "Switch codegen not implemented");
                    TB_SwitchEntry* entries = Tc_GetSwitchEntries(ctx, proc, instr, scratch);
                    
                    tb_inst_branch(tildeProc, entries[0].value->dt, regs[instr.branch.value], bbs[instr.branch.defaultCase].region, instr.branch.count, entries);
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
                
                break;
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
                auto tildeSymbol = (TB_Symbol*)instr.symAddress.symbol->tildeSymbol;
                dst = tb_inst_get_symbol_address(tildeProc, tildeSymbol);
                ctx->syms[instr.dst] = instr.symAddress.symbol;
                
                break;
            }
            case Op_MemberAccess: dst = tb_inst_member_access(tildeProc, regs[instr.memacc.base], instr.memacc.offset); break;
            case Op_ArrayAccess: break;
            case Op_Truncate: dst = tb_inst_trunc(tildeProc, unarySrc, unaryType); break;
            case Op_FloatExt: dst = tb_inst_fpxt(tildeProc, unarySrc, unaryType); break;
            case Op_SignExt: dst = tb_inst_sxt(tildeProc, unarySrc, unaryType); break;
            case Op_ZeroExt: dst = tb_inst_zxt(tildeProc, unarySrc, unaryType); break;
            case Op_Int2Ptr: dst = tb_inst_int2ptr(tildeProc, unarySrc); break;
            case Op_Ptr2Int: dst = tb_inst_ptr2int(tildeProc, unarySrc, unaryType); break;
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
        
        if(instr.bitfield & InstrBF_ViolatesSSA)
        {
            Assert(i != proc->instrs.length - 1);
            
            // Next instruction has to be branch
            auto& nextInstr = proc->instrs[i+1];
            Assert(nextInstr.op == Op_Branch);
            
            auto& destination = bbs[nextInstr.branch.defaultCase];
            Assert(!destination.phiValue1 || !destination.phiValue2);
            
            if(!destination.phiValue1)
                destination.phiValue1 = dst;
            else
                destination.phiValue2 = dst;
            
            destination.phiReg = instr.dst;
        }
        
        if(instr.bitfield & InstrBF_MergeSSA)
        {
            auto& region = bbs[ctx->lastRegion];
            Assert(region.phiValue1 && region.phiValue2);
            regs[region.phiReg] = tb_inst_phi2(tildeProc, region.region, region.phiValue1, region.phiValue2);
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
        bool isSigned = type->isSigned;
        int bits = type->size * 8;
        return tb_debug_get_integer(module, isSigned, bits);
    }
    
    if(type->typeId == Typeid_Float)
        return tb_debug_get_float(module, type->size == 4 ? TB_FLT_32 : TB_FLT_64);
    
    // Compound types
    
    if(type->typeId == Typeid_Ptr)
    {
        auto base = Tc_ConvertToDebugType(module, ((Ast_PtrType*)type)->baseType);
        return tb_debug_create_ptr(module, base);
    }
    
    if(type->typeId == Typeid_Arr)
    {
        // @temporary All arrays are size 5 for now
        auto base = Tc_ConvertToDebugType(module, ((Ast_ArrType*)type)->baseType);
        return tb_debug_create_array(module, base, 5);
    }
    
    TB_DebugType* structType = 0;
    Ast_StructType* astType = 0;
    
    // TODO: Support unions
    if(type->typeId == Typeid_Struct)
    {
        astType = (Ast_StructType*)type;
        structType = tb_debug_create_struct(module, -1, "");
    }
    else if(type->typeId == Typeid_Ident)
    {
        auto identType = (Ast_IdentType*)type;
        auto structDef = identType->structDef;
        astType = Ast_GetStructType(structDef);
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
        
        tb_debug_record_end(structType, astType->size, astType->align);
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
    
    auto procDecl = (Ast_ProcType*)type;
    int numArgs = procDecl->args.length + procDecl->retTypes.length - 1;
    if(procDecl->retTypes.length <= 0) numArgs = procDecl->args.length;
    int numTbRets = (procDecl->retTypes.length > 0);
    TB_DebugType* procType = tb_debug_create_func(module, TB_CDECL, numArgs, numTbRets, false);
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
        int j = max((int64)i, i + procDecl->retTypes.length - 1);
        
        TypeInfo* curType = procDecl->args[i]->type;
        auto debugType = Tc_ConvertToDebugType(module, curType);
        // Proc field offset is ignored, so it can be 0
        paramTypesToFill[j] = tb_debug_create_field(module, debugType, -1,
                                                    procDecl->args[i]->name->string, 0);
    }
    
    if(procDecl->retTypes.length > 0)
    {
        auto retDebugType = Tc_ConvertToDebugType(module, procDecl->retTypes.last());
        returnTypesToFill[0] = retDebugType;
    }
    
    return procType;
}
