#include <stdio.h>
#include "hmap/test_hmap.h"
#include "darray/test_darray.h"
#include "bitset/test_bitset.h"

int main() {
    printf("=== Box Test Suite ===\n");

    run_hmap_tests();
    run_darray_tests();
    run_bitset_tests();

    printf("=== All Tests Passed ===\n");
    return 0;
}
