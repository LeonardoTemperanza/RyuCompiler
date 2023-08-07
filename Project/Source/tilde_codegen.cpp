
#include "base.h"
#include "tilde_codegen.h"

void Tc_TestCode(Ast_FileScope* file)
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
    
    Tc_Context ctx;
    ctx.module = module;
    ctx.emitAsm = true;
    
    for_array(i, file->scope.stmts)
    {
        auto stmt = file->scope.stmts[i];
        if(stmt->kind == AstKind_ProcDef)
            Tc_GenProcDef(&ctx, (Ast_ProcDef*)stmt);
        else if(stmt->kind == AstKind_StructDef)
            Tc_GenStructDef(&ctx, (Ast_StructDef*)stmt);
        else
            Assert(false);
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
        // does not seem to work at this moment. (It generates
        // wrong executables)
        
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
}

TB_Node* Tc_GenNode(Tc_Context* ctx, Ast_Node* node)
{
    switch(node->kind)
    {
        case AstKind_ProcDef:       break;  // Not handled here
        case AstKind_StructDef:     break;  // Not handled here
        case AstKind_DeclBegin:     break;
        case AstKind_VarDecl:       return Tc_GenVarDecl(ctx, (Ast_VarDecl*)node); break;
        case AstKind_EnumDecl:      printf("Enumerator not implemented.\n"); break;
        case AstKind_DeclEnd:       break;
        case AstKind_StmtBegin:     break;
        case AstKind_Block:         Tc_GenBlock(ctx, (Ast_Block*)node); break;
        case AstKind_If:            Tc_GenIf(ctx, (Ast_If*)node); break;
        case AstKind_For:           Tc_GenFor(ctx, (Ast_For*)node); break;
        case AstKind_While:         Tc_GenWhile(ctx, (Ast_While*)node); break;
        case AstKind_DoWhile:       Tc_GenDoWhile(ctx, (Ast_DoWhile*)node); break;
        case AstKind_Switch:        Tc_GenSwitch(ctx, (Ast_Switch*)node); break;
        case AstKind_Defer:         Tc_GenDefer(ctx, (Ast_Defer*)node); break;
        case AstKind_Return:        Tc_GenReturn(ctx, (Ast_Return*)node); break;
        case AstKind_Break:         Tc_GenBreak(ctx, (Ast_Break*)node); break;
        case AstKind_Continue:      Tc_GenContinue(ctx, (Ast_Continue*)node); break;
        case AstKind_EmptyStmt:     break;
        case AstKind_StmtEnd:       break;
        case AstKind_ExprBegin:     break;
        case AstKind_Literal:       printf("Literal not implemented.\n"); break;
        case AstKind_NumLiteral:    return Tc_GenNumLiteral(ctx, (Ast_NumLiteral*)node); break;
        case AstKind_Ident:         return Tc_GenIdent(ctx, (Ast_IdentExpr*)node); break;
        case AstKind_FuncCall:      return Tc_GenFuncCall(ctx, (Ast_FuncCall*)node); break;
        case AstKind_BinaryExpr:    return Tc_GenBinExpr(ctx, (Ast_BinaryExpr*)node); break;
        case AstKind_UnaryExpr:     return Tc_GenUnaryExpr(ctx, (Ast_UnaryExpr*)node); break;
        case AstKind_TernaryExpr:   printf("Ternary expression not implemented.\n"); break;
        case AstKind_Typecast:      return Tc_GenNode(ctx, ((Ast_Typecast*)node)->expr);
        case AstKind_Subscript:     return Tc_GenSubscript(ctx, (Ast_Subscript*)node); break;
        case AstKind_MemberAccess:  return Tc_GenMemberAccess(ctx, (Ast_MemberAccess*)node); break;
        case AstKind_ExprEnd:       break;
        default: Assert(false && "Enum value out of bounds");
    }
    
    return 0;
}

void Tc_GenProcDef(Tc_Context* ctx, Ast_ProcDef* proc)
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
    Tc_GenBlock(ctx, (Ast_Block*)block);
    
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

