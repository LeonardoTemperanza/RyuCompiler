# Ryu Programming Language

**Warning:** The project only works on Windows at the moment, though it will be supported eventually.

**Warning:** This project is still in development and is not yet fully usable (if not for small toy projects). It's not really meant to be fully-fledged and compete with other programming languages right now. It was originally meant to be a valuable learning experience for me, and hopefully others could learn from this too.

## Language Description & Features

**Ryu** is a general-purpose, procedural, statically-typed programming language. It features the common operators you might want to use, control-flow structures as well as compound types. It's fully compatible with **C**, and automatically links with the **C Runtime Library**, so it can be used for common functionality. One of the most advanced features of the language is the possibility to write declarations/definitions in any order the user would like. This gets rid of the tedium and error-proneness of writing forward declarations by hand, and in general removes the need for header files. Function declarations are of course still needed when interfacing with separate object files. The compiler is also built in such a way so that it could be easily modified in the future to run arbitrary code at compile time (it's an upcoming feature). More specifically, it translates the **Abstract Syntax Tree** into an intermediate bytecode format before converting to the backend specific **Intermediate Representation**.

## Example Program

Here is an example which dynamically constructs a linked-list of nodes and then prints it. (It's included in `/Examples`, among others)

```c 
// When importing modules is implemented, there will be
// a "cstd.ryu" file that will include the c standard library
proc malloc(uint64 size)->^raw;
proc putchar(int c)->int c;

proc main()->int
{
    LinkedList ll;
    // Initting a linked list with InitLinkedList triggers a bug in Tilde Backend.
    ll.first = ll.last = 0;
    AddNode(&ll, 97);
    AddNode(&ll, 98);
    AddNode(&ll, 99);
    AddNode(&ll, 100);
    AddNode(&ll, 101);
    
    PrintLinkedList(ll);
    
    return 0;
}

struct LinkedList
{
    ^Node first;
    ^Node last;
}

struct Node
{
    ^Node next;
    int64 a;
    int64 b;
    int64 id;
}

proc InitLinkedList()->LinkedList
{
    LinkedList ll;
    ll.first = 0;
    ll.last = 0;
    return ll;
}

proc AddNode(^LinkedList ll, int16 id)
{
    ^Node newNode = cast(^Node) malloc(100);
    newNode.id = id;
    newNode.next = 0;
    
    if(ll.first == 0)
    {
        ll.first = newNode;
        ll.last = newNode;
        return;
    }
    
    ll.last.next = newNode;
    ll.last = newNode;
}

proc PrintLinkedList(LinkedList list)
{
    for(^Node n = list.first; n; n = n.next)
    {
        putchar(n.id);
    }
}
```

The example above shows some of the things that are possible with the language, such as handling pointers, type casts, compound-types and control-flow constructs such as `if` and `for` statements. The syntax of the language is similar to that of **C**, though there are some exceptions. For instance, the language does allow you to return multiple values in a single procedure (**note:** It's not fully implemented at the moment but it is set up in that way). As shown, since the compiler itself can handle and resolve the intricate dependencies and can figure out the order in which to compile each entity, it's possible to write code in a natural reading order without having to manually deal with the order of declarations.

## Build and Run

**Note:** This applies to Windows only at the moment. Building on Linux will be possible in the future.

First of all, clone the repository:
```bat 
git clone --recurse-submodules https://github.com/Username-Leon/RyuCompiler
```

#### 1) Install Visual Studio
Make sure you have Visual Studio installed. This is not just required for building, as the compiler will need `link.exe` from Visual Studio to produce the executable (unless `-tilde_linker` is specified, but that is experimental). The rest of the commands listed are assumed to be executed inside the **X64 Visual Studio Command Prompt**. If you're not familiar with it, [here's how you can find it](https://learn.microsoft.com/en-us/visualstudio/ide/reference/command-prompt-powershell?view=vs-2022).

#### 2) Build Tilde Backend 
This project is a front-end only, and uses the [Tilde Backend](https://github.com/RealNeGate/Cuik) (as well as a platform-specific linker) to generate the final executable.
After having installed a Lua interpreter, just run the following command in the `/Project/Source/tilde_backend/Cuik` directory:

```bat 
lua build.lua -tb
```

The generated tb.lib file (in the bin directory) should then be copied into `/Project/Libs`.

#### 3) Build Ryu Compiler

The project itself can simply be built with the `build.bat` script included in `/Project`:
```bat 
build.bat
```

After building, `ryu.exe` will be in the `/Build` directory. It can be added to the user's path, so that it can be run from anywhere.