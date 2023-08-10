
proc add()->int
{
    return 2+2;
}

proc main()->int
{
    return add() + add();
}

proc add2()->int
{
    int a = 2;
    int b = 2+2;
    
    a = 2 + 2;
    b = 2*(2+2);
    
    if(a)
    {
        b = 2*(2+2)+2;
        2*(2+2);
    }
    else
        2*2;
    
    return a+b;
}

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