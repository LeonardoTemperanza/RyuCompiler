
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
    FILE* outputAsmFile = 0;
};

void Tc_TestCode(Ast_FileScope* file);
TB_Node* Tc_GenNode(Tc_Context* ctx, Ast_Node* node);
void Tc_GenProcDef(Tc_Context* ctx, Ast_ProcDef* proc);
void Tc_GenStructDef(Tc_Context* ctx, Ast_StructDef* structDef);
TB_Node* Tc_GenVarDecl(Tc_Context* ctx, Ast_VarDecl* decl);
void Tc_GenBlock(Tc_Context* ctx, Ast_Block* block);
void Tc_GenIf(Tc_Context* ctx, Ast_If* stmt);
void Tc_GenFor(Tc_Context* ctx, Ast_For* stmt);
void Tc_GenWhile(Tc_Context* ctx, Ast_While* stmt);
void Tc_GenDoWhile(Tc_Context* ctx, Ast_DoWhile* stmt);
void Tc_GenSwitch(Tc_Context* ctx, Ast_Switch* stmt);
void Tc_GenDefer(Tc_Context* ctx, Ast_Defer* stmt);
void Tc_GenReturn(Tc_Context* ctx, Ast_Return* stmt);
void Tc_GenBreak(Tc_Context* ctx, Ast_Break* stmt);
void Tc_GenContinue(Tc_Context* ctx, Ast_Continue* stmt);
TB_Node* Tc_GenNumLiteral(Tc_Context* ctx, Ast_NumLiteral* expr);
TB_Node* Tc_GenIdent(Tc_Context* ctx, Ast_IdentExpr* expr);
TB_Node* Tc_GenFuncCall(Tc_Context* ctx, Ast_FuncCall* call);
TB_Node* Tc_GenBinExpr(Tc_Context* ctx, Ast_BinaryExpr* expr);
TB_Node* Tc_GenUnaryExpr(Tc_Context* ctx, Ast_UnaryExpr* expr);
TB_Node* Tc_GenTypecast(Tc_Context* ctx, Ast_Typecast* expr);
TB_Node* Tc_GenSubscript(Tc_Context* ctx, Ast_Subscript* expr);
TB_Node* Tc_GenMemberAccess(Tc_Context* ctx, Ast_MemberAccess* expr);

TB_DataType Tc_ConvertToTildeType(TypeInfo* type);
TB_DebugType* Tc_ConvertToDebugType(TB_Module* module, TypeInfo* type);
TB_DebugType* Tc_ConvertProcToDebugType(TB_Module* module, TypeInfo* type);
