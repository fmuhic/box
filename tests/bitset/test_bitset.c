#include <stdio.h>
#include <assert.h>
#include "test_bitset.h"
#include "box/core.h"
#include "box/bitset.h"
#include "box/hmap.h"

static uint64_t hash_bitset(bx_bitset set) {
    // Use block count as seed
    uint64_t h = (uint64_t)set.block_count;

    // Fast combination loop
    for (uint32_t i = 0; i < set.block_count; ++i) {
        h ^= set.bits[i];
        h *= 0xbf58476d1ce4e5b9ULL; 
    }

    // The Murmur Mixer
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccduLL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53uLL;
    h ^= h >> 33;

    return h;
}

static bool eq_bitset(bx_bitset a, bx_bitset b) {
    if (a.block_count != b.block_count) return false;
    if (a.block_count == 0) return true;
    return memcmp(a.bits, b.bits, a.block_count * sizeof(uint64_t)) == 0;
}

BX_HMAP_DECLARE(bx_bitset, int32_t, bset_i32)
BX_HMAP_SOURCE(bx_bitset, int32_t, bset_i32, hash_bitset, eq_bitset)


void test_bitset_popcount_utility() {
    printf("Running: test_bitset_popcount_utility\n");
    
    // Directly
    assert(bx_pop_count_64(0) == 0);
    assert(bx_pop_count_64(1) == 1);
    assert(bx_pop_count_64(0xFFFFFFFFFFFFFFFFULL) == 64);
    assert(bx_pop_count_64(0x1010101010101010ULL) == 8);

    // Via bitset wrapper
    bx_bitset set;
    bx_bitset_init(&set, 128);
    bx_bitset_set_count_and_clear(&set, 128);
    
    bx_bitset_set_fast(&set, 0);
    bx_bitset_set_fast(&set, 63); // End of block 0
    bx_bitset_set_fast(&set, 64); // Start of block 1
    
    assert(bx_bitset_count(&set) == 3);

    bx_bitset_drop(&set);
}

void test_bitset_basic_ops() {
    printf("Running: test_bitset_basic_ops\n");
    bx_bitset set;
    // Initialize for 64 bits (1 block)
    bx_bitset_init(&set, 64);
    bx_bitset_set_count_and_clear(&set, 64);

    // Initial state
    assert(bx_bitset_get(&set, 10) == false);

    // Set Fast and Get
    bx_bitset_set_fast(&set, 10);
    assert(bx_bitset_get(&set, 10) == true);

    // Clear
    bx_bitset_clear(&set, 10);
    assert(bx_bitset_get(&set, 10) == false);

    bx_bitset_drop(&set);
}

void test_bitset_set_safe_logic() {
    printf("Running: test_bitset_set_safe_logic\n");
    bx_bitset set;
    bx_bitset_init(&set, 8); // Capacity 1 block, count 0

    // Should trigger bx_bitset_grow automatically
    bx_bitset_set_safe(&set, 10);   // Lands in block 0
    bx_bitset_set_safe(&set, 100);  // Lands in block 1
    bx_bitset_set_safe(&set, 1000); // Lands in block 15

    assert(bx_bitset_get(&set, 10) == true);
    assert(bx_bitset_get(&set, 100) == true);
    assert(bx_bitset_get(&set, 1000) == true);
    
    // Check block_count updated correctly (1000 / 64 + 1 = 16)
    assert(set.block_count >= 16);

    bx_bitset_drop(&set);
}

void test_bitset_set_count_and_clear() {
    printf("Running: test_bitset_set_count_and_clear\n");
    bx_bitset set;
    bx_bitset_init(&set, 64);
    
    // Grow and set values
    bx_bitset_set_count_and_clear(&set, 256); // 4 blocks
    bx_bitset_set_fast(&set, 10);
    bx_bitset_set_fast(&set, 200);
    assert(bx_bitset_count(&set) == 2);

    // Clear via set_count_and_clear (should reset all bits to 0)
    bx_bitset_set_count_and_clear(&set, 256);
    assert(bx_bitset_count(&set) == 0);
    assert(bx_bitset_get(&set, 10) == false);

    bx_bitset_drop(&set);
}


void test_bitset_union_logic() {
    printf("Running: test_bitset_union_logic\n");
    bx_bitset a, b;
    bx_bitset_init(&a, 64);
    bx_bitset_init(&b, 64);
    bx_bitset_set_count_and_clear(&a, 64);
    bx_bitset_set_count_and_clear(&b, 64);

    bx_bitset_set_fast(&a, 1);
    bx_bitset_set_fast(&b, 2);

    bx_bitset_union(&a, &b);

    assert(bx_bitset_get(&a, 1) == true);
    assert(bx_bitset_get(&a, 2) == true);
    assert(bx_bitset_count(&a) == 2);

    bx_bitset_drop(&a);
    bx_bitset_drop(&b);
}

void test_bitset_fencepost_errors() {
    printf("Running: test_bitset_fencepost_errors\n");
    bx_bitset set;
    // Exactly two blocks
    bx_bitset_init(&set, 128);
    bx_bitset_set_count_and_clear(&set, 128);

    // Test the "edges" of the 64-bit boundaries
    bx_bitset_set_fast(&set, 63); 
    bx_bitset_set_fast(&set, 64);
    
    assert(bx_bitset_get(&set, 63) == true);
    assert(bx_bitset_get(&set, 64) == true);
    
    // Ensure bit 63 didn't spill into block 1, and bit 64 didn't spill into block 0
    assert((set.bits[0] & (1ULL << 63)) != 0);
    assert((set.bits[1] & (1ULL << 0)) != 0);
    
    bx_bitset_drop(&set);
}

