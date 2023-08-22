
#pragma once

#include "base.h"
#include "tilde_backend/Cuik/common/arena.h"
#include "tb.h"

struct Tc_Context
{
    TB_Module* module = 0;
    TB_Function* proc = 0;
    TB_Arena procArena;
    
    TB_Function* mainProc = 0;
    bool emitAsm = false;
    
    // Need address if false
    bool needValue = true;
    
    // Stuff for generation from bytecode
    Arena* regArena = 0;
    Arena* symArena = 0;
    Arena* flagArena = 0;
    Array<TB_Node*> regs;
    Array<bool> isLValue;
    Array<Interp_Symbol*> syms;
    Array<TB_Node*> bbs;  // Basic Blocks
    TB_PassingRule retPassingRule;
    Array<TB_PassingRule> argPassingRules;
};

Tc_Context Tc_InitCtx(TB_Module* module, bool emitAsm);
void Tc_ResetCtx(Tc_Context* ctx);

void Tc_TestCode(Ast_FileScope* file, Interp* interp);

// From bytecode
TB_DataType Tc_ToTBType(Interp_Type type);
void Tc_ExpandRegs(Tc_Context* ctx, int idx);
void Tc_GenProc(Tc_Context* ctx, Interp_Proc* proc);
void Tc_GenInstrs(Tc_Context* ctx, TB_Function* tildeProc, Interp_Proc* proc);

// From ast (@temporary , to remove later)
TB_Node* Tc_GenNode_(Tc_Context* ctx, Ast_Node* node);
void Tc_GenProcDef_(Tc_Context* ctx, Ast_ProcDef* proc);
void Tc_GenStructDef_(Tc_Context* ctx, Ast_StructDef* structDef);
TB_Node* Tc_GenVarDecl_(Tc_Context* ctx, Ast_VarDecl* decl);
void Tc_GenBlock_(Tc_Context* ctx, Ast_Block* block);
void Tc_GenIf_(Tc_Context* ctx, Ast_If* stmt);
void Tc_GenFor_(Tc_Context* ctx, Ast_For* stmt);
void Tc_GenWhile_(Tc_Context* ctx, Ast_While* stmt);
void Tc_GenDoWhile_(Tc_Context* ctx, Ast_DoWhile* stmt);
void Tc_GenSwitch_(Tc_Context* ctx, Ast_Switch* stmt);
void Tc_GenDefer_(Tc_Context* ctx, Ast_Defer* stmt);
void Tc_GenReturn_(Tc_Context* ctx, Ast_Return* stmt);
void Tc_GenBreak_(Tc_Context* ctx, Ast_Break* stmt);
void Tc_GenContinue_(Tc_Context* ctx, Ast_Continue* stmt);
TB_Node* Tc_GenNumLiteral_(Tc_Context* ctx, Ast_NumLiteral* expr);
TB_Node* Tc_GenIdent_(Tc_Context* ctx, Ast_IdentExpr* expr);
TB_Node* Tc_GenFuncCall_(Tc_Context* ctx, Ast_FuncCall* call);
TB_Node* Tc_GenBinExpr_(Tc_Context* ctx, Ast_BinaryExpr* expr);
TB_Node* Tc_GenUnaryExpr_(Tc_Context* ctx, Ast_UnaryExpr* expr);
TB_Node* Tc_GenTypecast_(Tc_Context* ctx, Ast_Typecast* expr);
TB_Node* Tc_GenSubscript_(Tc_Context* ctx, Ast_Subscript* expr);
TB_Node* Tc_GenMemberAccess_(Tc_Context* ctx, Ast_MemberAccess* expr);

TB_DataType Tc_ConvertToTildeType(TypeInfo* type);
TB_DebugType* Tc_ConvertToDebugType(TB_Module* module, TypeInfo* type);
TB_DebugType* Tc_ConvertProcToDebugType(TB_Module* module, TypeInfo* type);
