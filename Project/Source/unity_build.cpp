
// NOTE(Leo): This file is used for unity builds (which
// is the approach I commonly use). The source
// code can still be used in a non-unity build
// system, as there is a distinction between .cpp
// and .h files.

// Include appropriate platform layer file
#ifdef _WIN32
#include "os/os_windows.cpp"
#else
#ifdef __linux__
#include "os/os_linux.cpp"
#else
#error Unsupported platform
#endif
#endif

#include "base.cpp"

// This was a cool experiment,
// however let's continue with LLVM
// for now in order to get an idea of the structure
// of the front-end.
//#include "ir_generation.cpp"  // Ignore LLVM for now, we're just doing the type checking
//#include "llvm_ir_generation.cpp"
//#include "interpreter.cpp"
#include "lexer.cpp"
#include "main.cpp"
#include "memory_management.cpp"
#include "parser.cpp"
//#include "semantics.cpp"

#ifdef Debug
#include "debug.cpp"
#endif