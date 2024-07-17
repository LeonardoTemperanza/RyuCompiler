
#include "base.h"
#include "memory_management.h"
#include "semantics.h"

static DepGraph graph;

// I should implement some more features that could
// stress the dependency system more... Like #size_of and #type_of
// which are very simple to implement

bool MainDriver(Parser* p, Interp* interp, Ast_FileScope* file)
{
    ProfileFunc(prof);
    
    // Typer initialization
    size_t size = GB(1);
    size_t commitSize = MB(2);
    
    Arena typeArena = Arena_VirtualMemInit(size, commitSize);
    
    // Graph initialization
    // TODO: For now we only have one file, one ast.
    // later on we'll need to merge all dynamic arrays
    graph.interp = interp;
    graph.items = p->entities;
    graph.queues[0].procs = p->procs;
    p->procs.ptr    = nullptr;
    p->procs.len = 0;
    
    Typer t = InitTyper(&typeArena, p->tokenizer);
    t.fileScope = file;
    
    graph.typer = &t;
    
    // NOTE(Leo): The called functions have the responsibility of
    // appropriately calling Yield or SaveError to update
    // the dependency graph.
    
    bool done;
    bool progress;
    do
    {
        done = true;
        progress = false;
        
        // For each stage in the pipeline
        for(int i = CompPhase_Uninit + 1; i < CompPhase_EnumSize; ++i)
        {
            CompPhase phase = (CompPhase)i;
            
#if DebugDep
            String phaseStr = Dg_CompPhase2Str(phase);
            EPrintln<"%:">(phaseStr);
#endif
            
            // Detect cycles and perform topological sorting
            bool cycle = Dg_DetectCycle(&graph, phase);
            Dg_PerformStage(&graph, phase);
            
            if(cycle) graph.error = true;
            
            if(graph.infos[phase].numPassed > 0)
                progress = true;
            if(graph.infos[phase].numRemaining > 0)
                done = false;
#if DebugDep
            Dg_DebugPrintDeps(&g);
#endif
        }
    }
    while(!done && progress);
    
    if(!done && !progress)
        EPrintln<"Internal error: Unable to resolve code dependencies and/or detect a cycle.">();
    
    return !graph.error;
}

Queue* GetInputPipe(DepGraph graph, CompPhase phase)
{
    return &graph.queues[phase*2];
}

Queue* GetProcessingPipe(DepGraph graph, CompPhase phase)
{
    return &graph.queues[phase*2+1];
}

Queue* GetOutputPipe(DepGraph graph, CompPhase phase)
{
    return &graph.queues[(phase+1)*2];
}

Dg_IdxGen Dg_NewNode(Ast_Node* node, Arena* allocTo, Slice<Dg_Entity>* entities)
{
    entities->ResizeAndInit(allocTo, entities->len+1);
    Dg_IdxGen res = { (uint32)entities->len-1, 0 };
    
    // Update the node. The idx will be same until
    // the node has gone through all stages in the
    // pipeline, so it doesn't have to be maintained.
    node->entityIdx = res.idx;
    node->phase     = CompPhase_Typecheck;
    
    auto& item    = (*entities)[res.idx];
    item.node     = node;
    item.waitFor  = { 0, 0 };
    item.stackIdx = -1;
    item.sccIdx   = -1;
    item.flags    = 0;
    item.phase    = CompPhase_Typecheck;
    return res;
}

void Dg_Yield(Ast_Node* yieldUpon, CompPhase neededPhase)
{
    Dg_Dependency dep;
    Assert(yieldUpon->entityIdx != Dg_Null);
    
    dep.idx = yieldUpon->entityIdx;
    
    dep.neededPhase = neededPhase;
    graph.items[graph.curIdx].waitFor.Append(dep);
}

void Dg_Error()
{
    // Mark the current node
    graph.items[graph.curIdx].flags |= Entity_Error;
    graph.error = true;
}

