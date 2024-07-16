
// This module is tasked to render messages on
// terminals. Anything that involves showing text
// should be here.

#include "messages.h"
#include "lexer.h"
#include "os/os_generic.h"

#include "ast.h"
#include "semantics.h"
#include "parser.h"
#include "lexer.h"

static Message firstMessage;
static Message* lastMessage;

Message MakeMessage(MessageKind kind, MessagePhase phase, String path, String fileContents,
                    Token* at, MessageContent content)
{
    Message msg = {0};
    msg.kind    = kind;
    msg.phase   = phase;
    msg.path    = path;
    msg.fileContents = fileContents;
    msg.token   = at;
    msg.content = content;
    return msg;
}

Message MakeMessage(MessageKind kind, MessagePhase phase, String path, String fileContents,
                    Token* at, MessageContent content, Message* parent)
{
    Message msg = {0};
    msg.kind    = kind;
    msg.phase   = phase;
    msg.path    = path;
    msg.fileContents = fileContents;
    msg.token   = at;
    msg.content = content;
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

int GetMessageFileIdx(Message* message, Slice<String> sortedPaths)
{
    int res = -1;
    for(int i = 0; i < sortedPaths.len; ++i)
    {
        if(message->path == sortedPaths[i])
        {
            res = i;
            break;
        }
    }
    
    return res;
}

int GetNumTopLevelMessages()
{
    int res = 0;
    for(Message* msg = &firstMessage; msg; msg = msg->next)
        ++res;
    
    return res;
}

int GetMessageKindImportance(MessageKind kind)
{
    switch(kind)
    {
        case MsgKind_None: return -1;
        case MsgKind_Error: return 3;
        case MsgKind_Warning: return 2;
        case MsgKind_Log: return 1;
    }
    
    return -1;
}

void SortMessages()
{
    // We reorder messages in location order in a specific file,
    // and messages between different files should not be interleaved.
    // Files should be lexicographically sorted as well, for consistent
    // output. At the same time, errors > warnings > logs
    // e.g.: f1 e1 - f1 e2 - f1 e3 - f1 w1 - f1 w2 - f1 l1 - f2 e1 ...
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
        int numMessages = GetNumTopLevelMessages();
        
        // Can't use QSORT because it's a linked list.
        // Simple selection sort impl
        for(Message* mes = &firstMessage; mes; mes = mes->next)
        {
            int fileIdx = GetMessageFileIdx(mes, paths);
            assert(fileIdx != -1);
            
            int mesKindImportance = GetMessageKindImportance(mes->kind);
            
            Message* minMessage = mes;
            for(Message* mes2 = mes; mes2; mes2 = mes2->next)
            {
                int fileIdx2 = GetMessageFileIdx(mes2, paths);
                assert(fileIdx2 != -1);
                
                int mesKindImportance2 = GetMessageKindImportance(mes2->kind);
                
                // Determine whether to swap or not
                bool swap = false;
                {
                    if(fileIdx2 < fileIdx)
                    {
                        swap = true;
                    }
                    else if(fileIdx2 == fileIdx)
                    {
                        if(mesKindImportance2 < mesKindImportance)
                        {
                            swap = true;
                        }
                        else if(mesKindImportance2 == mesKindImportance)
                        {
                            if(mes2->token < mes->token)
                                swap = true;
                        }
                    }
                }
                
                if(swap)
                {
                    minMessage = mes2;
                    mesKindImportance = mesKindImportance2;
                    fileIdx = fileIdx2;
                }
            }
            
            if(minMessage != mes)
            {
                // Swap the two nodes
                if(minMessage->prev)
                    minMessage->prev->next = mes;
                if(minMessage->next)
                    minMessage->next->prev = mes;
                if(mes->prev)
                    mes->prev->next = minMessage;
                if(mes->next)
                    mes->next->prev = minMessage;
                
                mes = minMessage;
            }
        }
    }
}

void ShowMessages()
{
    MessagePrintCmd cmd = {0};
    cmd.showFileName = true;
    ShowMessages(&firstMessage, cmd);
}

void ShowMessages(Message* message, MessagePrintCmd cmd)
{
    if(!message || message->kind == MsgKind_None) return;
    
    PrintMessage(message, cmd);
    
    // Proceed recursively to all errors
    MessagePrintCmd childCmd = cmd;
    childCmd.showFileName = false;
    ShowMessages(message->first, childCmd);
    ShowMessages(message->next, cmd);
}