void Tc_GenStructDef(Tc_Context* ctx, Ast_StructDef* structDef)
{
    
}

TB_Node* Tc_GenVarDecl(Tc_Context* ctx, Ast_VarDecl* decl)
{
    TB_Node* node = tb_inst_local(ctx->proc, GetTypeSize(decl->type), GetTypeAlign(decl->type));
    decl->tildeNode = node;
    
    if(decl->initExpr)
    {
        auto exprNode = Tc_GenNode(ctx, decl->initExpr);
        tb_inst_store(ctx->proc, Tc_ConvertToTildeType(decl->type), node, exprNode, GetTypeAlign(decl->type), false);
    }
    
    return node;
}

// This will also handle defer stuff i think
void Tc_GenBlock(Tc_Context* ctx, Ast_Block* block)
{
    for_array(i, block->stmts)
        Tc_GenNode(ctx, block->stmts[i]);
    
    // If there was a defer in the block, ...
}

void Tc_GenIf(Tc_Context* ctx, Ast_If* stmt)
{
    TB_Node* thenNode = tb_inst_region(ctx->proc);
    TB_Node* endNode  = tb_inst_region(ctx->proc);
    TB_Node* elseNode = endNode;
    if(stmt->elseStmt)
        elseNode = tb_inst_region(ctx->proc);
    
    auto condNode = Tc_GenNode(ctx, stmt->condition);
    tb_inst_if(ctx->proc, condNode, thenNode, elseNode);
    
    // Then
    tb_inst_set_control(ctx->proc, thenNode);
    Tc_GenBlock(ctx, stmt->thenBlock);
    tb_inst_goto(ctx->proc, endNode);
    
    // Else
    if(stmt->elseStmt)
    {
        tb_inst_set_control(ctx->proc, elseNode);
        Tc_GenNode(ctx, stmt->elseStmt);
        tb_inst_goto(ctx->proc, endNode);
    }
    
    tb_inst_set_control(ctx->proc, endNode);
}

void Tc_GenFor(Tc_Context* ctx, Ast_For* stmt) {}
void Tc_GenWhile(Tc_Context* ctx, Ast_While* stmt) {}
void Tc_GenDoWhile(Tc_Context* ctx, Ast_DoWhile* smt) {}
void Tc_GenCheckDoWhile(Tc_Context* ctx, Ast_DoWhile* stmt) {}
void Tc_GenSwitch(Tc_Context* ctx, Ast_Switch* stmt) {}
void Tc_GenDefer(Tc_Context* ctx, Ast_Defer* stmt) {}

void Tc_GenReturn(Tc_Context* ctx, Ast_Return* stmt)
{
    TB_Node* node = Tc_GenNode(ctx, stmt->expr);
    Assert(node);
    
    tb_inst_ret(ctx->proc, 1, &node);
}

void Tc_GenBreak(Tc_Context* ctx, Ast_Break* stmt) {}
void Tc_GenContinue(Tc_Context* ctx, Ast_Continue* stmt) {}
TB_Node* Tc_GenNumLiteral(Tc_Context* ctx, Ast_NumLiteral* expr)
{
    TB_DataType dataType = { { TB_INT, 4, 32 } };
    auto node = tb_inst_sint(ctx->proc, dataType, 2);
    return node;
}

TB_Node* Tc_GenIdent(Tc_Context* ctx, Ast_IdentExpr* expr)
{
    // Procedure
    if(expr->declaration->kind == AstKind_ProcDef)
    {
        auto procDef = (Ast_ProcDef*)expr->declaration;
        auto address = tb_inst_get_symbol_address(ctx->proc, (TB_Symbol*)procDef->tildeProc);
        
        return address;
        return tb_inst_load(ctx->proc, Tc_ConvertToTildeType(procDef->type), address, GetTypeAlign(procDef->type), false);
    }
    
    // Variable declaration (global or local)
    if(ctx->needValue)
        return tb_inst_load(ctx->proc, Tc_ConvertToTildeType(expr->type), expr->declaration->tildeNode, GetTypeAlign(expr->type), false);
    else
        return expr->declaration->tildeNode;
}