#if 0
void Dg_UpdateQueue(DepGraph* graph, CompPhase phase, int inputIdx, bool success)
{
    Dg_Idx entityIdx = q->processing[inputIdx];
    Dg_Entity& entity = graph->items[entityIdx];
    if(entity.flags & Entity_Error) return;
    
    if(success)
    {
        entity.waitFor.FreeAll();
        Dg_UpdatePhase(&entity, (CompPhase)(q->phase+1));
        q->output.Append(q->outputArena, entityIdx);
        ++q->numSucceeded;
    }
    else
    {
        q->input.Append(q->inputArena, entityIdx);
        ++q->numFailed;
    }
}
#endif

void Dg_PerformStage(DepGraph* graph, CompPhase phase)
{
    ProfileFunc(prof);
    
    ResetTyper(graph->typer);
    
    Queue* inputQueue      = GetInputPipe(*graph, phase);
    Queue* processingQueue = GetProcessingPipe(*graph, phase);
    Queue* outputQueue     = GetOutputPipe(*graph, phase);
    
    // Procedure definitions
    bool passed = true;
    switch(phase)
    {
        case CompPhase_Uninit: break;
        case CompPhase_EnumSize: break;
        case CompPhase_Typecheck:
        {
            for(int i = 0; i < inputQueue->procs.len; ++i)
            {
                Dg_Idx id = inputQueue->procs[i].id;
                auto proc = (Ast_ProcDef*)graph->items[id].node;
                bool ok = CheckProcDef(graph->typer, proc);
            }
            
            break;
        }
        case CompPhase_ComputeSize:
        {
            for(int i = 0; i < inputQueue->procs.len; ++i)
            {
                Dg_Idx id = inputQueue->procs[i].id;
                auto proc = (Ast_ProcDef*)graph->items[id].node;
                bool ok = ComputeSizeProc(t, proc);
            }
            
            break;
        }
        case CompPhase_Bytecode:
        {
            for(int i = 0; i < inputQueue->procs.len; ++i)
            {
                Dg_Idx id = inputQueue->procs[i].id;
                auto proc = (Ast_ProcDef*)graph->items[id].node;
                bool ok = CG_GenProc(t, proc);
            }
            
            break;
        }
        case CompPhase_Run:
        {
            
            
            break;
        }
    }
    
    // Global variables, etc.
}

#if 0
void Dg_PerformStage(DepGraph* graph, Queue* q)
{
    ProfileFunc(prof);
    
    Swap(auto, *q->inputArena, *q->processingArena);
    Swap(auto, q->input, q->processing);
    
    ResetTyper(graph->typer);
    
    bool passed = false;
    switch(q->phase)
    {
        case CompPhase_Uninit:    break;
        case CompPhase_EnumSize:  break;
        case CompPhase_Typecheck:
        {
            for_array(i, q->processing)
            {
                auto& gNode = graph->items[q->processing[i]];
                if(gNode.flags & Entity_Error) continue;
                
                graph->curIdx = q->processing[i];
                bool outcome = CheckNode(graph->typer, gNode.node);
                Dg_UpdateQueue(graph, q, i, outcome);
            }
            
            break;
        }
        case CompPhase_ComputeSize:
        {
            for_array(i, q->processing)
            {
                auto& gNode = graph->items[q->processing[i]];
                auto astNode = gNode.node;
                
                if(gNode.flags & Entity_Error) continue;
                graph->curIdx = q->processing[i];
                
                bool outcome = ComputeSize(graph->typer, astNode);
                Dg_UpdateQueue(graph, q, i, outcome);
            }
            
            break;
        }
        case CompPhase_Bytecode:
        {
            for_array(i, q->processing)
            {
                auto& gNode = graph->items[q->processing[i]];
                auto astNode = gNode.node;
                
                //bool success = GenBytecode(graph->interp, astNode);
                Dg_UpdateQueue(graph, q, i, true);
            }
            
            break;
        }
        case CompPhase_Run:
        {
            for_array(i, q->processing)
            {
                auto& gNode = graph->items[q->processing[i]];
                auto astNode = gNode.node;
                
                ++q->numSucceeded;
            }
            
            break;
        }
    }
    
    Arena_FreeAll(q->processingArena);
    q->processing.ptr = 0;
    q->processing.len = 0;
}
#endif

