
#include "base.h"
#include "atom.h"

void Atom_InternStrings(Array<Array<ToIntern>> intern)
{
    ProfileFunc(prof);
    ScratchArena scratch;
    
    // Allocate hash table here
    AtomTable table = { { 0, 0 } };
    
    for(int i = 0; i < intern.length; ++i)
    {
        for(int j = 0; j < intern[i].length; ++j)
        {
            // TODO: actually call a procedure here
            Atom* atom = Atom_GetOrAddAtom(table, intern[i][j].toIntern);
            *intern[i][j].patch = atom;
        }
    }
}

Atom* Atom_GetOrAddAtom(AtomTable table, String str)
{
    return 0;
}

uint32 Atom_StringHash(String str)
{
    return 0;
}

void Atom_ChangeSizeAndRehash(AtomTable table)
{
    
}