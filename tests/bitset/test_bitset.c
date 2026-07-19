#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "test_bitset.h"
#include "box/core.h"
#include "box/bitset.h"
#include "box/hmap.h"

static uint64_t hash_bitset(bx_bitset set)
{
    uint64_t h = (uint64_t)set.block_count;

    for (uint32_t i = 0; i < set.block_count; ++i)
    {
        h ^= set.bits[i];
        h *= 0xbf58476d1ce4e5b9ULL;
    }

    h ^= h >> 33;
    h *= 0xff51afd7ed558ccduLL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53uLL;
    h ^= h >> 33;

    return h;
}

static bool eq_bitset(bx_bitset a, bx_bitset b)
{
    if (a.block_count != b.block_count)
    {
        return false;
    }
    if (a.block_count == 0)
    {
        return true;
    }
    return memcmp(a.bits, b.bits, a.block_count * sizeof(uint64_t)) == 0;
}

BX_HMAP_DECLARE(bx_bitset, int32_t, bset_i32, hash_bitset, eq_bitset)

void test_bitset_popcount_utility(void)
{
    printf("Running: test_bitset_popcount_utility\n");

    assert(bx_pop_count_64(0) == 0);
    assert(bx_pop_count_64(1) == 1);
    assert(bx_pop_count_64(0xFFFFFFFFFFFFFFFFULL) == 64);
    assert(bx_pop_count_64(0x1010101010101010ULL) == 8);

    bx_bitset set;
    bx_bitset_init_capacity(&set, 128);
    bx_bitset_set_count_and_clear(&set, 128);

    bx_bitset_set_fast(&set, 0);
    bx_bitset_set_fast(&set, 63); // End of block 0
    bx_bitset_set_fast(&set, 64); // Start of block 1

    assert(bx_bitset_popcount(&set) == 3);

    bx_bitset_drop(&set);
}

void test_bitset_init_defaults(void)
{
    printf("Running: test_bitset_init_defaults\n");

    // Plain init allocates nothing, matching every other container
    bx_bitset set;
    bx_bitset_init(&set);
    assert(set.bits == NULL);
    assert(set.block_capacity == 0);
    assert(set.block_count == 0);

    // An empty bitset still answers queries without allocating
    assert(bx_bitset_get(&set, 100) == false);
    assert(bx_bitset_popcount(&set) == 0);
    bx_bitset_unset(&set, 100);

    // First write allocates lazily
    bx_bitset_set_safe(&set, 63);
    assert(set.bits != NULL);
    assert(bx_bitset_get(&set, 63) == true);
    assert(bx_bitset_popcount(&set) == 1);

    bx_bitset_drop(&set);

    // init_capacity rounds bits up to whole blocks
    bx_bitset other;
    bx_bitset_init_capacity(&other, 65);
    assert(other.block_capacity == 2);
    assert(other.block_count == 0);
    bx_bitset_drop(&other);
}

void test_bitset_basic_ops(void)
{
    printf("Running: test_bitset_basic_ops\n");
    bx_bitset set;
    bx_bitset_init_capacity(&set, 64);
    bx_bitset_set_count_and_clear(&set, 64);

    assert(bx_bitset_get(&set, 10) == false);

    bx_bitset_set_fast(&set, 10);
    assert(bx_bitset_get(&set, 10) == true);

    bx_bitset_unset(&set, 10);
    assert(bx_bitset_get(&set, 10) == false);

    bx_bitset_drop(&set);
}

