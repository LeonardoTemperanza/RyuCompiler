
// NOTE(Leo): This file is used for unity builds (which
// is the approach I commonly use). The source
// code can still be used in a non-unity build
// system, as there is a distinction between .cpp
// and .h files.

#define UnityBuild

#include "os/os_generic.cpp"

#include "base.cpp"
#include "messages.cpp"
#include "main.cpp"
#include "lexer.cpp"
#include "memory_management.cpp"
#include "parser.cpp"
#include "semantics.cpp"
#include "dependency_graph.cpp"
//#include "tilde_codegen.cpp"
//#include "bytecode_builder.cpp"
//#include "interpreter.cpp"

#include "tilde_codegen_from_ast.h"
