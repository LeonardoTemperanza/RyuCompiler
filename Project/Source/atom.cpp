
#include "base.h"
#include "atom.h"

// Array of primes immediately following a power of two
// Using constant values because of the fact that prime number
// calculation can be somewhat expensive. @performance Profiling is needed
const float loadFactor = 0.5f;
const uint64 primes[] =
{
    0ull, 2ull, 3ull, 5ull, 7ull, 11ull, 13ull, 17ull, 23ull, 29ull, 37ull, 47ull, 59ull,
    73ull, 97ull, 127ull, 151ull, 197ull, 251ull, 313ull, 397ull, 499ull, 631ull, 797ull,
    1009ull, 1259ull, 1597ull, 2011ull, 2539ull, 3203ull, 4027ull, 5087ull, 6421ull, 8089ull,
    10193ull, 12853ull, 16193ull, 20399ull, 25717ull, 32401ull, 40823ull, 51437ull, 64811ull,
    81649ull, 102877ull, 129607ull, 163307ull, 205759ull, 259229ull, 326617ull, 411527ull,
    518509ull, 653267ull, 823117ull, 1037059ull, 1306601ull, 1646237ull, 2074129ull, 
    2613229ull, 3292489ull, 4148279ull, 5226491ull, 6584983ull, 8296553ull, 10453007ull, 
    13169977ull, 16593127ull, 20906033ull, 26339969ull, 33186281ull, 41812097ull, 
    52679969ull, 66372617ull, 83624237ull, 105359939ull, 132745199ull, 167248483ull, 
    210719881ull, 265490441ull, 334496971ull, 4247846927ull, 5351951779ull, 6743036717ull, 
    8495693897ull, 10703903591ull, 13486073473ull, 16991387857ull, 21407807219ull, 
    26972146961ull, 33982775741ull, 42815614441ull, 53944293929ull, 67965551447ull, 
    85631228929ull, 107888587883ull, 135931102921ull, 171262457903ull, 215777175787ull, 
    271862205833ull, 342524915839ull, 431554351609ull, 543724411781ull, 685049831731ull, 
    863108703229ull, 1087448823553ull, 1370099663459ull, 1726217406467ull, 2174897647073ull, 
    2740199326961ull, 3452434812973ull, 4349795294267ull, 5480398654009ull, 6904869625999ull, 
    8699590588571ull, 10960797308051ull, 13809739252051ull, 17399181177241ull, 
    21921594616111ull, 27619478504183ull, 34798362354533ull, 43843189232363ull, 
    55238957008387ull, 69596724709081ull, 87686378464759ull, 110477914016779ull, 
    139193449418173ull, 175372756929481ull, 220955828033581ull, 278386898836457ull, 
    350745513859007ull, 441911656067171ull, 556773797672909ull, 701491027718027ull, 
    883823312134381ull, 1113547595345903ull, 1402982055436147ull, 1767646624268779ull, 
    2227095190691797ull, 2805964110872297ull, 3535293248537579ull, 4454190381383713ull, 
    5611928221744609ull, 7070586497075177ull, 8908380762767489ull, 11223856443489329ull, 
    14141172994150357ull, 17816761525534927ull, 22447712886978529ull, 28282345988300791ull, 
    35633523051069991ull, 44895425773957261ull, 56564691976601587ull, 71267046102139967ull, 
    89790851547914507ull, 113129383953203213ull, 142534092204280003ull, 
    179581703095829107ull, 226258767906406483ull, 2873307249533267101ull, 
    3620140286502504283ull, 4561090950536962147ull, 5746614499066534157ull, 
    7240280573005008577ull, 9122181901073924329ull, 11493228998133068689ull, 
    14480561146010017169ull, 18446744073709551557ull
};

void Atom_InternStrings(Slice<Slice<ToIntern>> intern)
{
    ProfileFunc(prof);
    
    constexpr uint64 minPrimeIdx = 14;
    AtomTable table;
    table.primeIdx = minPrimeIdx;
    table.entries.ptr = (AtomEntry*)malloc(sizeof(AtomEntry) * primes[minPrimeIdx]);
    table.entries.length = primes[minPrimeIdx];
    
    for(int i = 0; i < intern.length; ++i)
    {
        for(int j = 0; j < intern[i].length; ++j)
        {
            Atom* atom = Atom_GetOrAddAtom(&table, intern[i][j].toIntern, intern[i][j].hash);
            *intern[i][j].patch = atom;
        }
    }
}

Atom* Atom_GetOrAddAtom(AtomTable* table, String str, uint64 hash)
{
    bool added = false;
    
    for(int i = 0; ; i++)
    {
        auto idx = Atom_ProbingScheme(hash, i, table->entries.length);
        
        break;
    }
    
    if(added) ++table->numOccupied;
    
    // Check if table needs to be resized
    if((float)table->numOccupied / table->entries.length > loadFactor)
        Atom_EnlargeTable(table);
    
    return 0;
}

cforceinline uint64 Atom_ProbingScheme(uint64 hash, uint64 iter, uint64 length)
{
    // This is a quadratic probing scheme
    return (hash + iter * iter) % length;
}

void Atom_EnlargeTable(AtomTable* table)
{
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
    auto oldEntries = table->entries.ptr;
    defer(free(oldEntries));
    
    for_array(i, table->entries)
    {
        auto& entry = table->entries[i];
        if(!entry.occupied) continue;
        
        // Add to newEntries, with collision resolution but without resizing
        bool added = false;
        // TODO
    }
    
    table->entries.ptr = newEntries;
    table->entries.length = newSize;
}
