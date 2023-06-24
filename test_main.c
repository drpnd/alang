#include <stdio.h>
int func();
int func2(int);
int func3();
int
main()
{
    printf("%x\n", func2(4));
    printf("%x\n", func3());
    printf("%x\n", func3());
    printf("%x\n", func3());
}
