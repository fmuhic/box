#include "stdio.h"
#include "box/hmap.h"

BX_DECLARE_ADD(int);

int main()
{
    int result = bx_add(3, 2);
    printf("Sum is %i", result);
    return 0;
}
