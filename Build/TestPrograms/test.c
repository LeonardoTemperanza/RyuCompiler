
// Commento singola linea

// Need to add: types, more files support, compound types, pointers

proc main()
{
    //i = 0;

    // Bugs in the parser?
    2<3+4;

    //^int abcdef = 2+3*4+5+-;

    {
        int abcdef;
        ^int prova = abcdef;
    }

    ^int prova = abcdef;

    return prova;
}