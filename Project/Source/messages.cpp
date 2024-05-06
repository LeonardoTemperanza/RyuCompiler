
#include "messages.h"

static Message firstMessage;
static Message* lastMessage;

Message* AppendMessage(MessageKind kind, MessagePhase phase, Arena* dst)
{
    return AppendMessage(kind, phase, nullptr, dst);
}

Message* AppendMessage(MessageKind kind, MessagePhase phase, Message* parent, Arena* dst)
{
    Message msg = {0};
    msg.kind    = kind;
    msg.phase   = phase;
    
    Message* newMsg = nullptr;
    
    if(firstMessage.kind == MesKind_None)
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
    
    if(parent)
    {
        if(!parent->first)
            parent->first = newMsg;
        else
        {
            Message* lastChild = parent->first;
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
    Slice<String> fileNames = {0};
    
    // Find the list of files
    {
        for(Message* msg = &firstMessage; msg; msg = msg->next)
        {
            bool isNew = true;
            for(int i = 0; i < fileNames.len; ++i)
            {
                if(fileNames[i] == msg->fileName)
                {
                    isNew = false;
                    break;
                }
            }
            
            if(isNew) fileNames.Append(scratch, msg->fileName);
        }
    }
    
    // Sort the files lexicographically
    {
        String tmp;
#define Tmp_Less(i, j) i < j
#define Tmp_Swap(i, j) tmp = fileNames[i], fileNames[i] = fileNames[j], fileNames[j] = tmp 
        QSORT(fileNames.len, Tmp_Less, Tmp_Swap);
#undef Tmp_Swap
#undef Tmp_Less
    }
    
    // For each file in the sorted array, do a sorting pass
    {
        for(int i = 0; i < fileNames.len; ++i)
        {
            
        }
    }
}

void ShowMessages()
{
    ShowMessages(&firstMessage, {0});
}

void ShowMessages(Message* message, MessageDrawCmd cmd)
{
    // Draw message
    printf("Show message");
    
    if(0)
    {
        ShowMessages(message->first, {0});
        ShowMessages(message->next, {0});
    }
}
