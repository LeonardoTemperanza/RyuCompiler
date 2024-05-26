
#pragma once

#include "base.h"
#include "lexer.h"

////////
// Compilation messages

enum MessageKind
{
    MsgKind_None = 0,  // Sentinel value, used for "null" messages
    MsgKind_Error,
    MsgKind_Warning,
    MsgKind_Log
};

enum MessagePhase
{
    MsgPhase_Syntax = 0,
    MsgPhase_Semantics
};

enum MessageContentKind
{
    Msg_None = 0,
    Msg_UnexpectedToken,
    Msg_IncompatibleTypes,
    Msg_IncompatibleReturns,
    Msg_CannotDereference,
    Msg_CannotConvertToScalar,
    Msg_CannotConvertToIntegral,
    Msg_LookAtProcDecl,
    Msg_SpecializedMessage
};

struct MessageContent
{
    MessageContentKind kind;
    String message;
    TokenKind token;
};

struct Message
{
    MessageKind kind;
    MessagePhase phase;
    
    Token* token;
    String path;
    String fileContents;
    
    MessageContent content;
    
    // N-ary tree. Children are nodes that continue
    // this message. Things like:
    // 1: "incorrect number of arguments in function call"
    // 2 (child of 1): "Here's the procedure call signature"
    // It's possible to have multiple levels of nesting,
    // though i'm not sure if that would be any useful
    Message* first;
    Message* parent;
    Message* next;
    Message* prev;
};

struct MessageDrawCmd
{
    bool showFileName;
    //others
};

Message MakeMessage(MessageKind kind, MessagePhase phase, String path, String fileContents,
                    Token* at, MessageContent content);
Message MakeMessage(MessageKind kind, MessagePhase phase, String path, String fileContents,
                    Token* at, MessageContent content, Message* parent);

Message* AppendMessage(Message msg, Arena* dst);

// Message renderer
void SortMessages();
void ShowMessages();
void ShowMessages(Message* message, MessageDrawCmd cmd);

////////
// Command line arguments

// NOTE:
// Usage: Just use the global variable "args".
// Its members are named just the way it's specified by
// the CmdLineArgsInfo macro.

// Var name, string representation, type, default value, description
// This is an X-macro. For more info: https://en.wikipedia.org/wiki/X_macro
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

// Should be set at the start of main.
// Can be used in any module.
CmdLineArgs args;

// minCol and maxCol is the range of column indices within which to print the description.
// minCol should be where to start and maxCol should be the width of the terminal
template<typename t>
void PrintArg(char* argName, char* desc, t defVal, int lPad, int rPad, int minCol, int maxCol, bool printDesc);

// Only specializations can be used
template<typename t> int PrintArgAttributes(t defVal)  { static_assert(false, "Unknown command line argument type"); }
template<> int PrintArgAttributes<int>(int defVal)     { return printf("<int value> (default: %d)", defVal); }
template<> int PrintArgAttributes<bool>(bool defVal)   { return 0; }
template<> int PrintArgAttributes<char*>(char* defVal) { return printf("<string value> (default: %s)", defVal); }
template<typename t> int ArgAttributes(t defVal)  { static_assert(false, "Unknown command line argument type"); }
template<> int ArgAttributes<int>(int defVal)     { return snprintf(nullptr, 0, "<int value> (default: %d)", defVal); }
template<> int ArgAttributes<bool>(bool defVal)   { return 0; }
template<> int ArgAttributes<char*>(char* defVal) { return snprintf(nullptr, 0, "<string value> (default: %s)", defVal); }

void PrintHelp();
void PrintTimeBenchmark(double frontend, double irGen, double backend, double linker);
