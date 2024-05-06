
#include "messages.h"

static Message firstMessage;
static Message* lastMessage;

Message* AppendMessage(MessageKind kind, MessagePhase phase, Arena* dst)
{
    return AppendMessage(kind, phase, nullptr);
}

Message* AppendMessage(MessageKind kind, MessagePhase phase, Message* basedOn, Arena* dst)
{
    Message msg = {0};
    msg.kind    = kind;
    msg.phase   = phase;
    msg.basedOn = basedOn;
    
    if(firstMessage.messageKind == MesKind_None)
    {
        firstMessage = msg;
        lastMessage  = &firstMessage;
    }
    else
    {
        auto newMsg = Arena_AllocVar(dst, Message);
        *newMsg = msg;
        lastMessage->next = newMsg;
        lastMessage = newMsg;
    }
}

void ShowMessages()
{
    
    for(Message* msg = &firstMessage, msg; msg = msg->next))
    {
    }
}

void RenderMessage()
{
    
}
