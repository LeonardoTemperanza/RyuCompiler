
#include "base.h"
#include "atom.h"

void Atom_InternStrings(Slice<Slice<ToIntern>> intern)
{
    ProfileFunc(prof);
    
    // Allocate hash table here
    AtomTable table;
    table.stringArena = Arena_VirtualMemInit(GB(4), MB(2));
    table.atomArena = Arena_VirtualMemInit(GB(4), MB(2));
    
    for(int i = 0; i < intern.length; ++i)
    {
        for(int j = 0; j < intern[i].length; ++j)
        {
            Atom* atom = Atom_GetOrAddAtom(&table, intern[i][j].toIntern);
            *intern[i][j].patch = atom;
        }
    }
}

// @temporary @performance
// Just an array to get the system up and running,
// In the future it will be a reasonably performant hash table
Atom* Atom_GetOrAddAtom(AtomTable* table, String str)
{
    Atom* result = 0;
    bool found = false;
    for_array(i, table->atoms)
    {
        if(str == table->atoms[i].string)
        {
            result = &table->atoms[i];
            found = true;
        }
    }
    
    if(!found)
    {
        // This is actually unnecessary
        Atom newAtom;
        char* newStr = Arena_PushStringAndNullTerminate(&table->stringArena, str);
        newAtom.string = newStr;
        table->atoms.Append(&table->atomArena, newAtom);
        result = &table->atoms[table->atoms.length-1];
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
