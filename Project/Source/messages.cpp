
// This module is tasked to render messages on
// terminals. Anything that involves showing text
// should be here.

#include "messages.h"
#include "lexer.h"
#include "os/os_generic.h"

static Message firstMessage;
static Message* lastMessage;

Message MakeMessage(MessageKind kind, MessagePhase phase, String path, String fileContents,
                    Token* at, MessageContent content)
{
    Message msg = {0};
    msg.kind    = kind;
    msg.phase   = phase;
    msg.path    = path;
    return msg;
}

Message MakeMessage(MessageKind kind, MessagePhase phase, String path, String fileContents,
                    Token* at, MessageContent content, Message* parent)
{
    Message msg = {0};
    msg.kind    = kind;
    msg.phase   = phase;
    msg.path    = path;
    msg.parent  = parent;
    return msg;
}

Message* AppendMessage(Message msg, Arena* dst)
{
    Message* newMsg = nullptr;
    
    if(firstMessage.kind == MsgKind_None)
    {
        firstMessage = msg;
        lastMessage  = &firstMessage;
        newMsg = &firstMessage;
    }
    else
    {
        newMsg = Arena_AllocVar(dst, Message);
        *newMsg = msg;
        
        lastMessage->next = newMsg;
        newMsg->prev = lastMessage;
        lastMessage = newMsg;
    }
    
    if(msg.parent)
    {
        if(!msg.parent->first)
            msg.parent->first = newMsg;
        else
        {
            Message* lastChild = msg.parent->first;
            while(lastChild->next) lastChild = lastChild->next;
            lastChild->next = newMsg;
        }
    }
    
    return newMsg;
}

void SortMessages()
{
    // We reorder messages in location order in a specific file,
    // and messages between different files should not be interleaved.
    // Files should be lexicographically sorted as well, for consistent
    // output. At the same time, errors > warnings > logs
    // e.g.: f1 e1 - f1 e3 - f1 e3 - f1 w1 - f1 w2 - f1 l1 - f2 e1 ...
    // Children messages need not be sorted
    
    ScratchArena scratch;
    Slice<String> paths = {0};
    
    // Find the list of files
    {
        for(Message* msg = &firstMessage; msg; msg = msg->next)
        {
            bool isNew = true;
            for(int i = 0; i < paths.len; ++i)
            {
                if(paths[i] == msg->path)
                {
                    isNew = false;
                    break;
                }
            }
            
            if(isNew) paths.Append(scratch, msg->path);
        }
    }
    
    // Sort the files lexicographically
    {
        String tmp;
#define Tmp_Less(i, j) LexCmp(paths[i], paths[j])
#define Tmp_Swap(i, j) tmp = paths[i], paths[i] = paths[j], paths[j] = tmp 
        QSORT(paths.len, Tmp_Less, Tmp_Swap);
#undef Tmp_Swap
#undef Tmp_Less
    }
    
    // For each file in the sorted array, do a sorting pass on the nodes in the tree
    {
        for(int i = 0; i < paths.len; ++i)
        {
            for(Message* mes = &firstMessage; mes; mes = mes->next)
            {
                //if(mes->path != paths[i]) break;
            }
        }
    }
}

void ShowMessages()
{
    MessageDrawCmd cmd = {0};
    cmd.showFileName = true;
    ShowMessages(&firstMessage, cmd);
}

void ShowMessages(Message* message, MessageDrawCmd cmd)
{
    if(!message || message->kind == MsgKind_None) return;
    
    // Draw message
    static int debug = 0;
    
    MessageContent content = message->content;
    switch(content.kind)
    {
        case Msg_None: fprintf(stderr, "None!\n"); break;
        case Msg_UnexpectedToken:
        {
            fprintf(stderr, "Unexpected token!\n");
            break;
        }
        
        case Msg_IncompatibleTypes: break;
        case Msg_IncompatibleReturns: break;
        case Msg_CannotDereference: break;
        case Msg_CannotConvertToScalar: break;
        case Msg_CannotConvertToIntegral: break;
        case Msg_LookAtProcDecl: break;
        
        case Msg_SpecializedMessage:
        {
            fprintf(stderr, "%.*s\n", (int)content.message.len, content.message.ptr);
            break;
        }
    }
    
    // Proceed recursively to all errors
    MessageDrawCmd childCmd = cmd;
    childCmd.showFileName = false;
    ShowMessages(message->first, childCmd);
    ShowMessages(message->next, cmd);
}

