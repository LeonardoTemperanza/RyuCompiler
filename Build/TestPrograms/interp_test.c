
struct teststr
{
    int a; int b;
}


proc add(int a, int b, teststr str2)->teststr
{
    teststr str;
    return str;
}

/*
proc add2()->int
{
    int a = 2-2;
    int b = 2+2;
    
    if(a)
    {
        int c;
        b = 2*(2+2)+2;
    }
    else
        b = 2*2;
    
    return a+b;
}
*/

proc main()->int
{
    teststr str;
    //str = add(2, 3, str);
    add(2, 3, str);
    return 0;
    
    //return add2();
    //return add() + add();
}

/*
proc prova()->(int, float)
{
    int testint, float testfloat = prova();
    
    testfloat += 2;
    
    return 3, 4;
}
*/

/*
proc add(int i, int j)->int
{
    i = 2;
    
    return i;
}
*/

/*proc Fibonacci(int i)->int
{
    if(i <= 1) return 1;
    if(i <= 2) return 2;
    return Fibonacci(i-1) + Fibonacci(i-2);
}
*/
