
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

struct Message
{
    MessageKind kind;
    MessagePhase phase;
    
    Token* token;
    String fileName;
    
    // N-ary tree. Children are nodes that continue
    // this message. Things like:
    // 1: "incorrect number of arguments in function call"
    // 2 (based on 1): "Here's the function call signature"
    // It's possible to have multiple levels of nesting,
    // though i'm not sure if that would be any useful
    Message* first;
    Message* next;
    Message* prev;
};

struct MessageDrawCmd
{
    bool showFileName;
    //others
};

Message* AppendMessage(MessageKind kind, MessagePhase phase, Arena* dst);
Message* AppendMessage(MessageKind kind, MessagePhase phase, Message* basedOn, Arena* dst);

// Message renderer
void SortMessages();
void ShowMessages();
void ShowMessages(Message* message, MessageDrawCmd cmd);

// Stuff like "render cmd line args" should be here maybe?
