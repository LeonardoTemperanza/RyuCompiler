
#pragma once

#include "base.h"

// Array of primes immediately following a power of two
// Using constant values because of the fact that prime number
// calculation can be somewhat expensive. @performance Profiling is needed
const float loadFactor = 0.5f;
const uint64 primes[] =
{
    73ull, 151ull, 313ull, 631ull, 1259ull, 2539ull, 5087ull, 10193ull,
    20399ull, 32401ull, 81649ull, 163307ull, 326617ull, 653267ull, 1306601ull, 2613229ull,
    5226491ull, 10453007ull, 20906033ull, 41812097ull, 83624237ull, 167248483ull, 334496971ull, 6743036717ull,
    13486073473ull, 26972146961ull, 53944293929ull, 107888587883ull, 215777175787ull, 
    431554351609ull, 863108703229ull, 1726217406467ull, 3452434812973ull,
    6904869625999ull, 13809739252051ull, 27619478504183ull, 55238957008387ull,
    110477914016779ull, 220955828033581ull, 441911656067171ull, 883823312134381ull,
    1767646624268779ull, 3535293248537579ull, 7070586497075177ull, 14141172994150357ull,
    28282345988300791ull, 56564691976601587ull, 113129383953203213ull, 226258767906406483ull,
    3620140286502504283ull, 7240280573005008577ull, 14480561146010017169ull
};

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
    Arena* atomArena;
    
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

AtomTable InitAtomTable(Arena* atomArena);
Atom* Atom_GetOrAddAtom(AtomTable* table, String str, uint64 hash);
cforceinline uint64 Atom_ProbingScheme(uint64 hash, uint64 iter, uint64 length);
void Atom_GrowTable(AtomTable* table);
