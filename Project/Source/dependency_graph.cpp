
#include "base.h"
#include "memory_management.h"
#include "semantics.h"

// TODO: A lot of stuff still missing...

// Lastly I should implement some more features that could
// stress the dependency system more... Like #size_of and #type_of
// which are very simple to implement

DepGraph Dg_InitGraph(Arena* phaseArenas[CompPhase_EnumSize][2])
{
    DepGraph graph;
    graph.arena = Arena_VirtualMemInit(GB(1), MB(2));
    
    graph.queues[0].outputArena = phaseArenas[0][1];  // Input arena of stage 1
    for(int i = 1; i < CompPhase_EnumSize; ++i)
    {
        auto& queue = graph.queues[i];
        queue.inputArena = graph.queues[i-1].outputArena;
        queue.processingArena = phaseArenas[i][0];
        queue.outputArena = phaseArenas[i][1];
        queue.input      = { 0, 0 };
        queue.processing = { 0, 0 };
        queue.output     = { 0, 0 };
        
        queue.phase  = (CompPhase)i;
        queue.isDone = false;
    }
    
    return graph;
}

int MainDriver(Parser* p, Ast_FileScope* file)
{
    // Typer initialization
    size_t size = GB(2);
    size_t commitSize = MB(2);
    
    Arena typeArena = Arena_VirtualMemInit(size, commitSize);
    
    // Initialize the graph
    Arena  phaseArenas[CompPhase_EnumSize][2];
    Arena* phaseArenaPtrs[CompPhase_EnumSize][2];
    for(int i = 0; i < CompPhase_EnumSize; ++i)
    {
        for(int j = 0; j < 2; ++j)
        {
            phaseArenaPtrs[i][j] = &phaseArenas[i][j];
            phaseArenas[i][j] = Arena_VirtualMemInit(size, commitSize);
        }
    }
    
    DepGraph g = Dg_InitGraph(phaseArenaPtrs);
    Typer t = InitTyper(&typeArena, p);
    t.graph = &g;
    g.typer = &t;
    t.fileScope = file;
    
    // First pass.
    // Add all top-level declarations to the symbol table
    // Functions will yield if a scope has a construct that
    // could add to the symbol table later
    Ast_Block* block = &file->scope;
    for_array(i, block->stmts)
    {
        auto astNode = block->stmts[i];
        
        if(Ast_IsDecl(astNode))
            AddDeclaration(&t, block, (Ast_Declaration*)astNode);
        
        // Add nodes to the graph
        auto idxGen = Dg_NewNode(&g, astNode);
        astNode->entityIdx = idxGen.idx;
        astNode->phase = CompPhase_Parse;
        auto& typecheckQueue = g.queues[CompPhase_Parse];
        typecheckQueue.input.Append(typecheckQueue.inputArena, idxGen.idx);
    }
    
    // NOTE(Leo): The called functions have the responsibility of
    // appropriately calling Yield or SaveError to update
    // the dependency graph.
    
    bool exit = false;
    int remainingIters = 10;
    while(!exit && remainingIters > 0)
    {
        --remainingIters;
        exit = true;
        // For each stage in the pipeline
        for(int i = CompPhase_Uninit + 1; i < CompPhase_EnumSize; ++i)
        {
            auto& queue = g.queues[i];
            Dg_UpdateQueueArrays(&g, &queue);
            
            if(queue.input.length == 0) continue;
            else exit = false;
            
            // Detect cycle and perform topological sort
            bool detected = Dg_DetectCycle(&g, &queue);
            if(detected)
            {
                printf("Cycle!!\n");
                return 1;
            }
            else
            {
                Dg_PerformStage(&g, &queue);
            }
        }
    }
    
    if(!exit)
    {
        // @temporary
        printf("There likely was an infinite loop in the program.\n");
    }
    
    if(t.tokenizer->status != CompStatus_Success)
        return 1;
    
    return 0;
}

Dg_IdxGen Dg_NewNode(DepGraph* g, Ast_Node* node)
{
    Dg_IdxGen res = { 0, 0 };
    if(g->firstFree == Dg_Null)
    {
        g->items.ResizeAndInit(&g->arena, g->items.length+1);
        g->items[g->items.length-1].gen = 0;
        res = { (uint32)g->items.length-1, 0 };
    }
    else
    {
        auto freeNode = g->firstFree;
        g->firstFree = g->items[g->firstFree].nextFree;
        res = { freeNode, g->items[freeNode].gen };
    }
    
    // Update the node. The idx will be same until
    // the node has gone through all stages in the
    // pipeline, so it doesn't have to be maintained.
    node->entityIdx = res.idx;
    node->phase     = CompPhase_Parse;
    
    auto& item    = g->items[res.idx];
    item.node     = node;
    item.waitFor  = { 0, 0 };
    item.stackIdx = -1;
    item.sccIdx   = -1;
    item.onStack  = false;
    return res;
}

