
struct String
{
    ^char ptr;
    uint64 length;
}

struct Person
{
    String name;
    String lastName;
    
    uint8 age;
    
    ^Group belongsTo;
}

proc SwapNames(^Person p)->Person res
{
    String tmp = p.name;
    p.name = p.lastName;
    p.lastName = tmp;
}

struct Group
{
    ^Person pArray;
    int numPeople;
}

struct Node
{
    int info;
    ^Node next;
}

struct LinkedList
{
    ^Node first;
    ^Node last;
}

operator +(Person p1, Person p2)->int
{
    return p1.age + p2.age;
}

int prova2;

proc main(int argCount, ^^char argValue)->int
{
    Society society;
    
    ^Person p;
    p.name.length = p.name.length++;
    p.belongsTo = &society;
    
    for(int i = 0; i < p.name.length; ++i)
    {
        
    }
    
    p.name.length++;
    
    int i = 0;
    
    if(argCount > 0);
    
    return argCount;
}

proc test2(int num)
{
    num + 2;
}

proc test(int num)->int
{
    LinkedList testprova;
    ^int testint = 0;
    testprova.first = cast(^Node)testint;
    testprova.last = testprova.first;
    
    //^int testint2 = testprova;
    
    int a;
    ^int aptr;
    int b;
    int c;
    
    [3]^int arrayofintptrs;
    
    a = b = 2;
    
    //testprova = testint;
    
    //for(int i = 0; i < *aptr; ++i);
    
    //if(testprova);
    
    //testprova.next = testprova + 5 - prova2;
    
    //test(testprova) + testprova;
    
    //test2(num) + test2(num);  // Error: operation between 2 'none' types
    
    return 0;
    
    //return testprova;
}

proc Fibonacci(int num)->int
{
    if(num <= 0) return 0;
    if(num <= 1) return 1;
    return Fibonacci(num - 1) + Fibonacci(num - 2);
    
    float b;
    return b;
}

proc FibonacciIterative(int num)->int
{
    //int num;
    int last = 1;
    int secondLast = 0;
    for(int i = 1; i <= num; i = i+1)
    {
        int current = last + secondLast;
        secondLast = last;
        last = current;
    }
    
    return last;
}