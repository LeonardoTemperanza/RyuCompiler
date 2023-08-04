
#pragma once

//#include "semantics.h"

// TODO: Explanation here on how the dependency graph works

struct Ast_Node;
struct Parser;
struct Ast_FileScope;

struct Typer;

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
    CompPhase_Codegen,
    
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
    Dg_Gen gen;
    CompPhase neededPhase;
};

struct Dg_Entity
{
    // Invalidates all references to this entity
    // once it's removed
    Dg_Gen gen = 0;
    union
    {
        struct
        {
            Ast_Node* node;
            DynArray<Dg_Dependency> waitFor;
            
            // Tarjan visit
            uint32 stackIdx;
            uint32 sccIdx;  // Strongly Connected Component
            bool onStack;
        };
        Dg_Idx nextFree = Dg_Null;  // Only used when current node is not used
    };
};

struct Queue
{
    Arena* inputArena;
    Arena* processingArena;
    Arena* outputArena;  // This is the same as the input arena of the next stage
    
    Array<Dg_Idx> input;
    Array<Dg_Idx> processing;
    Array<Dg_Idx> output;
    
    CompPhase phase;
    bool isDone = false;
};

struct DepGraph
{
    Arena* arena;  // Where to allocate entities (items)
    
    // Entity pool
    Array<Dg_Entity> items = { 0, 0 };
    Dg_Idx firstFree = Dg_Null;
    Dg_Idx lastFree  = Dg_Null;
    
    // There's one queue for each stage in the
    // pipeline. The first entry is used as a
    // failsafe empty queue in case index is 0.
    Queue queues[CompPhase_EnumSize];
    
    // Context for ease of use
    Dg_Idx curIdx = 0;
    
    Typer* typer;
};

DepGraph Dg_InitGraph(Arena* arena, Arena* phaseArenas[CompPhase_EnumSize][2]);
Dg_IdxGen Dg_NewNode(DepGraph* g, Ast_Node* node);
void Dg_RemoveNode(DepGraph* g, Dg_Idx idx);
void Dg_UpdateQueueArrays(DepGraph* g, Queue* q);
void Dg_CompletedAllStages(DepGraph* g, Dg_Idx entityIdx);
void Dg_Yield(DepGraph* g, Ast_Node* yieldUpon, CompPhase neededPhase);
void Dg_Error(DepGraph* g, Ast_Node* entity);
void Dg_PerformStage(DepGraph* g, Queue* q);
bool Dg_DetectCycle(DepGraph* g, Queue* queue);
void Dg_TarjanVisit(DepGraph* g, Dg_Entity* node, Array<Dg_Entity*>* stack, Arena* stackArena);
void Dg_TopologicalSort(DepGraph* g, Queue* queue);  // Assumes that 'stackIdx' is populated
bool Dg_IsDependencyBreakable(DepGraph* g, Queue* q, Dg_Entity* source, Dg_Idx target);
void Dg_ExplainCyclicDependency(DepGraph* g, Queue* q, Dg_Idx start, Dg_Idx end);

int MainDriver(Parser* p, Ast_FileScope* file);