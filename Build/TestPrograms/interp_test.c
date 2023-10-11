
proc putchar(int c)->int c;
proc puts(^char str)->int numChars;

proc free(^raw ptr);

proc fib(int n)->int
{
    if(n < 2) return n;
    int n1 = n + 1;
    return fib(n1) + fib(n1-1);
}

proc main()->int
{
    //int a = 2;
    
    int b = 3+4+5;
    
    b += 3;
    
    ^char string = cast(^char) malloc(5);
    defer free(string);
    
    Person p;
    
    Node n;
    int a = 3;
    //n.info += a;
    
    return 5;
}

proc malloc(uint64 size)->^raw;

struct Node
{
    ^Node next;
    Person p;
}

struct Person
{
    int64 id;
    Town t;
}

struct Town
{
    String name;
    int64 numInhabitants;
}

struct String
{
    ^char ptr;
    int64 length;
}

proc test()->int
{
    malloc(10);
    return 10;
}

/*

proc test5(int num)
{
    switch(num)
    {
        case 0: putchar(97 + 4);
        case 1: { putchar(97 + 5); fallthrough; }
        case 2: putchar(97 + 10);
        case 3: putchar(97 + 11);
        case 4: putchar(97 + 12);
        default: putchar(97 + 13);
    }
}

proc main()->int
{
    return 0;
}

/*
int a = 2;

proc test()
{
    ++a;
    return;
}

proc test2(int num)
{
    int a = 97;
}

proc test3(int num)->(int, float)
{
    return 3, 4.5;
}

proc test4(int num)->(int, float)
{
    return 2, 6.9;
}

proc test5(int num)->int
{
    int res = -1;
    switch(num)
    {
        case 0: res = 2;
        case 1: res = 5;
        case 2: res = 10;
        case 3: res = 61;
        case 4: res = 70;
        default: res = 80;
    }
    
    return res;
}

proc main()->int
{
    test2(0);
    
    test5(2);
    
    int i, float f = test3(2);
    
    ^char str = cast(^char) malloc(5);
    defer free(str);
    
    *str = 97;
    *(str+1) = 98;
    *(str+2) = 99;
    *(str+3) = 100;
    *(str+4) = 0;
    
    puts(str);
    return 0;
}
