
#pragma once

#include "base.h"
#include "lexer.h"

enum MessageKind
{
    MesKind_None = 0,  // Sentinel value, used for "null" messages
    MesKind_Error,
    MesKind_Warning,
    MesKind_Log
};

enum MessagePhase
{
    MesPhase_Syntax = 0,
    MesPhase_Semantics
};

enum MessageFlags
{
    Mes_Rendered = 1 << 0,
};

struct Message
{
    uint8 flags;
    
    MessageKind kind;
    MessagePhase phase;
    
    Token* token;
    
    Message* first;
    Message* next;
};

struct MessageDrawCmd
{
    bool showFileName;
    //others
};

Message* AppendMessage(MessageKind kind, MessagePhase phase);
Message* AppendMessage(MessageKind kind, MessagePhase phase, Message* basedOn);

// Message renderer
void ShowMessages();
void RenderMessage();