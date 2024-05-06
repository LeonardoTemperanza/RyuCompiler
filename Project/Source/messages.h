
#pragma once

#include "base.h"
#include "lexer.h"

enum MessageKind
{
    MsgKind_None = 0,  // Sentinel value, used for "null" messages
    MsgKind_Error,
    MsgKind_Warning,
    MsgKind_Log
};

enum MessagePhase
{
    MsgPhase_Syntax = 0,
    MsgPhase_Semantics
};

enum MessageContentKind
{
    Msg_None = 0,
    Msg_UnexpectedToken,
    Msg_IncompatibleTypes,
    Msg_IncompatibleReturns,
    Msg_CannotDereference,
    Msg_CannotConvertToScalar,
    Msg_CannotConvertToIntegral,
    Msg_LookAtProcDecl,
    Msg_SpecializedMessage
};

struct MessageContent
{
    MessageContentKind contentKind;
    char* message;
    TokenKind token;
    
};

struct Message
{
    MessageKind kind;
    MessagePhase phase;
    
    Token* token;
    String fileName;
    String fileContents;
    
    MessageContent content;
    
    // N-ary tree. Children are nodes that continue
    // this message. Things like:
    // 1: "incorrect number of arguments in function call"
    // 2 (child of 1): "Here's the procedure call signature"
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