uint32 globalIdx = 0;
bool Dg_DetectCycle(DepGraph* g, CompPhase phase)
{
    ProfileFunc(prof);
    
    // Apply Tarjan's algorithm to find all Strongly Connected Components
    
    // Only nodes from the current phase queue are considered for
    // the start of the DFS. In the DFS all nodes are considered
    
    Queue* input = GetInputPipe(*g, phase);
    
    // Clean variables for next use of algorithm
    globalIdx = 0;
    
    for(int i = 0; i < input->procs.len; ++i)
    {
        int id = input->procs[i].id;
        g->items[id].stackIdx = -1;
    }
    // Repeat for all things
    
    for(int i = 0; i < input->procs.len; ++i)
    {
        int id = input->procs[i].id;
        auto it = &g->items[id];
        if(it->stackIdx == -1)
        {
            ScratchArena scratch;
            Slice<Dg_Entity*> stack = { 0, 0 };
            
            Dg_TarjanVisit(g, it, &stack, scratch);
        }
        
        Assert(it->stackIdx != -1);
    }
    
    // At this point all stack indices are populated (not -1)
    
    // This differs from the original algorithm,
    // because this is done in place instead of
    // producing a separate array (I suspect this could be faster)
    // Also slightly easier to implement.
    //Dg_TopologicalSort(g, q);  // Changes the order in the queue
    
    // Loop through all SCCs
    uint32 start = 0;
    uint32 numGroups = 0;
    
    // Number of SCCs which contain at least one cycle
    uint32 numCycles = 0;
    
    while(start < input->procs.len)
    {
        int startId = input->procs[start].id;
        auto groupId = g->items[startId].sccIdx;
        
        uint32 end = start;
        while(end < input->procs.len && g->items[input->procs[end].id].sccIdx == groupId)
            ++end;
        
        ++numGroups;
        bool isCycle = false;
        
        if(end <= start + 1)  // If only one item, there might be no cycle
        {
            auto& startNode = g->items[input->procs[start].id];
            if(!(startNode.flags & Entity_Error))  // Ignore errors
            {
                for_array(i, startNode.waitFor)
                {
                    auto& dep = startNode.waitFor[i];
                    
                    // Ignore errors
                    if(g->items[dep.idx].flags & Entity_Error)
                        continue;
                    
                    // Ignore dated dependency and errors
                    if(dep.neededPhase < g->items[dep.idx].phase)
                        continue;
                    
                    if(g->items[dep.idx].sccIdx == groupId)
                    {
                        isCycle = true;
                        break;
                    }
                }
            }
        }
        else  // There's at least one cycle for sure
            isCycle = true;
        
#if DebugDep
        EPrint<"Group %, indices % %">(groupId, start, end-1);
#endif
        
        if(isCycle)
        {
#if DebugDep
            EPrintln<", cycle\n">();
#endif
            
            ++numCycles;
            // Set these items' error flags, the errors will be propagated later
            // to all dependant nodes
            for(int i = start; i < end; ++i)
            {
                auto& item = g->items[input->procs[i].id];
                item.flags |= Entity_Error;
            }
            
            //Dg_ExplainCyclicDependency(g, q, start, end);
        }
        
#if DebugDep
        EPrintln<"">();
#endif
        start = end;
    }
    
    return numCycles > 0;
}

