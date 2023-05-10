
struct prova
{
    // Qua ci vanno varie dichiarazioni
    int info;
    ^prova next;
}

proc main()->(int, float)
{
    prova testprova;

    ^int iptr;
    ^float fptr;
    iptr = fptr;

    ^int testintptr;
    ^float testfloatptr;
    float testfloat;
    int testint;
    testintptr + testint;
    testint + testintptr;
    testfloat - testint;
    3 + 4 * 2 / 2 % 3;
    //p + 2;

    //int prova;  // first
    //int prova;  // second

    int provaint;
    //float prova = 2 + provaint + 5;

    prova testprova2;

    while(int p);

    prova p;

    //do; while(p);

    if(2);

    //if(prova p);

    if(float f = 3.0)
	f + 4;

    if(int i = 0) { testprova.info + i; }

    //if(prova);

    if(int i) { i + 2; }

    //if(prova) {}

    float prova2 = provaint;
    float32 prova3;
    float prova4;
    //float prova4;
}