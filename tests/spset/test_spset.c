#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include "box/core.h"
#include "box/spset.h"

typedef struct Vec2
{
    float x, y;
} Vec2;

BX_SPSET_DECLARE(uint32_t, u32)
BX_SPSET_SOURCE(uint32_t, u32)

BX_SPSET_DECLARE(Vec2, v2)
BX_SPSET_SOURCE(Vec2, v2)

void test_spset_basic_ops()
{
    printf("Running: test_spset_basic_ops\n");
    bx_spset_u32 set;
    bx_spset_u32_init(&set);

    // Insert (ID, Value)
    assert(bx_spset_u32_insert(&set, 10, 100) == true);
    assert(bx_spset_u32_insert(&set, 20, 200) == true);
    assert(set.size == 2);

    // Get and Pointer check
    uint32_t* val = bx_spset_u32_get(&set, 10);
    assert(val != NULL && *val == 100);

    assert(bx_spset_u32_contains(&set, 20));
    assert(bx_spset_u32_get(&set, 30) == NULL);

    // Erase
    bx_spset_u32_erase(&set, 10);
    assert(set.size == 1);
    assert(bx_spset_u32_get(&set, 10) == NULL);
    assert(bx_spset_u32_contains(&set, 20));

    bx_spset_u32_drop(&set);
}

void test_spset_get_and_modify()
{
    printf("Running: test_spset_get_and_modify\n");
    bx_spset_v2 set;
    bx_spset_v2_init(&set);

    bx_spset_v2_insert(&set, 5, (Vec2){ 1.0f, 2.0f });

    // Modify value directly through the returned pointer
    Vec2* ptr = bx_spset_v2_get(&set, 5);
    assert(ptr != NULL);
    ptr->x = 10.0f;

    // Verify change
    Vec2* ptr2 = bx_spset_v2_get(&set, 5);
    assert(ptr2->x == 10.0f);

    bx_spset_v2_drop(&set);
}

void test_spset_paging_logic()
{
    printf("Running: test_spset_paging_logic\n");
    bx_spset_u32 set;
    bx_spset_u32_init(&set);

    uint32_t id1 = 10;     // Page 0
    uint32_t id2 = 2000;   // Page 1 (if size is 1024)
    uint32_t id3 = 100000; // Page 97

    bx_spset_u32_insert(&set, id1, 1);
    bx_spset_u32_insert(&set, id2, 2);
    bx_spset_u32_insert(&set, id3, 3);

    assert(set.size == 3);
    assert(*bx_spset_u32_get(&set, id3) == 3);

    // Verify pointer table grew and specific pages exist
    assert(set.page_count > (id3 >> 10));
    size_t p2 = id2 >> 10;
    assert(set.sparse[p2] != NULL);
    assert(set.sparse[p2 + 1] == NULL);

    bx_spset_u32_drop(&set);
}

void test_spset_swap_and_pop_correction()
{
    printf("Running: test_spset_swap_and_pop_correction\n");
    bx_spset_u32 set;
    bx_spset_u32_init(&set);

    // Sequence: [10, 20, 30]
    bx_spset_u32_insert(&set, 10, 111);
    bx_spset_u32_insert(&set, 20, 222);
    bx_spset_u32_insert(&set, 30, 333);

    // Erase middle (20). 30 should move to index 1.
    bx_spset_u32_erase(&set, 20);

    assert(set.size == 2);
    // Verify ID 30 moved into index 1
    assert(set.ids[1] == 30);
    assert(set.dense[1] == 333);

    // Verify the "Correction": sparse table must now point 30 to index 1
    // If the correction failed, this get() would return NULL or wrong data
    uint32_t* ptr30 = bx_spset_u32_get(&set, 30);
    assert(ptr30 != NULL && *ptr30 == 333);

    bx_spset_u32_drop(&set);
}