// Only works for single return value
TB_Node* Tc_GenFuncCall(Tc_Context* ctx, Ast_FuncCall* call)
{
    auto targetNode = Tc_GenNode(ctx, call->target);
    
    ScratchArena scratch;
    auto argNodes = Arena_AllocArray(scratch, call->args.length, TB_Node*);
    for_array(i, call->args)
    {
        argNodes[i] = Tc_GenNode(ctx, call->args[i]);
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

TB_Node* Tc_GenBinExpr(Tc_Context* ctx, Ast_BinaryExpr* expr)
{
    if(expr->op == '=')
        ctx->needValue = false;
    TB_Node* lhs = Tc_GenNode(ctx, expr->lhs);
    ctx->needValue = true;
    TB_Node* rhs = Tc_GenNode(ctx, expr->rhs);
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

TB_Node* Tc_GenUnaryExpr(Tc_Context* ctx, Ast_UnaryExpr* expr) {return 0;}
TB_Node* Tc_GenTypecast(Tc_Context* ctx, Ast_Typecast* expr) { return Tc_GenNode(ctx, expr); }
TB_Node* Tc_GenSubscript(Tc_Context* ctx, Ast_Subscript* expr) {return 0;}
TB_Node* Tc_GenMemberAccess(Tc_Context* ctx, Ast_MemberAccess* expr) {return 0;}

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
    
    if(type->typeId == Typeid_Struct)
    {
        Assert(false && "Not supported yet");
        //auto structType = tb_debug_create_struct(module
    }
    
    if(type->typeId == Typeid_Ident)
    {
        Assert(false && "Not implemented yet");
        
#if 0
        auto identType = (Ast_DeclaratorIdent*) type;
        auto structDef = identType->structDef;
        auto structType = Ast_GetDeclStruct(structDef);
        
        TB_DebugType* structTypeTB = tb_debug_create_struct(module, structDef->size, structDef->name->string);
        
        // Fill in the struct type
        TB_DebugType** types = tb_debug_record_begin(structTypeTB, structType->memberTypes.length);
        for_array(i, structType->memberTypes.length)
        {
            auto fillWith = Tc_ConvertToDebugType(module, structType->memberTypes[i]); 
            int size = GetTypeSize(structType->memberTypes[i]);
            // @incomplete
            types[i] = tb_debug_create_field(module, fillWith, size, structType->memberNames->string, 0);
        }
        
        tb_debug_record_end(structTypeTB, structDef->size, structDef->align);
#endif
    }
    
    if(type->typeId == Typeid_Proc) return Tc_ConvertProcToDebugType(module, type);
    
    return 0;
}

TB_DebugType* Tc_ConvertProcToDebugType(TB_Module* module, TypeInfo* type)
{
    Assert(type->typeId == Typeid_Proc);
    
    auto procDecl = (Ast_DeclaratorProc*)type;
    TB_DebugType* procType = tb_debug_create_func(module, TB_CDECL, procDecl->args.length, 1, false);
    TB_DebugType** paramTypesToFill = tb_debug_func_params(procType);
    TB_DebugType** returnTypesToFill = tb_debug_func_returns(procType);
    for(int i = 0; i < procDecl->args.length; ++i)
    {
        TypeInfo* curType = procDecl->args[i]->type;
        auto debugType = Tc_ConvertToDebugType(module, curType);
        paramTypesToFill[i] = tb_debug_create_field(module, debugType, strlen(procDecl->args[i]->name->string),
                                                    procDecl->args[i]->name->string, 0);
    }
    
    auto retDebugType = Tc_ConvertToDebugType(module, procDecl->retTypes[0]);
    returnTypesToFill[0] = retDebugType;
    
    return procType;
}
