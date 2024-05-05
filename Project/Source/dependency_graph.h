
#pragma once

#include "base.h"
#include "tb.h"

////////
// Dependency graph explanation
// In this programming language, there is no strict order of declaration/
// definition. This means that this compiler can't be single pass. Not only
// that, but, given the desired complexity of the language, particularly compile
// time execution, certain procedures and variables might have to pause their
// compilation to wait for other things. A procedure might not be able to complete
// the type checking phase because it might need the type of a struct which is declared
// (and defined) later on in the code, perhaps in another file. This is the reason behind
// the existence of "compilation entities", as I call them. I coined this term because
// there is little to no research/documentation on this specific part of compilation.
// Jonathan Blow called them "compilation units" in this video:
// https://www.youtube.com/watch?v=MnctEW1oL-E&t=227s&ab_channel=JonathanBlow
// but I would like to differentiate from the actual compilation units, aka object files.
// The definition of compilation entity is rather vague, but I would say it's any "part" of the
// code which might have to wait for another entity to complete a specific stage, or viceversa
// (another entity could possibly want to wait for this one to complete a stage). Compilation
// entities and compilation stages are defined in a language-dependent manner, but in general
// they should be as big as possible without breaking the aforementioned rule, for performance
// reasons. "Compilation phase" and "compilation stage" are used interchangebly.
// This file handles the dependency graph, which is this big data structure that contains the
// entities and their dependencies.
// Other modules should call Dg_Yield and Dg_Error to tell the this module to update the dependency
// graph accordingly.

struct Ast_Node;
struct Parser;
struct Ast_FileScope;

struct Typer;
struct Interp;

struct DepGraph;
typedef uint32 Dg_Idx;
typedef uint32 Dg_Gen;

#define Dg_Null -1

// Indicates at which point a specific compilation entity
// is at in the compilation process. For example, a compilation
// entity at phase CompPhase_ComputeSize means that it is currently
// doing that (and it hasn't finished), but it has finished typechecking
// successfully.
enum CompPhase : uint8
{
    CompPhase_Uninit = 0,
    CompPhase_Typecheck,
    CompPhase_ComputeSize,
    CompPhase_Bytecode,
    CompPhase_Run,
    
    CompPhase_EnumSize
};

struct Dg_IdxGen
{
    Dg_Idx idx;
    Dg_Gen gen;
};

struct Dg_Dependency
{
    Dg_Idx idx;
    CompPhase neededPhase;
};

enum EntityFlags
{
    // Tarjan visit
    Entity_OnStack  = 1 << 0,
    Entity_Error    = 1 << 1
};

struct Dg_Entity
{
    uint8 flags;
    
    Ast_Node* node;
    
    // Dependencies
    Array<Dg_Dependency> waitFor;
    
    // Tarjan visit
    CompPhase phase;
    uint32 stackIdx;
    uint32 sccIdx;  // Strongly Connected Component
};

struct Dg_ProcDef
{
    Dg_Idx id;
    TB_Function* ir;
};

struct Queue
{
    Array<Dg_ProcDef> procs;
    // others
    
    
};

struct PhaseInfo
{
    int numPassed;
    int numRemaining;
};

struct DepGraph
{
    Arena arena;  // Where to allocate entities (items)
    
    // Entity list
    Slice<Dg_Entity> items;
    
    // Input queue is queues[compphase*2],
    // Processing queue is queues[compphase*2+1]
    // Output queue is the same queue as the input of the next phase
    Queue queues[CompPhase_EnumSize*2+1];
    PhaseInfo infos[CompPhase_EnumSize];
    
    // Context for ease of use
    Dg_Idx curIdx;
    
    Typer* typer;
    Interp* interp;
    
    // False if no error
    bool error;
};

// To use in other modules to update the dependency graph
// based on the outcome of an operation.
void Dg_Yield(Ast_Node* yieldUpon, CompPhase neededPhase);
void Dg_Error();

Queue* GetInputPipe(DepGraph graph, CompPhase phase);
Queue* GetProcessingPipe(DepGraph graph, CompPhase phase);
// NOTE: output pipe of current phase is the same as the input pipe of the next one
// right now. We might change this later when we start parallelizing pipeline execution
Queue* GetOutputPipe(DepGraph graph, CompPhase phase);

Dg_IdxGen Dg_NewNode(Ast_Node* node, Arena* allocTo, Slice<Dg_Entity>* entities);
void Dg_StartIteration(DepGraph* g, Queue* q);

void Dg_UpdateQueue(DepGraph* g, Queue* q, int inputIdx, bool success);
void Dg_PerformStage(DepGraph* g, CompPhase phase);
bool Dg_DetectCycle(DepGraph* g, CompPhase phase);
void Dg_TarjanVisit(DepGraph* g, Dg_Entity* node, Slice<Dg_Entity*>* stack, Arena* stackArena);
void Dg_TopologicalSort(DepGraph* g, Queue* queue);  // Assumes that 'stackIdx' is populated
// Prints an error message for the user
void Dg_ExplainCyclicDependency(DepGraph* g, Queue* q, Dg_Idx start, Dg_Idx end);
String Dg_CompPhase2Sentence(CompPhase phase, bool pastTense = false);
String Dg_CompPhase2Str(CompPhase phase);
// For debugging purposes
void Dg_DebugPrintDeps(DepGraph* g);

bool MainDriver(Parser* p, Interp* interp, Ast_FileScope* file);