void PrintMessage(Message* message, MessagePrintCmd cmd)
{
    PrintMessageLocation(message);
    
    ScratchArena scratch;
    
    switch(message->kind)
    {
        case MsgKind_None: break;
        case MsgKind_Error:
        {
            SetErrorColor();
            EPrint("Error: ");
            ResetColor();
            break;
        }
        case MsgKind_Warning:
        {
            SetWarningColor();
            EPrint("Warning: ");
            ResetColor();
            break;
        }
        case MsgKind_Log:
        {
            SetLogColor();
            EPrint("Log: ");
            ResetColor();
            break;
        }
    }
    
    MessageContent content = message->content;
    Token* token = message->token;
    switch(content.kind)
    {
        case Msg_None: EPrintln("Null message."); break;
        case Msg_UnexpectedToken:
        {
            EPrint("Unexpectedly found '%S', instead of ", token->text);
            
            if(content.tokKind == Tok_Ident)
            {
                EPrintln("an identifier.");
            }
            else
            {
                EPrintln("'%S'.", TokKindToString(content.tokKind, scratch));
            }
            
            break;
        }
        case Msg_IncompatibleTypes:
        {
            EPrint("Incompatible types!\n");
            break;
        }
        case Msg_IncompatibleReturns:
        {
            EPrint("Incompatible returns!\n");
            break;
        }
        case Msg_CannotDereference:
        {
            EPrint("Cannot dereference this type!\n");
            break;
        }
        case Msg_CannotConvertToScalar:
        {
            EPrint("Cannot convert this type to a scalar type!\n");
            break;
        }
        case Msg_CannotConvertToIntegral:
        {
            EPrint("Cannot convert this type to an integral type!\n");
            break;
        }
        case Msg_LookAtProcDecl:
        {
            EPrint("Here's the procedure call signature:\n");
            break;
        }
        case Msg_SpecializedMessage:
        {
            EPrint("%.*s\n", (int)content.message.len, content.message.ptr);
            break;
        }
    }
    
    EPrint("\n");
    PrintMessageSource(message);
}

void PrintMessageLocation(Message* message)
{
    Token* token = message->token;
    if(token->kind == Tok_Error)
        return;
    
    // TODO: Use fileNumLines (first it actually needs to be filled in) to print
    // the last line and last character when the error points to EOF.
    
    int lineNum = token->lineNum;
    int charNum = token->sc - token->sl + 1;
    if(token->kind == Tok_EOF)
    {
        EPrint("EOF: ");
        return;
    }
    
    EPrint("%S", message->path);
    EPrint("(%d,%d): ", token->lineNum, token->sc - token->sl + 1);
}

void PrintMessageSource(Message* message)
{
    EPrint("    > ");
    
    Token* token = message->token;
    String fileContents = message->fileContents;
    
    bool endOfLine = false;
    
    int i = token->sl;
    while(!endOfLine)
    {
        if(fileContents[i] == '\t')
            EPrint("    ");
        else
            EPrint("%c", fileContents[i]);
        
        ++i;
        endOfLine = fileContents[i] == '\r' ||
            fileContents[i] == '\n' ||
            fileContents[i] == 0;
    }
    
    int length = i - token->sl;
    
    EPrint("\n    > ");
    
    for(int i = token->sl; i < token->sc; ++i)
    {
        if(fileContents[i] == '\t')
            EPrint("    ");
        else
            EPrint(" ");
    }
    
    EPrint("^");
    
    for(int i = token->sc; i < token->ec - 1; ++i)
    {
        if(fileContents[i] == '\t')
            EPrint("----");
        else
            EPrint("-");
    }
    
    if(token->sc != token->ec)
        EPrint("^\n");
    else
        EPrint("\n");
}

// Printing utilities (alternative to printf, which also works for my custom types, such as TypeInfo and String)
int Print(char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int res = Print(stdout, fmt, args);
    va_end(args);
    return res;
}

int EPrint(char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int res = Print(stderr, fmt, args);
    va_end(args);
    return res;
}

int Println(char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int res = Print(stdout, fmt, args);
    va_end(args);
    
    putc('\n', stdout);
    ++res;
    return res;
}

int EPrintln(char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int res = Print(stderr, fmt, args);
    va_end(args);
    
    putc('\n', stderr);
    ++res;
    return res;
}

int Print(FILE* stream, char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int res = Print(stream, fmt, args);
    va_end(args);
    return res;
}

