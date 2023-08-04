
#pragma once

#include "base.h"

struct Atom;
struct ToIntern;
struct AtomTable;
struct AtomEntry;

struct ToIntern
{
    String toIntern;
    Atom** patch;
};

struct Atom
{
    char* string = 0;
    
    // Other stuff in the future
};

// Hash table stuff
struct AtomTable
{
    Arena* arena;
    Array<AtomEntry> entries;
};

struct AtomEntry
{
    char* string = 0;  // Null-terminated
    Atom* value;
};

void Atom_InternStrings(Array<Array<ToIntern>> intern);
Atom* Atom_GetOrAddAtom(AtomTable* table, String str, Arena* atomArena);
uint32 Atom_StringHash(String str);
void Atom_ChangeSizeAndRehash(AtomTable table);