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
BX_SPSET_DECLARE(Vec2, v2)

void test_spset_basic_ops(void)
{
    printf("Running: test_spset_basic_ops\n");
    bx_spset_u32 set;
    bx_spset_u32_init(&set);

    assert(bx_spset_u32_insert(&set, 10, 100) == true);
    assert(bx_spset_u32_insert(&set, 20, 200) == true);
    assert(bx_spset_u32_size(&set) == 2);

    uint32_t* val = bx_spset_u32_get(&set, 10);
    assert(val != NULL && *val == 100);

    assert(bx_spset_u32_contains(&set, 20));
    assert(bx_spset_u32_get(&set, 30) == NULL);

    bx_spset_u32_erase(&set, 10);
    assert(bx_spset_u32_size(&set) == 1);
    assert(bx_spset_u32_get(&set, 10) == NULL);
    assert(bx_spset_u32_contains(&set, 20));

    bx_spset_u32_drop(&set);
}

void test_spset_get_and_modify(void)
{
    printf("Running: test_spset_get_and_modify\n");
    bx_spset_v2 set;
    bx_spset_v2_init(&set);

    bx_spset_v2_insert(&set, 5, (Vec2){ 1.0f, 2.0f });

    Vec2* ptr = bx_spset_v2_get(&set, 5);
    assert(ptr != NULL);
    ptr->x = 10.0f;

    Vec2* ptr2 = bx_spset_v2_get(&set, 5);
    assert(ptr2->x == 10.0f);

    bx_spset_v2_drop(&set);
}

void test_spset_paging_logic(void)
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

    assert(bx_spset_u32_size(&set) == 3);
    assert(*bx_spset_u32_get(&set, id3) == 3);

    assert(set.base.page_count > (id3 >> 10));
    uint32_t p2 = id2 >> 10;
    assert(set.base.sparse[p2] != NULL);
    assert(set.base.sparse[p2 + 1] == NULL);

    bx_spset_u32_drop(&set);
}

void test_spset_swap_and_pop_correction(void)
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

    assert(bx_spset_u32_size(&set) == 2);
    assert(bx_spset_u32_ids(&set)[1] == 30);
    assert(bx_spset_u32_data(&set)[1] == 333);

    // Verify the "Correction": sparse table must now point 30 to index 1
    // If the correction failed, this get() would return NULL or wrong data
    uint32_t* ptr30 = bx_spset_u32_get(&set, 30);
    assert(ptr30 != NULL && *ptr30 == 333);

    bx_spset_u32_drop(&set);
}

void test_spset_duplicate_and_update(void)
{
    printf("Running: test_spset_duplicate_and_update\n");
    bx_spset_u32 set;
    bx_spset_u32_init(&set);

    // Initial insert
    assert(bx_spset_u32_insert(&set, 50, 100) == true);

    // Duplicate insert should return false but UPDATE the value
    assert(bx_spset_u32_insert(&set, 50, 999) == false);
    assert(bx_spset_u32_size(&set) == 1);
    assert(*bx_spset_u32_get(&set, 50) == 999);

    bx_spset_u32_drop(&set);
}

void test_spset_erase_last_element(void)
{
    printf("Running: test_spset_erase_last_element\n");
    bx_spset_u32 set;
    bx_spset_u32_init(&set);

    bx_spset_u32_insert(&set, 10, 1);
    bx_spset_u32_insert(&set, 20, 2);

    // Erase the last element (20)
    // "swap" logic shouldn't break when index == last_idx
    bx_spset_u32_erase(&set, 20);

    assert(bx_spset_u32_size(&set) == 1);
    assert(bx_spset_u32_contains(&set, 10));
    assert(!bx_spset_u32_contains(&set, 20));

    bx_spset_u32_drop(&set);
}

void test_spset_clear_and_reuse(void)
{
    printf("Running: test_spset_clear_and_reuse\n");
    bx_spset_u32 set;
    bx_spset_u32_init(&set);

    for (uint32_t i = 0; i < 100; i++)
    {
        bx_spset_u32_insert(&set, i, i * 10);
    }

    bx_spset_u32_clear(&set);

    assert(bx_spset_u32_size(&set) == 0);
    assert(bx_spset_u32_get(&set, 50) == NULL);

    bx_spset_u32_insert(&set, 50, 500);
    assert(bx_spset_u32_size(&set) == 1);
    assert(*bx_spset_u32_get(&set, 50) == 500);

    bx_spset_u32_drop(&set);
}

