#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "test_darray.h"
#include "box/darray.h"

typedef struct vec3
{
    float x, y, z;
} vec3;

typedef struct Entity
{
    const char* name;
    vec3 position;
} Entity;

BX_DARRAY_DECLARE(int32_t, i32)
BX_DARRAY_DECLARE(vec3, v3)
BX_DARRAY_DECLARE(Entity, ent)

void test_darray_basic_ops()
{
    printf("Running: test_darray_basic_ops\n");
    bx_darray_i32 arr;
    bx_darray_i32_init(&arr);

    bx_darray_i32_push(&arr, 10);
    bx_darray_i32_push(&arr, 20);
    bx_darray_i32_push(&arr, 30);

    assert(arr.base.size == 3);
    assert(*bx_darray_i32_get(&arr, 0) == 10);
    assert(*bx_darray_i32_get(&arr, 2) == 30);

    int32_t val = bx_darray_i32_pop(&arr);
    assert(val == 30);
    assert(arr.base.size == 2);

    bx_darray_i32_drop(&arr);
}

void test_darray_growth()
{
    printf("Running: test_darray_growth\n");
    bx_darray_i32 arr;
    bx_darray_i32_init(&arr);

    // Push 100 items to force multiple reallocs
    for (int i = 0; i < 100; i++)
    {
        bx_darray_i32_push(&arr, i);
    }

    assert(arr.base.size == 100);
    assert(arr.base.capacity >= 100);

    for (int i = 0; i < 100; i++)
    {
        assert(*bx_darray_i32_get(&arr, i) == i);
    }

    bx_darray_i32_drop(&arr);
}

void test_darray_remove_swap()
{
    printf("Running: test_darray_remove_swap\n");
    bx_darray_i32 arr;
    bx_darray_i32_init(&arr);

    bx_darray_i32_push(&arr, 1);
    bx_darray_i32_push(&arr, 2);
    bx_darray_i32_push(&arr, 3);
    bx_darray_i32_push(&arr, 4);

    // Remove index 1 (value 2). Index 3 (value 4) should move to index 1.
    bx_darray_i32_remove_swap(&arr, 1);

    assert(arr.base.size == 3);
    assert(*bx_darray_i32_get(&arr, 0) == 1);
    assert(*bx_darray_i32_get(&arr, 1) == 4); // The swap happened
    assert(*bx_darray_i32_get(&arr, 2) == 3);

    bx_darray_i32_drop(&arr);
}

void test_darray_complex_structures()
{
    printf("Running: test_darray_complex_structures\n");
    bx_darray_ent entities;
    bx_darray_ent_init(&entities);

    Entity e1 = { "Player", { 1, 2, 3 } };
    Entity e2 = { "Enemy", { 10, 20, 30 } };

    bx_darray_ent_push(&entities, e1);
    bx_darray_ent_push(&entities, e2);

    Entity* p = bx_darray_ent_get(&entities, 0);
    assert(strcmp(p->name, "Player") == 0);
    assert(p->position.z == 3.0f);

    bx_darray_ent_drop(&entities);
}
void test_darray_resize()
{
    printf("Running: test_darray_resize\n");
    bx_darray_i32 arr;
    bx_darray_i32_init(&arr);

    bx_darray_i32_resize(&arr, 5);
    assert(arr.base.size == 5);
    assert(arr.base.capacity >= 5);

    *bx_darray_i32_get(&arr, 4) = 42;
    assert(*bx_darray_i32_get(&arr, 4) == 42);

    bx_darray_i32_resize(&arr, 2);
    assert(arr.base.size == 2);

    bx_darray_i32_drop(&arr);
}

void test_darray_emplace()
{
    printf("Running: test_darray_emplace\n");
    bx_darray_ent entities;
    bx_darray_ent_init(&entities);

    Entity* e = bx_darray_ent_emplace(&entities);
    e->name = "Boss";
    e->position = (vec3){ 5, 6, 7 };

    assert(entities.base.size == 1);
    assert(strcmp(bx_darray_ent_get(&entities, 0)->name, "Boss") == 0);
    assert(bx_darray_ent_get(&entities, 0)->position.y == 6.0f);

    bx_darray_ent_drop(&entities);
}

void run_darray_tests()
{
    printf("\n--- Starting darray tests ---\n");
    test_darray_basic_ops();
    test_darray_growth();
    test_darray_remove_swap();
    test_darray_complex_structures();
    test_darray_resize();
    test_darray_emplace();
    printf("--- darray tests passed ---\n\n");
}
