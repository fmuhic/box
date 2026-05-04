#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include "hmap/test_hmap.h"

int main() {
    printf("=== Box Test Suite ===\n");

    run_hmap_tests();

    printf("=== All Tests Passed ===\n");
    return 0;
}