void Dg_RemoveNode(DepGraph* g, Dg_Idx idx)
{
    // Invalidate all other references to this entity.
    // (the generation should always be checked when
    // traversing the graph)
    ++g->items[idx].gen;
    g->items[idx].waitFor.FreeAll();
    
    if(g->firstFree == Dg_Null)
    {
        g->firstFree = idx;
        g->lastFree  = idx;
    }
    else
    {
        g->items[g->lastFree].nextFree = idx;
        g->lastFree = idx;
    }
}

void Dg_UpdateQueueArrays(DepGraph* g, Queue* q)
{
    if(q->phase < CompPhase_EnumSize - 1)
        q->output = g->queues[q->phase+1].input;
    
    if(q->phase > CompPhase_Uninit + 1)
        q->input = g->queues[q->phase-1].output;
}

void Dg_CompletedAllStages(DepGraph* g, Dg_Idx entityIdx)
{
    // Is there anything else we need to do?
    
    //Dg_RemoveNode(g, entityIdx);
}

void Dg_Yield(DepGraph* g, Ast_Node* yieldUpon, CompPhase neededPhase)
{
    Dg_Dependency dep;
    if(yieldUpon->entityIdx == Dg_Null)
    {
        Dg_IdxGen idxGen = Dg_NewNode(g, yieldUpon);
        dep.idx = idxGen.idx;
        dep.gen = idxGen.gen;
    }
    else
    {
        dep.idx = yieldUpon->entityIdx;
        dep.gen = g->items[dep.idx].gen;
    }
    
    dep.neededPhase = neededPhase;
    g->items[g->curIdx].waitFor.Append(dep);
}

void Dg_Error(DepGraph* g)
{
    printf("Error!\n");  // Need to mark the node somehow
}

void Dg_PerformStage(DepGraph* graph, Queue* q)
{
    Swap(auto, q->inputArena, q->processingArena);
    Swap(auto, q->input, q->processing);
    
    // Reset system states
    graph->typer->curScope = &graph->typer->fileScope->scope;
    
    bool passed = false;
    switch(q->phase)
    {
        case CompPhase_Uninit:    break;
        case CompPhase_EnumSize:  break;
        case CompPhase_Parse:  // Need to do typechecking
        {
            for_array(i, q->processing)
            {
                auto& gNode = graph->items[q->processing[i]];
                graph->curIdx = q->processing[i];
                bool outcome = CheckNode(graph->typer, gNode.node);
                if(outcome)
                {
                    gNode.waitFor.FreeAll();
                    gNode.node->phase = CompPhase_Typecheck;
                    q->output.Append(q->outputArena, q->processing[i]);
                }
                else
                    q->input.Append(q->inputArena, q->processing[i]);
            }
            
            break;
        }
        case CompPhase_Typecheck:  // Need to compute size or perform codegen
        {
            for_array(i, q->processing)
            {
                auto& gNode = graph->items[q->processing[i]];
                auto astNode = graph->items[q->processing[i]].node;
                graph->curIdx = q->processing[i];
                if(astNode->kind == AstKind_StructDef)  // Need size
                {
                    if(ComputeSize(graph->typer, (Ast_StructDef*)astNode))
                    {
                        gNode.waitFor.FreeAll();
                        astNode->phase = CompPhase_ComputeSize;
                        q->output.Append(q->outputArena, q->processing[i]);
                    }
                    else
                        q->input.Append(q->inputArena, q->processing[i]);
                }
                else  // Need codegen
                {
                    
                }
            }
            
            break;
        }
        case CompPhase_ComputeSize:  // Need to perform codegen
        {
            for_array(i, q->processing)
            {
                /*auto& gNode = graph->items[q->processing[i]];
                auto astNode = graph->items[q->processing[i]].node;
                gNode.waitFor.FreeAll();
                astNode->phase = CompPhase_Codegen;
                q->output.Append(q->outputArena, q->processing[i]);
*/
            }
            
            break;
        }
        case CompPhase_Codegen:
        {
            for_array(i, q->processing)
            {
                auto& gNode = graph->items[q->processing[i]];
                gNode.waitFor.FreeAll();
                q->output.Append(q->outputArena, q->processing[i]);
            }
            
            break;
        }
    }
    
    Arena_FreeAll(q->processingArena);
    q->processing.ptr = 0;
    q->processing.length = 0;
}

uint32 globalIdx = 0;