void test_bitset_set_safe_logic(void)
{
    printf("Running: test_bitset_set_safe_logic\n");
    bx_bitset set;
    bx_bitset_init_capacity(&set, 8); // Capacity 1 block, count 0

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

void test_bitset_set_count_and_clear(void)
{
    printf("Running: test_bitset_set_count_and_clear\n");
    bx_bitset set;
    bx_bitset_init_capacity(&set, 64);

    bx_bitset_set_count_and_clear(&set, 256); // 4 blocks
    bx_bitset_set_fast(&set, 10);
    bx_bitset_set_fast(&set, 200);
    assert(bx_bitset_popcount(&set) == 2);

    // Re-issuing the same count must still zero every bit
    bx_bitset_set_count_and_clear(&set, 256);
    assert(bx_bitset_popcount(&set) == 0);
    assert(bx_bitset_get(&set, 10) == false);

    bx_bitset_drop(&set);
}

void test_bitset_union_logic(void)
{
    printf("Running: test_bitset_union_logic\n");
    bx_bitset a, b;
    bx_bitset_init_capacity(&a, 64);
    bx_bitset_init_capacity(&b, 64);
    bx_bitset_set_count_and_clear(&a, 64);
    bx_bitset_set_count_and_clear(&b, 64);

    bx_bitset_set_fast(&a, 1);
    bx_bitset_set_fast(&b, 2);

    bx_bitset_union(&a, &b);

    assert(bx_bitset_get(&a, 1) == true);
    assert(bx_bitset_get(&a, 2) == true);
    assert(bx_bitset_popcount(&a) == 2);

    bx_bitset_drop(&a);
    bx_bitset_drop(&b);
}

void test_bitset_fencepost_errors(void)
{
    printf("Running: test_bitset_fencepost_errors\n");
    bx_bitset set;
    bx_bitset_init_capacity(&set, 128);
    bx_bitset_set_count_and_clear(&set, 128);

    bx_bitset_set_fast(&set, 63);
    bx_bitset_set_fast(&set, 64);

    assert(bx_bitset_get(&set, 63) == true);
    assert(bx_bitset_get(&set, 64) == true);

    // Ensure bit 63 didn't spill into block 1, and bit 64 didn't spill into block 0
    assert((set.bits[0] & (1ULL << 63)) != 0);
    assert((set.bits[1] & (1ULL << 0)) != 0);

    bx_bitset_drop(&set);
}

void test_bitset_growth_preservation(void)
{
    printf("Running: test_bitset_growth_preservation\n");
    bx_bitset set;
    bx_bitset_init_capacity(&set, 64);
    bx_bitset_set_count_and_clear(&set, 64);

    bx_bitset_set_fast(&set, 10);
    bx_bitset_set_fast(&set, 60);

    bx_bitset_grow_blocks(&set, 10); // Growth to 10 blocks (640 bits)

    // Verify old data is still there after memcpy/realloc
    assert(bx_bitset_get(&set, 10) == true);
    assert(bx_bitset_get(&set, 60) == true);
    assert(bx_bitset_popcount(&set) == 2);

    bx_bitset_drop(&set);
}

void test_bitset_out_of_bounds_safety(void)
{
    printf("Running: test_bitset_out_of_bounds_safety\n");
    bx_bitset set;
    bx_bitset_init_capacity(&set, 64);
    bx_bitset_set_count_and_clear(&set, 64);

    // get and unset absorb out-of-range indices rather than growing or faulting
    assert(bx_bitset_get(&set, 5000) == false);

    bx_bitset_unset(&set, 5000);

    bx_bitset_drop(&set);
}

void test_bitset_stress_random_access(void)
{
    printf("Running: test_bitset_stress_random_access\n");
    bx_bitset set;
    bx_bitset_init_capacity(&set, 1024);
    bx_bitset_set_count_and_clear(&set, 1024);

    for (uint32_t i = 0; i < 1024; i += 3)
    {
        bx_bitset_set_fast(&set, i);
    }

    for (uint32_t i = 0; i < 1024; i++)
    {
        bool expected = (i % 3 == 0);
        if (bx_bitset_get(&set, i) != expected)
        {
            fprintf(stderr, "Fail at index %u\n", i);
            assert(false);
        }
    }

    bx_bitset_drop(&set);
}

void test_bitset_reinit_cycle(void)
{
    printf("Running: test_bitset_reinit_cycle\n");
    bx_bitset set;

    bx_bitset_init_capacity(&set, 64);
    bx_bitset_set_safe(&set, 10);
    bx_bitset_drop(&set);

    // Re-init straight after drop must not see any of the old state
    bx_bitset_init_capacity(&set, 128);
    bx_bitset_set_count_and_clear(&set, 128);
    assert(bx_bitset_get(&set, 10) == false);
    bx_bitset_set_fast(&set, 10);
    assert(bx_bitset_get(&set, 10) == true);

    bx_bitset_drop(&set);
}

void test_bitset_as_hmap_key(void)
{
    printf("Running: test_bitset_as_hmap_key\n");

    bx_hmap_bset_i32 map;
    bx_hmap_bset_i32_init(&map);

    // Create three bitsets with different patterns
    bx_bitset s1, s2, s3;
    bx_bitset_init_capacity(&s1, 128);
    bx_bitset_set_count_and_clear(&s1, 128);
    bx_bitset_init_capacity(&s2, 128);
    bx_bitset_set_count_and_clear(&s2, 128);
    bx_bitset_init_capacity(&s3, 128);
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
    assert(map.base.size == 2);

    int32_t* v1 = bx_hmap_bset_i32_get(&map, s1);
    assert(v1 != NULL && *v1 == 1001);

    // This proves the content-based hashing and equality work.
    int32_t* v3 = bx_hmap_bset_i32_get(&map, s3);
    assert(v3 != NULL && *v3 == 1001);

    bx_hmap_bset_i32_insert(&map, s3, 9999);
    assert(map.base.size == 2); // Size should not increase
    assert(*bx_hmap_bset_i32_get(&map, s1) == 9999);

    bx_hmap_bset_i32_drop(&map);
    bx_bitset_drop(&s1);
    bx_bitset_drop(&s2);
    bx_bitset_drop(&s3);
}

void run_bitset_tests(void)
{
    printf("\n--- Starting bitset tests ---\n");
    test_bitset_popcount_utility();
    test_bitset_init_defaults();
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
