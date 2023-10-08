
#pragma once

//#include "semantics.h"

// TODO: Explanation here on how the dependency graph works

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
// entity at phase CompPhase_Typecheck means that it finished
// typechecking but has not finished the CompPhase_ComputeSize yet.
enum CompPhase : uint8
{
    CompPhase_Uninit = 0,
    CompPhase_Parse,
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
    Entity_OnStack = 1 << 0,
    Entity_Error   = 1 << 1
};

struct Dg_Entity
{
    Ast_Node* node;
    
    Array<Dg_Dependency> waitFor;
    
    // Tarjan visit
    CompPhase phase;
    uint32 stackIdx;
    uint32 sccIdx;  // Strongly Connected Component
    uint8 flags;
};

struct Queue
{
    Arena* inputArena;
    Arena* processingArena;
    Arena* outputArena;  // This is the same as the input arena of the next stage
    
    Slice<Dg_Idx> input;
    Slice<Dg_Idx> processing;
    Slice<Dg_Idx> output;
    
    CompPhase phase;
    bool isDone = false;
};

struct DepGraph
{
    Arena arena;  // Where to allocate entities (items)
    
    // Entity pool
    Slice<Dg_Entity> items = { 0, 0 };
    Dg_Idx firstFree = Dg_Null;
    Dg_Idx lastFree  = Dg_Null;
    
    // There's one queue for each stage in the
    // pipeline. The first entry is used as a
    // failsafe empty queue in case index is 0.
    Queue queues[CompPhase_EnumSize];
    
    // Context for ease of use
    Dg_Idx curIdx = 0;
    
    Typer* typer;
    Interp* interp;
    
    bool status = true;
};

DepGraph Dg_InitGraph(Arena* phaseArenas[CompPhase_EnumSize][2]);
Dg_IdxGen Dg_NewNode(Ast_Node* node, Arena* allocTo, Slice<Dg_Entity>* entities);
void Dg_UpdateQueueArrays(DepGraph* g, Queue* q);
void Dg_Yield(DepGraph* g, Ast_Node* yieldUpon, CompPhase neededPhase);
void Dg_Error(DepGraph* g);
void Dg_PerformStage(DepGraph* g, Queue* q);
bool Dg_DetectCycle(DepGraph* g, Queue* queue);
void Dg_TarjanVisit(DepGraph* g, Dg_Entity* node, Slice<Dg_Entity*>* stack, Arena* stackArena);
void Dg_TopologicalSort(DepGraph* g, Queue* queue);  // Assumes that 'stackIdx' is populated
// Prints an error message for the user
void Dg_ExplainCyclicDependency(DepGraph* g, Queue* q, Dg_Idx start, Dg_Idx end);
String Dg_CompPhase2Str(CompPhase phase, bool pastTense = false);
// For debugging purposes
void Dg_DebugPrintDependencies(DepGraph* g);

int MainDriver(Parser* p, Interp* interp, Ast_FileScope* file);
