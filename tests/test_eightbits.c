#include "stdio.h"
#include "b8/hmap.h"

B8_DECLARE_ADD(int);

int main()
{
    int result = b8_add(3, 2);
    printf("Sum is %i", result);
    return 0;
}
