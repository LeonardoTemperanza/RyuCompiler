
#pragma once

#include "base.h"

struct Atom;
struct ToIntern;
struct AtomTable;
struct AtomEntry;

struct ToIntern
{
    String toIntern;
    uint64 hash;
    Atom** patch;
};

struct Atom
{
    String s;
    
    // Other info (like a lazily allocated null-terminated string
    // if an API, such as LLVM, requires null terminated strings). 
    char* nullTermStr = 0;
};

// Hash table stuff
struct AtomTable
{
    Slice<AtomEntry> entries = 0;
    uint64 primeIdx = 0;
    uint64 numOccupied = 0;
};

struct AtomEntry
{
    bool32 occupied;
    uint64 hash;
    String key;
    Atom* value;
};

void Atom_InternStrings(Slice<Slice<ToIntern>> intern);
Atom* Atom_GetOrAddAtom(AtomTable* table, String str, uint64 hash);
cforceinline uint64 Atom_ProbingScheme(uint64 hash, uint64 iter, uint64 length);
void Atom_EnlargeTable(AtomTable* table);
