
#include "tilde_codegen_from_ast.h"

CG_Ctx CG_Init()
{
    CG_Ctx ctx = {0};
    
    // Create module
    // TODO: Add target options
    ctx.module = tb_module_create_for_host(false);
    
}

void CG_Cleanup(CG_Ctx* ctx)
{
    tb_module_destroy(ctx->module);
    
    ctx->module = nullptr;
}

// TODO: If we start using a smarter error system (and we should,
// because for example we'll have to order the error messages,
// which right now we don't do) then we probably won't need to have to return
// anything
void Codegen(Interp* interp, Dg_Entity* entity)
{
    ProfileFunc(prof);
}