bool Dg_DetectCycle(DepGraph* g, Queue* q)
{
    if(q->input.length <= 2) return false;  // Can't have a cycle
    
    // Apply Tarjan's algorithm to find all Strongly Connected Components
    // Found here: https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
    
    // Clean variables for next use of algorithm
    
    globalIdx = 0;
    for_array(i, q->input)
        g->items[q->input[i]].stackIdx = -1;
    
    for_array(i, q->input)
    {
        auto it = &g->items[q->input[i]];
        if(it->stackIdx == -1)
        {
            ScratchArena scratch;
            Array<Dg_Entity*> stack = { 0, 0 };
            Dg_TarjanVisit(g, it, &stack, scratch);
        }
        
        Assert(it->stackIdx != -1);
    }
    
    // At this point all stack indices are populated (not -1)
    
    // This differs from the original algorithm,
    // because this is done in place instead of
    // Producing a separate array (maybe this is faster?)
    // Also slightly easier to implement.
    Dg_TopologicalSort(g, q);  // Changes the order in the queue
    
    // Loop through all SCCs
    uint32 start = 0;
    uint32 numGroups = 0;
    
    // @temporary
    uint32 numCycles = 0;
    
    while(true)
    {
        if(start >= q->input.length) break;  // No more SCCs
        
        auto groupId = g->items[q->input[start]].sccIdx;
        
        uint32 end = start;
        while(end < q->input.length && g->items[q->input[end]].sccIdx == groupId) ++end;
        
        ++numGroups;
        
        //printf("Group %d, indices %d %d\n", groupId, start, end-1);
        
        if(end > start + 1)  // If only one item, there's no cycle in this SCC
        {
            bool canPass = true;
            for(int i = start; i < end && canPass; ++i)
            {
                for_array(j, g->items[q->input[i]].waitFor)
                {
                    auto& it = g->items[q->input[i]].waitFor[j];
                    //printf("needed %d, current %d\n", it.neededPhase, q->phase);
                    
                    if(it.neededPhase == q->phase+1)
                    {
                        //printf("stackidx %d, groupId %d\n", g->items[it.idx].stackIdx, groupId);
                        if(g->items[it.idx].stackIdx != groupId)
                        {
                            //printf("cannot pass!\n");
                            canPass = false;
                            break;
                        }
                    }
                    
                    // If the dependency is not breakable it can't pass
                    // We need to figure out how to update the dependencies
                    if(!Dg_IsDependencyBreakable(g, q, &g->items[q->input[i]], it.idx))
                    {
                        canPass = false;
                        break;
                    }
                }
            }
            
            if(canPass)
            {
                // Dependencies can be removed, i would think
                
            }
            else
            {
                // Explain the cycle to the user
                Dg_ExplainCyclicDependency(g, q, start, end);
                
                ++numCycles;
            }
        }
        
        start = end;
    }
    
    //printf("Num cycles: %d\n", numCycles);
    
    return numCycles > 0;
}

void Dg_TarjanVisit(DepGraph* g, Dg_Entity* node, Array<Dg_Entity*>* stack, Arena* stackArena)
{
    node->stackIdx = globalIdx;
    node->sccIdx = globalIdx;
    ++globalIdx;
    
    stack->Append(stackArena, node);
    node->onStack = true;
    
    // Consider successors of node
    for_array(i, node->waitFor)
    {
        auto waitFor = &g->items[node->waitFor[i].idx];
        if(waitFor->stackIdx == -1)
        {
            // Successor has not yet been visited,
            // recurse on it
            Dg_TarjanVisit(g, waitFor, stack, stackArena);
            node->sccIdx = min(node->sccIdx, waitFor->sccIdx);
        }
        else if(waitFor->onStack)  // If on the stack
        {
            // Successor w is in the stack and hence in the current SCC
            // If w is not in the stack, than (v, w) edge pointing to an
            // SCC already found and must be ignored.
            // NOTE: the next line may look odd - but it is correct.
            // It's waitFor->stackIdx and not waitFor->sccIdx; that is
            // deliberate and from the original paper.
            node->sccIdx = min(node->sccIdx, waitFor->stackIdx);
        }
    }
    
    // This means that 'node' is a root node for a given SCC
    if(node->sccIdx == node->stackIdx)
    {
        // Pop the stack up to and including the current node
        Dg_Entity* popped;
        do
        {
            popped = (*stack)[stack->length-1];
            stack->Resize(stackArena, stack->length-1);
            popped->onStack = false;
        }
        while(popped != node);
    }
}

// Assumes that 'stackIdx' is populated
void Dg_TopologicalSort(DepGraph* graph, Queue* queue)
{
    Dg_Idx tmp;
    
#define Tmp_Less(i, j) graph->items[queue->input[i]].stackIdx < graph->items[queue->input[j]].stackIdx
#define Tmp_Swap(i, j) tmp = queue->input[i], queue->input[i] = queue->input[j], queue->input[j] = tmp
    QSORT(queue->input.length, Tmp_Less, Tmp_Swap);
#undef Tmp_Swap
#undef Tmp_Less
}

bool Dg_IsDependencyBreakable(DepGraph* g, Queue* q, Dg_Entity* source, Dg_Idx target)
{
    return true;
}

void Dg_ExplainCyclicDependency(DepGraph* g, Queue* q, Dg_Idx start, Dg_Idx end)
{
    
}