
#include "tilde_codegen_from_ast.h"
#include "ast.h"

CG_Ctx CG_Init()
{
    CG_Ctx ctx = {0};
    
    // Create module
    // TODO: Add target options
    tb_module_create_for_host(false);
}

void CG_Cleanup(CG_Ctx* ctx)
{
    tb_module_destroy(ctx->module);
    
    ctx->module = nullptr;
}

void Codegen(Interp* interp, Dg_Entity* entity)
{
    ProfileFunc(prof);
}

void CG_Proc(Ast_ProcDef* proc)
{
}