int Print(FILE* stream, char* fmt, va_list args)
{
    int res = 0;
    
    ScratchArena scratch;
    for(int i = 0; fmt[i] != '\0'; ++i)
    {
        if(fmt[i] == '%')
        {
            switch(fmt[i+1])
            {
                default:
                {
                    putc(fmt[i], stream);
                    putc(fmt[i+1], stream);
                    res += 2;
                    break;
                }
                case 'd':
                {
                    int toPrint = va_arg(args, int);
                    res += fprintf(stream, "%d", toPrint);
                    break;
                }
                case 'f':
                {
                    double toPrint = va_arg(args, double);
                    res += fprintf(stream, "%f", toPrint);
                    break;
                }
                case 'c':
                {
                    char toPrint = va_arg(args, int);
                    putc(toPrint, stream);
                    ++res;
                    break;
                }
                case 's':
                {
                    char* toPrint = va_arg(args, char*);
                    res += fprintf(stream, "%s", toPrint);
                    break;
                }
                case '.':
                {
                    if(fmt[i+2] == '*' && fmt[i+3] == 's')
                    {
                        i += 2;
                        int numChars = va_arg(args, int);
                        char* toPrint = va_arg(args, char*);
                        res += fprintf(stream, "%.*s", numChars, toPrint);
                    }
                    else
                    {
                        putc(fmt[i], stream);
                        putc(fmt[i+1], stream);
                        res += 2;
                    }
                    
                    break;
                }
                // Custom specifiers
                case 'T':  // TypeInfo*
                {
                    TypeInfo* toPrint = va_arg(args, TypeInfo*);
                    String typeStr = TypeInfo2String(toPrint, scratch);
                    res += fprintf(stream, "%.*s", (int)typeStr.len, typeStr.ptr);
                    break;
                }
                case 'S':  // String
                {
                    static_assert(sizeof(String) >= sizeof(int));
                    
                    String toPrint = va_arg(args, String);
                    res += fprintf(stream, "%.*s", (int)toPrint.len, toPrint.ptr);
                    break;
                }
            }
            
            ++i;
        }
        else
        {
            putc(fmt[i], stream);
            ++res;
        }
    }
    
    return res;
}

template<typename t>
void PrintHelpArg(char* argName, char* desc, t defVal, int lPad, int rPad, int minCol, int maxCol, bool printDesc)
{
    defer(fflush(stdout););
    
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
#define X(varName, string, type, defaultVal, desc) PrintHelpArg<type>(string, desc, defaultVal, lPad, rPad, maxLength, numCols, printDesc);
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

enum FileExtension
{
    File_Unknown,
    File_Ryu,
    File_Obj
};

constexpr FileExtension extensionTypes[] = { File_Ryu, File_Obj, File_Obj };
constexpr char* extensionStrings[] = { "ryu", "o", "obj" };

FileExtension GetExtensionFromPath(char* path, char** outExt)
{
    Assert(outExt);
    
    int dotIdx = -1;
    for(int i = 0; path[i] != 0; ++i)
    {
        if(path[i] == '.')
            dotIdx = i;  // Don't break, keep going to find the last one
    }
    
    if(dotIdx == -1)
    {
        // Ptr to null terminator
        *outExt = &path[strlen(path)];
        return File_Unknown;
    }
    
    // Found '.'
    *outExt = &path[dotIdx+1];
    
    for(int i = 0; i < StArraySize(extensionStrings); ++i)
    {
        if(strcmp(*outExt, extensionStrings[i]) == 0)
            return (FileExtension)extensionTypes[i];
    }
    
    return File_Unknown;
}

FilePaths ParseCmdLineArgs(Slice<char*> argStrings)
{
    FilePaths paths;
    
    // First argument is executable name
    int at = 1;
    while(at < argStrings.len)
    {
        String argStr = { argStrings[at], (int64)strlen(argStrings[at]) };
        
        if(argStr.len <= 0 || argStr[0] != '-')
        {
            // This is required to be a file name.
            
            // Find out the extension of the file
            char* extStr = 0;
            auto extType = GetExtensionFromPath(argStr.ptr, &extStr);
            if(extType == File_Ryu)
                paths.srcFiles.Append(argStr.ptr);
            else if(extType == File_Obj)
                paths.objFiles.Append(argStr.ptr);
            else if(extType == File_Unknown)
                EPrint("Unknown file extension '.%s', will be ignored.\n", extStr);
            
            ++at;
            continue;
        }
        
        ++argStr.ptr;
        --argStr.len;
        
        // Use the X macro to parse arguments based on their type, default value, etc.
#define X(varName, string, type, defaultVal, desc) \
if(strcmp(argStr.ptr, string) == 0) { ++at; args.varName = ParseArg<type>(argStrings, &at); continue; }
        
        CmdLineArgsInfo
            
            // If not any other case
        {
            ++at;
            fprintf(stderr, "Unknown command line argument: '%.*s', will be ignored. Use '-h' for more info.\n", (int)argStr.len, argStr.ptr);
        }
#undef X
    }
    
    return paths;
}
