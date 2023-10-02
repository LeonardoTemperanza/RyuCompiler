
#include <stdio.h>

void print_stuff(int n)
{
    for(int i = 0; i < n; ++i)
    {
        printf("stuff\n");
    }
}

int main()
{
    char* str = (char*)malloc(5);
    *str = 98;
    *(str+1) = 99;
    *(str+2) = 100;
    *(str+3) = 101;
    *(str+4) = 0;
    puts(str);
    return 0;
}