void test_spset_dense_iteration(void)
{
    printf("Running: test_spset_dense_iteration\n");
    bx_spset_u32 set;
    bx_spset_u32_init(&set);

    uint32_t ids[] = { 10, 50, 100 };
    for (int i = 0; i < 3; ++i)
    {
        bx_spset_u32_insert(&set, ids[i], ids[i] * 2);
    }

    uint32_t sum_ids = 0;
    uint32_t sum_vals = 0;
    for (uint32_t i = 0; i < bx_spset_u32_size(&set); i++)
    {
        sum_ids += bx_spset_u32_ids(&set)[i];
        sum_vals += bx_spset_u32_data(&set)[i];
    }
    assert(sum_ids == (10 + 50 + 100));
    assert(sum_vals == (20 + 100 + 200));

    bx_spset_u32_drop(&set);
}

void test_spset_reserve(void)
{
    printf("Running: test_spset_reserve\n");
    bx_spset_u32 set;
    bx_spset_u32_init(&set);

    // Capacity is honoured exactly, never rounded up
    bx_spset_u32_reserve(&set, 64);
    assert(set.base.dense.capacity == 64);
    assert(set.base.ids.capacity == 64);

    // Dense and ids arrays must stay in lockstep as elements are added
    for (uint32_t i = 0; i < 64; i++)
    {
        bx_spset_u32_insert(&set, i * 100, i);
    }
    assert(bx_spset_u32_size(&set) == 64);
    assert(set.base.dense.size == set.base.ids.size);
    assert(set.base.dense.capacity == set.base.ids.capacity);

    for (uint32_t i = 0; i < 64; i++)
    {
        assert(*bx_spset_u32_get(&set, i * 100) == i);
    }

    bx_spset_u32_drop(&set);
}

void test_spset_init_capacity_is_allocation_free(void)
{
    printf("Running: test_spset_init_capacity_is_allocation_free\n");
    bx_spset_u32 set;

    // 2555 elements spans ceil(2555 / 1024) = 3 pages, covering IDs 0..3071
    bx_spset_u32_init_capacity(&set, 2555);
    assert(set.base.dense.capacity == 2555); // exact, not rounded
    assert(set.base.page_count == 3);
    assert(set.base.sparse[0] != NULL);
    assert(set.base.sparse[1] != NULL);
    assert(set.base.sparse[2] != NULL);

    // Inserting across all three pages must not allocate anything further
    bx_spset_u32_insert(&set, 5, 50);     // page 0
    bx_spset_u32_insert(&set, 1500, 150); // page 1
    bx_spset_u32_insert(&set, 2500, 250); // page 2

    assert(set.base.dense.capacity == 2555);
    assert(set.base.page_count == 3);

    assert(*bx_spset_u32_get(&set, 5) == 50);
    assert(*bx_spset_u32_get(&set, 1500) == 150);
    assert(*bx_spset_u32_get(&set, 2500) == 250);

    bx_spset_u32_drop(&set);
}

void test_spset_reserve_ids_stays_lazy(void)
{
    printf("Running: test_spset_reserve_ids_stays_lazy\n");
    bx_spset_u32 set;
    bx_spset_u32_init(&set);

    // reserve_ids sizes the directory only; pages are still materialized on demand
    bx_spset_u32_reserve_ids(&set, 100000);
    assert(set.base.page_count == 98);
    assert(set.base.sparse[50] == NULL);

    bx_spset_u32_insert(&set, 51200, 7); // page 50
    assert(set.base.sparse[50] != NULL);
    assert(set.base.sparse[51] == NULL);

    bx_spset_u32_drop(&set);
}

void test_spset_reserve_is_a_hint_not_a_bound(void)
{
    printf("Running: test_spset_reserve_is_a_hint_not_a_bound\n");
    bx_spset_u32 set;
    bx_spset_u32_init_capacity(&set, 64);
    assert(set.base.page_count == 1);

    // IDs outside [0, capacity) still work, they just fall back to lazy growth
    bx_spset_u32_insert(&set, 100000, 999);
    assert(set.base.page_count == 98);
    assert(*bx_spset_u32_get(&set, 100000) == 999);
    assert(bx_spset_u32_size(&set) == 1);

    bx_spset_u32_drop(&set);
}

void run_spset_tests(void)
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
    test_spset_reserve();
    test_spset_init_capacity_is_allocation_free();
    test_spset_reserve_ids_stays_lazy();
    test_spset_reserve_is_a_hint_not_a_bound();
    printf("--- spset tests passed ---\n\n");
}