void Dg_TarjanVisit(DepGraph* g, Dg_Entity* node, Slice<Dg_Entity*>* stack, Arena* stackArena)
{
    // Apply Tarjan's algorithm to find all Strongly Connected Components
    // Found here: https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
    
    node->stackIdx = globalIdx;
    node->sccIdx = globalIdx;
    ++globalIdx;
    
    stack->Append(stackArena, node);
    node->flags |= Entity_OnStack;
    
    // Consider successors of node
    for_array(i, node->waitFor)
    {
        auto& waitFor = g->items[node->waitFor[i].idx];
        
        // Not part of the original algorithm; ignore the
        // dated dependencies
        if(node->waitFor[i].neededPhase < waitFor.phase)
            continue;
        
        if(waitFor.stackIdx == -1)
        {
            // Successor has not yet been visited,
            // recurse on it
            Dg_TarjanVisit(g, &waitFor, stack, stackArena);
            node->sccIdx = min(node->sccIdx, waitFor.sccIdx);
        }
        else if(waitFor.flags & Entity_OnStack)
        {
            // Successor w is in the stack and hence in the current SCC
            // If w is not in the stack, than (v, w) edge pointing to an
            // SCC already found and must be ignored.
            // NOTE: the next line may look odd - but it is correct.
            // It's waitFor.stackIdx and not waitFor.sccIdx; that is
            // deliberate and from the original paper.
            node->sccIdx = min(node->sccIdx, waitFor.stackIdx);
        }
        
        // Not part of the original algorithm; propagate
        // the error to all nodes that depend on it
        if(waitFor.flags & Entity_Error)
            node->flags |= Entity_Error;
    }
    
    // This means that 'node' is a root node for a given SCC
    if(node->sccIdx == node->stackIdx)
    {
        // Pop the stack up to and including the current node
        Dg_Entity* popped;
        do
        {
            popped = (*stack)[stack->len-1];
            stack->Resize(stackArena, stack->len-1);
            popped->flags &= ~Entity_OnStack;
        }
        while(popped != node);
    }
}

#if 0
// Assumes that 'stackIdx' is populated
void Dg_TopologicalSort(DepGraph* graph, Queue* queue)
{
    Dg_Idx tmp;
    
#define Tmp_Less(i, j) graph->items[queue->input[i]].stackIdx < graph->items[queue->input[j]].stackIdx
#define Tmp_Swap(i, j) tmp = queue->input[i], queue->input[i] = queue->input[j], queue->input[j] = tmp
    QSORT(queue->input.len, Tmp_Less, Tmp_Swap);
#undef Tmp_Swap
#undef Tmp_Less
}
#endif

#if 0
// TODO: We need an error API, where error infos are produced
void Dg_ExplainCyclicDependency(DepGraph* g, Queue* q, Dg_Idx start, Dg_Idx end)
{
    SetErrorColor();
    EPrint<"Error">();
    ResetColor();
    EPrintln<": Cyclic dependency was detected in the code; (TODO: complete message)">();
    
#if 0
    auto& startItem = g->items[q->input[start]];
    auto& endItem = g->items[q->input[end]];
    Ast_Node* startNode = g->items[q->input[start]].node;
    Ast_Node* endNode = g->items[end].node;
    CompPhase neededStartPhase = CompPhase_Uninit;
    CompPhase neededEndPhase   = CompPhase_Uninit;
    
    bool isDirect = false;
    for_array(i, g->items[start].waitFor)
    {
        auto& dep = g->items[start].waitFor[i];
        if(dep.idx == end)
        {
            isDirect = true;
            neededEndPhase = dep.neededPhase;
            break;
        }
    }
    
    if(!isDirect)
    {
        // Get max needed phase
        for(Dg_Idx i = 0; i < end; ++i)
        {
            auto& item = g->items[q->input[i]];
            for_array(j, item.waitFor)
            {
                if(item.waitFor[j].idx == end)
                    neededEndPhase = max(neededEndPhase, item.waitFor[j].neededPhase);
            }
        }
    }
    
    for_array(i, g->items[q->input[end]].waitFor)
    {
        auto& dep = g->items[end].waitFor[i];
        if(dep.idx == start)
        {
            
            break;
        }
    }
    
    ScratchArena scratch;
    StringBuilder strBuilder(scratch);
    strBuilder.Append("This construct can't ");
    strBuilder.Append(Dg_CompPhase2Sentence(g->items[start].phase));
    
    strBuilder.Append("...");
    
    CompileError(g->typer->tokenizer, startNode->where, strBuilder.string);
    
    strBuilder.Reset();
    if(isDirect)
        strBuilder.Append("... because it needs this other construct to have already ");
    else
        strBuilder.Append("... because it indirectly needs this other construct to have already ");
    
    strBuilder.Append(Dg_CompPhase2Sentence(CompPhase_Uninit, true));
    strBuilder.Append(", which in turn needs the previous one to have ");
    strBuilder.Append(Dg_CompPhase2Sentence(neededStartPhase, true));
    strBuilder.Append('.');
    
    CompileErrorContinue(g->typer->tokenizer, startNode->where, strBuilder.string);
#endif
}
#endif

