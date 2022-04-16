#include "error.h"
#include <stdio.h>

void panic(const char *msg)
{
    printf("panic: %s\n", msg);
    while(1);
}
