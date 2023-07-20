
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
};

// Hash table stuff
struct AtomTable
{
    Array<AtomEntry> entries;
};

struct AtomEntry
{
    char* string = 0;  // Null-terminated
    Atom* value;
};

void Atom_InternStrings(Array<Array<ToIntern>> intern);
Atom* Atom_GetOrAddAtom(AtomTable table, String str);
uint32 Atom_StringHash(String str);
void Atom_ChangeSizeAndRehash(AtomTable table);