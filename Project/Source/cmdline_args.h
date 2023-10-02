
#pragma once

#include "base.h"

// NOTE:
// Usage: Just use the global variable "cmdLineArgs".
// Its members are named just the way it's specified by
// the CmdLineArgsInfo macro.

// Var name, string representation, type, default value, description
#define CmdLineArgsInfo \
X(help,              "h",               bool,  false,        "Print description of available command line arguments") \
X(optLevel,          "O",               int,   0, \
"Optimization level, 0: disabled, 1: basic memory, control flow optimizations, 2: currently not available") \
X(useTildeLinker,    "tilde_linker",    bool,  false, \
"Use Tilde Linker (experimental) instead of the platform specific linker") \
X(emitBytecode,      "emit_bc",         bool,  false,        "Print bytecode used for interpretation (#run directives)") \
X(emitIr,            "emit_ir",         bool,  false,        "Print Intermediate Representation for the selected backend") \
X(emitAsm,           "emit_asm",        bool,  false,        "Print generated assembly code") \
X(debug,             "debug",           bool,  false,        "Generate debug information") \
X(time,              "time",            bool,  false, \
"Print information about the timing of the various phases of the compilation process") \
X(outputFile,        "o",               char*, "output.exe", "Desired path for the output file")

struct CmdLineArgs
{
#define X(varName, string, type, defaultVal, desc) type varName = defaultVal;
    CmdLineArgsInfo
#undef X
};