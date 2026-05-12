#include <stdio.h>
#include "hmap/test_hmap.h"
#include "darray/test_darray.h"
#include "bitset/test_bitset.h"
#include "spset/test_spset.h"

int main() {
    printf("=== Box Test Suite ===\n");

    run_hmap_tests();
    run_darray_tests();
    run_bitset_tests();
    run_spset_tests();

    printf("=== All Tests Passed ===\n");
    return 0;
}
