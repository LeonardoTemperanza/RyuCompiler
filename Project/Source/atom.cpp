
#include "base.h"
#include "atom.h"

void Atom_InternStrings(Array<Array<ToIntern>> intern)
{
    ProfileFunc(prof);
    ScratchArena scratch;
    
    // @leak This will "leak", doesn't really matter for now
    // or in general
    Arena atomArena = Arena_VirtualMemInit(GB(4), MB(2));
    
    // Allocate hash table here
    AtomTable table = { scratch, { 0, 0 } };
    
    for(int i = 0; i < intern.length; ++i)
    {
        for(int j = 0; j < intern[i].length; ++j)
        {
            Atom* atom = Atom_GetOrAddAtom(&table, intern[i][j].toIntern, &atomArena);
            *intern[i][j].patch = atom;
        }
    }
}

// @temporary @performance
// Just an array to get the system up and running,
// In the future it will be a reasonably performant hash table
Atom* Atom_GetOrAddAtom(AtomTable* table, String str, Arena* atomArena)
{
    Atom* result = 0;
    bool found = false;
    for_array(i, table->entries)
    {
        if(str == table->entries[i].string)
        {
            result = table->entries[i].value;
            found = true;
        }
    }
    
    if(!found)
    {
        // This is actually unnecessary
        Atom* newAtom = Arena_AllocVar(atomArena, Atom);
        char* newStr = Arena_PushStringAndNullTerminate(atomArena, str);
        newAtom->string = newStr;
        AtomEntry entry = { newStr, newAtom };
        table->entries.Append(table->arena, entry);
        result = newAtom;
    }
    
    return result;
}

uint32 Atom_StringHash(String str)
{
    return 0;
}

void Atom_ChangeSizeAndRehash(AtomTable table)
{
    
}
