
proc putchar(int c)->int c;
proc puts(^char str)->int numChars;
proc malloc(uint64 size)->^raw;
proc free(^raw ptr);

int a = 2;

proc test()
{
    ++a;
    return;
}

proc test2(int num)
{
    int a = 97;
    putchar(a + num);
    
    if(!a)
        return;
    
    putchar(97);
}

proc main()->int
{
    test2(0);
    
    ^char str = cast(^char) malloc(5);
    defer free(str);
    
    *str = 97;
    *(str+1) = 98;
    *(str+2) = 99;
    *(str+3) = 100;
    *(str+4) = 0;
    puts(str);
    
    int prova = *str;
    
    free(str);
    return 0;
}