void ShowFileLine(Token* token, char* fileContents)
{
    
}

// TODO: Clean this up...
template<typename t>
void PrintArg(char* argName, char* desc, t defVal, int lPad, int rPad, int minCol, int maxCol, bool printDesc)
{
    // Print left padding
    int numChars = printf("%*c", lPad, ' ');
    
    numChars += printf("-%s ", argName);
    numChars += PrintArgAttributes<t>(defVal);
    
    // Print right padding
    numChars += printf("%*c", max(0, minCol - (numChars)), ' ');
    
    if(!printDesc)
    {
        printf("\n");
        return;
    }
    
    // Print one word at a time without exceeding the maximum columns
    int i = 0;
    bool firstWord = true;
    while(desc[i] != '\0')
    {
        if(desc[i] == ' ')
        {
            while(desc[i] == ' ') ++i;  // Eat all subsequent spaces
        }
        
        // Get word
        int start = i;
        while(desc[i] != ' ' && desc[i] != '\0') ++i;
        
        int length = i - start;
        bool exceedsMax = numChars + length + 1 >= maxCol;
        
        // If word doesn't fit here, show it in the next line.
        if(exceedsMax)
        {
            numChars = 0;
            printf("\n");
            numChars += printf("%*c", minCol, ' ');
        }
        else if(!firstWord)
            numChars += printf(" ");
        
        numChars += printf("%.*s", length, &desc[start]);
        
        firstWord = false;
    }
    
    printf("\n");
}

void PrintHelp()
{
    constexpr int lPad = 4;
    constexpr int rPad = 4;
    constexpr int minTermWidthForDesc = 65;
    
    int numCols = CurrentTerminalWidth();
    printf("Ryu Compiler v0.0\n");
    printf("Usage: ryu file1.ryu [ options ]\n");
    
    bool printDesc = numCols > minTermWidthForDesc;
    
    if(printDesc)
        printf("Options:\n");
    else
        printf("Options: (widen terminal window to get full description)\n");
    
    // Get max length of output strings
    int maxLength = 0;
#define X(varName, string, type, defaultVal, desc) maxLength = max(maxLength, strlen(string) + ArgAttributes<type>(defaultVal));
    CmdLineArgsInfo
#undef X
    
    // Add the left padding and also 3 because '-',
    // the space between the name and the default value string,
    // and the space between the default value string and the description
    maxLength += 3 + lPad;
    
    // Use the X macro to print the arguments
#define X(varName, string, type, defaultVal, desc) PrintArg<type>(string, desc, defaultVal, lPad, rPad, maxLength, numCols, printDesc);
    CmdLineArgsInfo
#undef X
}

void PrintTimeBenchmark(double frontend, double irGen, double backend, double linker)
{
    const double totalExceptLinker = frontend + irGen + backend;
    const double total = totalExceptLinker + linker;
    const int pad = 19;
    int numChars = 0;
    
    printf("----- Timings -----\n");
    numChars = printf("Frontend:", frontend);
    printf("%*c%lfs\n", max(1, pad - numChars), ' ', frontend);
    numChars = printf("IR Generation:");
    printf("%*c%lfs\n", max(1, pad - numChars), ' ', irGen);
    // TODO: Update this with the llvm backend
    numChars = printf("Backend (Tilde):");
    printf("%*c%lfs\n", max(1, pad - numChars), ' ', backend);
    
    numChars = printf("Total (no link):");
    printf("%*c%lfs\n\n", max(1, pad - numChars), ' ', totalExceptLinker);
    
    // TODO: Linker name is platform dependant
    if(args.useTildeLinker)
        numChars = printf("Linker (Tilde):");
    else
        numChars = printf("Linker (%s):", GetPlatformLinkerName());
    
    printf("%*c%lfs\n\n", max(1, pad - numChars), ' ', linker);
    
    numChars = printf("Total:", total);
    printf("%*c%lfs\n", max(1, pad - numChars), ' ', total);
    
    printf("-------------------\n");
}