String Dg_CompPhase2Sentence(CompPhase phase, bool pastTense)
{
    String res;
    if(!pastTense)
    {
        switch(phase)
        {
            case CompPhase_Uninit:      res = StrLit("[uninitialized phase]"); break;
            case CompPhase_Typecheck:   res = StrLit("determine its type");    break;
            case CompPhase_ComputeSize: res = StrLit("determine its size");    break;
            case CompPhase_Bytecode:    res = StrLit("generate its bytecode"); break;
            case CompPhase_Run:         res = StrLit("run at compile time");   break;
            case CompPhase_EnumSize:    res = StrLit("[enum size]");           break;
            default:                    res = StrLit("[invalid enum]");        break;
        }
    }
    else
    {
        switch(phase)
        {
            case CompPhase_Uninit:      res = StrLit("[uninitialized phase]");     break;
            case CompPhase_Typecheck:   res = StrLit("determined its type");       break;
            case CompPhase_ComputeSize: res = StrLit("determined its size");       break;
            case CompPhase_Bytecode:    res = StrLit("generated its bytecode");    break;
            case CompPhase_Run:         res = StrLit("run its compile time code"); break;
            case CompPhase_EnumSize:    res = StrLit("[enum size]");               break;
            default:                    res = StrLit("[invalid enum]");            break;
        }
    }
    
    return res;
}

String Dg_CompPhase2Str(CompPhase phase)
{
    String res;
    switch(phase)
    {
        case CompPhase_Uninit:      res = StrLit("[uninitialized phase]"); break;
        case CompPhase_Typecheck:   res = StrLit("Typechecking");          break;
        case CompPhase_ComputeSize: res = StrLit("Compute size");          break;
        case CompPhase_Bytecode:    res = StrLit("Bytecode");              break;
        case CompPhase_Run:         res = StrLit("Run");                   break;
        case CompPhase_EnumSize:    res = StrLit("[enum size]");           break;
        default:                    res = StrLit("[invalid enum]");        break;
    }
    
    return res;
}

void Dg_DebugPrintDeps(DepGraph* g)
{
    ScratchArena scratch;
    
    EPrintln<"Graph:">();
    
    for_array(i, g->items)
    {
        int numChars = 0;
        Token* start = g->items[i].node->where;
        Token* end = start;
        while(end < start + 2 && end->kind != Tok_EOF) ++end;
        
        String nodeStr = { start->text.ptr, end->ec - start->sc + 1 };
        nodeStr = nodeStr.CopyToArena(scratch);
        // Substitute newlines with spaces
        for_array(j, nodeStr)
        {
            if(nodeStr[j] == '\n')
                nodeStr.ptr[j] = ' ';
        }
        
        String phase = Dg_CompPhase2Str(g->items[i].phase);
        numChars += EPrint<"% ... (%)">(nodeStr, phase);
        
        if(g->items[i].node->kind == AstKind_ProcDecl)
            EPrint<" (decl)">();
        
        if(g->items[i].flags & Entity_Error)
            EPrint<" (error)">();
        
        auto& waitFor = g->items[i].waitFor;
        if(waitFor.len <= 0)
            EPrintln<" (no deps)">();
        
        for_array(j, waitFor)
        {
            int numSpaces = j > 0 ? numChars : 1;
            for(int i = 0; i < numSpaces; ++i)
                EPrint<" ">();
            
            EPrint<"-> ">();
            
            {
                Token* start = g->items[waitFor[j].idx].node->where;
                Token* end = start;
                while(end < start + 2 && end->kind != Tok_EOF) ++end;
                
                String nodeStr = { start->text.ptr, end->ec - start->sc + 1 };
                nodeStr = nodeStr.CopyToArena(scratch);
                // Substitute newlines with spaces
                for_array(j, nodeStr)
                {
                    if(nodeStr[j] == '\n')
                        nodeStr.ptr[j] = ' ';
                }
                
                EPrint<"% ... ">(nodeStr);
                String phase = Dg_CompPhase2Str(waitFor[j].neededPhase);
                EPrintln<"(%)">(phase);
            }
        }
    }
}
