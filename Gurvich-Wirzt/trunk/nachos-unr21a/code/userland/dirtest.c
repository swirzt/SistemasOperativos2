#include "syscall.h"
#include "lib.c"

int main()
{
    Ls();
    Mkdir("Test");
    Cd("Test");
    Ls();
    return 0;
}