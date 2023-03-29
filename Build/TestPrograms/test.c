
// Commento singola linea

// Need to add: types, more files support, compound types, pointers

proc main()->prova p
{
    //test1();

    //^int prova = 0;
    //^^[2+3+4+5]^prova(2, 3) a;
    ^[2]^^test1(2) a;

    // Types are also expressions, in a way.
    // I think syntactically it works out in the end
    // but the semantic analysis will then analyze the expression
    // and see if it's actually a type.
    // Ok nvm, the best thing is to probably try parsing the expression
    // first and then convert to what it actually is.
    // It's most likely going to be an expression anyway.

    //test1(2, 2) prova;
    return test1();
}

/*
proc test(a, b, c, d)
{
    return 0;
}
*/

proc test1()
{
    return 2;
}

proc test2()
{
    return 3;
}

proc test3()
{
    return 4;
}