void test_bitset_growth_preservation() {
    printf("Running: test_bitset_growth_preservation\n");
    bx_bitset set;
    bx_bitset_init(&set, 64);
    bx_bitset_set_count_and_clear(&set, 64);

    bx_bitset_set_fast(&set, 10);
    bx_bitset_set_fast(&set, 60);

    // Trigger growth manually
    bx_bitset_grow(&set, 10); // Growth to 10 blocks (640 bits)
    
    // Verify old data is still there after memcpy/realloc
    assert(bx_bitset_get(&set, 10) == true);
    assert(bx_bitset_get(&set, 60) == true);
    assert(bx_bitset_count(&set) == 2);

    bx_bitset_drop(&set);
}

void test_bitset_out_of_bounds_safety() {
    printf("Running: test_bitset_out_of_bounds_safety\n");
    bx_bitset set;
    bx_bitset_init(&set, 64);
    bx_bitset_set_count_and_clear(&set, 64);

    // bx_bitset_get and bx_bitset_clear are designed to handle 
    // indices beyond block_count by returning false/doing nothing.
    assert(bx_bitset_get(&set, 5000) == false);
    
    // This should not crash
    bx_bitset_clear(&set, 5000); 

    bx_bitset_drop(&set);
}

void test_bitset_stress_random_access() {
    printf("Running: test_bitset_stress_random_access\n");
    bx_bitset set;
    bx_bitset_init(&set, 1024);
    bx_bitset_set_count_and_clear(&set, 1024);

    // Pattern: Set every prime-ish interval
    for (uint32_t i = 0; i < 1024; i += 3) {
        bx_bitset_set_fast(&set, i);
    }

    for (uint32_t i = 0; i < 1024; i++) {
        bool expected = (i % 3 == 0);
        if (bx_bitset_get(&set, i) != expected) {
            fprintf(stderr, "Fail at index %u\n", i);
            assert(false);
        }
    }

    bx_bitset_drop(&set);
}

void test_bitset_reinit_cycle() {
    printf("Running: test_bitset_reinit_cycle\n");
    bx_bitset set;
    
    // Cycle 1
    bx_bitset_init(&set, 64);
    bx_bitset_set_safe(&set, 10);
    bx_bitset_drop(&set);
    
    // Cycle 2: Immediately re-init
    bx_bitset_init(&set, 128);
    bx_bitset_set_count_and_clear(&set, 128);
    assert(bx_bitset_get(&set, 10) == false);
    bx_bitset_set_fast(&set, 10);
    assert(bx_bitset_get(&set, 10) == true);
    
    bx_bitset_drop(&set);
}

void test_bitset_as_hmap_key() {
    printf("Running: test_bitset_as_hmap_key\n");

    bx_hmap_bset_i32 map;
    bx_hmap_bset_i32_init(&map);

    // Create three bitsets with different patterns
    bx_bitset s1, s2, s3;
    bx_bitset_init(&s1, 128);
    bx_bitset_set_count_and_clear(&s1, 128);
    bx_bitset_init(&s2, 128);
    bx_bitset_set_count_and_clear(&s2, 128);
    bx_bitset_init(&s3, 128);
    bx_bitset_set_count_and_clear(&s3, 128);

    bx_bitset_set_fast(&s1, 10);
    bx_bitset_set_fast(&s1, 70);

    bx_bitset_set_fast(&s2, 20);
    bx_bitset_set_fast(&s2, 80);

    // s3 is a duplicate of s1 but a different memory object
    bx_bitset_set_fast(&s3, 10);
    bx_bitset_set_fast(&s3, 70);

    // Insert s1 and s2
    bx_hmap_bset_i32_insert(&map, s1, 1001);
    bx_hmap_bset_i32_insert(&map, s2, 2002);
    assert(map.size == 2);

    // Verify retrieval using exact same object
    int32_t* v1 = bx_hmap_bset_i32_get(&map, s1);
    assert(v1 != NULL && *v1 == 1001);

    // Verify retrieval using s3 (different object, identical content)
    // This proves the content-based hashing and equality work.
    int32_t* v3 = bx_hmap_bset_i32_get(&map, s3);
    assert(v3 != NULL && *v3 == 1001);

    // Update value using s3
    bx_hmap_bset_i32_insert(&map, s3, 9999);
    assert(map.size == 2); // Size should not increase
    assert(*bx_hmap_bset_i32_get(&map, s1) == 9999);

    bx_hmap_bset_i32_drop(&map);
    bx_bitset_drop(&s1);
    bx_bitset_drop(&s2);
    bx_bitset_drop(&s3);
}


void run_bitset_tests() {
    printf("\n--- Starting bitset tests ---\n");
    test_bitset_popcount_utility();
    test_bitset_basic_ops();
    test_bitset_set_safe_logic();
    test_bitset_set_count_and_clear();
    test_bitset_union_logic();
    test_bitset_fencepost_errors();
    test_bitset_growth_preservation();
    test_bitset_out_of_bounds_safety();
    test_bitset_stress_random_access();
    test_bitset_reinit_cycle();
    test_bitset_as_hmap_key();
    printf("--- bitset tests passed ---\n\n");
}
