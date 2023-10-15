
#include "base.h"
#include "atom.h"

AtomTable InitAtomTable(Arena* atomArena)
{
    AtomTable t;
    t.atomArena = atomArena;
    const uint64 numBytes = sizeof(AtomEntry) * primes[0];
    t.entries.ptr = (AtomEntry*)malloc(numBytes);
    t.entries.length = primes[0];
    memset(t.entries.ptr, 0, numBytes);
    
    return t;
}

Atom* Atom_GetOrAddAtom(AtomTable* table, String str, uint64 hash)
{
    ProfileFunc(prof);
    
    bool exit = false;
    bool added = false;
    Atom* res = 0;
    for(int i = 0; !exit; i++)
    {
        auto idx = Atom_ProbingScheme(hash, i, table->entries.length);
        
        if(table->entries[idx].occupied)
        {
            // Exact check
            if(table->entries[idx].key == str)
            {
                res = table->entries[idx].value;
                exit = true;
            }
        }
        else
        {
            // Insert entry
            auto newAtom = Arena_AllocVar(table->atomArena, Atom);
            newAtom->s = str;
            newAtom->nullTermStr = 0;
            
            table->entries[idx].occupied = true;
            table->entries[idx].hash     = hash;
            table->entries[idx].key      = str;
            table->entries[idx].value    = newAtom;
            
            exit = true;
            res = newAtom;
            added = true;
        }
    }
    
    if(added) ++table->numOccupied;
    
    // Check if table needs to be resized
    if((float)table->numOccupied / table->entries.length > loadFactor)
        Atom_GrowTable(table);
    
    return res;
}

cforceinline uint64 Atom_ProbingScheme(uint64 hash, uint64 iter, uint64 length)
{
    // This is a quadratic probing scheme
    return (hash + iter * iter) % length;
}

void Atom_GrowTable(AtomTable* table)
{
    ProfileFunc(prof);
    
    int newSize = 0;
    const int numPrimes = StArraySize(primes);
    
    int oldSize = primes[numPrimes];
    ++table->primeIdx;
    
    // If the prime idx is more than the number of prime numbers stored,
    // just start multiplying by two
    // TODO: Ideally you would calculate the prime nums at runtime
    if(table->primeIdx >= numPrimes)
    {
        newSize = primes[numPrimes-1];
        for(int i = numPrimes; i <= table->primeIdx; ++i)
            newSize *= 2;
    }
    else
        newSize = primes[table->primeIdx];
    
    // Repopulate table
    auto newEntries = (AtomEntry*)malloc(sizeof(AtomEntry) * newSize);
    memset(newEntries, 0, sizeof(AtomEntry) * newSize);
    auto oldEntries = table->entries.ptr;
    defer(free(oldEntries));
    
    for_array(i, table->entries)
    {
        // Add each entry
        auto& entry = table->entries[i];
        if(!entry.occupied) continue;
        
        bool exit = false;
        for(int i = 0; !exit; ++i)
        {
            uint64 idx = Atom_ProbingScheme(entry.hash, i, newSize);
            if(newEntries[idx].occupied) continue;
            
            // Add entry
            newEntries[idx] = entry;
            exit = true;
        }
    }
    
    table->entries.ptr = newEntries;
    table->entries.length = newSize;
}
