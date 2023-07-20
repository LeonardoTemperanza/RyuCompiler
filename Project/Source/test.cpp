
// Testing semantic properties of c++

struct teststruct
{
    int a;
};

int operator +(teststruct t, teststruct t2);

void test()
{
    // Casts
#if 0
    bool b = 0;
    int i = 0;
    float f = 0;
    void* vptr = 0;
    double* dptr = 0;
    int iarr[4] = {0,0,0,0};
    teststruct s = {0};
    
    (int)b;
    (float)b;
    (void*)b;
    (double*)b;
    (teststruct)b;
    int* prova = (int*)b;     // Why does this work?
    (bool)i;
    (float)i;
    (void*)i;
    (double*)i;
    (teststruct)b;
    (bool)f;
    (int)f;
    //(void*)f;     // error
    //(double*)f;   // error
    (bool)vptr;
    (int)vptr;
    //(float)vptr;  // error
    (void*)vptr;
    (double*)vptr;
    (bool)dptr;
    (int)dptr;
    //(float)vptr;  // error
    (void*)dptr;
    (double*)dptr;
    
#endif
}