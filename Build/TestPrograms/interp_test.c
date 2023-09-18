
struct teststr
{
    int a; int b;
}


proc add(int a, int b, teststr str2)->teststr
{
    teststr str;
    return str;
}

proc add_int(int a, int b)->float
{
    return a+b + 0.2;
}

proc add_float()->int
{
    return 40;
}

proc main()->int
{
    //float f = 3.4;
    //int i = f + 2;
    
    teststr str;
    teststr str2;
    //*str;
    str = str2;
    //str + 2;
    
    float f = 4.8d * cast(int) 5.3;
    int i = f;
    
    //teststr input;
    //teststr str = add(3,4, input);
    
    //float f = 3.4 +
    //return 3.4;
    
    return add_int(3.4d, 4.7);
    
    //teststr str;
    //add(2, 3, str);
    //return 2+6*2+ add_int(2+2,2+3);
}