void test_spset_duplicate_and_update()
{
    printf("Running: test_spset_duplicate_and_update\n");
    bx_spset_u32 set;
    bx_spset_u32_init(&set);

    // Initial insert
    assert(bx_spset_u32_insert(&set, 50, 100) == true);

    // Duplicate insert should return false but UPDATE the value
    assert(bx_spset_u32_insert(&set, 50, 999) == false);
    assert(set.size == 1);
    assert(*bx_spset_u32_get(&set, 50) == 999);

    bx_spset_u32_drop(&set);
}

void test_spset_erase_last_element()
{
    printf("Running: test_spset_erase_last_element\n");
    bx_spset_u32 set;
    bx_spset_u32_init(&set);

    bx_spset_u32_insert(&set, 10, 1);
    bx_spset_u32_insert(&set, 20, 2);

    // Erase the last element (20)
    // "swap" logic shouldn't break when index == last_idx
    bx_spset_u32_erase(&set, 20);

    assert(set.size == 1);
    assert(bx_spset_u32_contains(&set, 10));
    assert(!bx_spset_u32_contains(&set, 20));

    bx_spset_u32_drop(&set);
}

void test_spset_clear_and_reuse()
{
    printf("Running: test_spset_clear_and_reuse\n");
    bx_spset_u32 set;
    bx_spset_u32_init(&set);

    for (uint32_t i = 0; i < 100; i++)
    {
        bx_spset_u32_insert(&set, i, i * 10);
    }

    bx_spset_u32_clear(&set);

    assert(set.size == 0);
    assert(bx_spset_u32_get(&set, 50) == NULL);

    // Re-inserting
    bx_spset_u32_insert(&set, 50, 500);
    assert(set.size == 1);
    assert(*bx_spset_u32_get(&set, 50) == 500);

    bx_spset_u32_drop(&set);
}

void test_spset_dense_iteration()
{
    printf("Running: test_spset_dense_iteration\n");
    bx_spset_u32 set;
    bx_spset_u32_init(&set);

    uint32_t ids[] = { 10, 50, 100 };
    for (int i = 0; i < 3; ++i)
    {
        bx_spset_u32_insert(&set, ids[i], ids[i] * 2);
    }

    // Test that we can iterate linearly (cache-friendly)
    uint32_t sum_ids = 0;
    uint32_t sum_vals = 0;
    for (size_t i = 0; i < set.size; i++)
    {
        sum_ids += set.ids[i];
        sum_vals += set.dense[i];
    }
    assert(sum_ids == (10 + 50 + 100));
    assert(sum_vals == (20 + 100 + 200));

    bx_spset_u32_drop(&set);
}

// Custom Allocator Test
static int g_spset_allocs = 0;
static int g_spset_frees = 0;

static void* mock_alloc(size_t size)
{
    g_spset_allocs++;
    return bx_alloc(size);
}

static void mock_free(void* ptr)
{
    if (ptr)
    {
        g_spset_frees++;
        bx_free(ptr);
    }
}

BX_SPSET_DECLARE(uint32_t, custom)
BX_SPSET_SOURCE(uint32_t, custom, mock_alloc, mock_free)

void test_spset_custom_allocator()
{
    printf("Running: test_spset_custom_allocator\n");
    g_spset_allocs = 0;
    g_spset_frees = 0;

    bx_spset_custom set;
    bx_spset_custom_init(&set);
    bx_spset_custom_insert(&set, 1, 10);
    bx_spset_custom_insert(&set, 2000, 20);

    bx_spset_custom_drop(&set);
    assert(g_spset_allocs > 0);
    assert(g_spset_frees == g_spset_allocs);
}

void run_spset_tests()
{
    printf("\n--- Starting spset tests ---\n");
    test_spset_basic_ops();
    test_spset_get_and_modify();
    test_spset_paging_logic();
    test_spset_swap_and_pop_correction();
    test_spset_duplicate_and_update();
    test_spset_erase_last_element();
    test_spset_clear_and_reuse();
    test_spset_dense_iteration();
    test_spset_custom_allocator();
    printf("--- spset tests passed ---\n\n");
}
