#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "test_darray.h"
#include "box/darray.h"

typedef struct vec3 {
    float x, y, z;
} vec3;

typedef struct Entity {
    const char* name;
    vec3 position;
} Entity;

BX_DARRAY_DECLARE(int32_t, i32)
BX_DARRAY_SOURCE(int32_t, i32)

BX_DARRAY_DECLARE(vec3, v3)
BX_DARRAY_SOURCE(vec3, v3)

BX_DARRAY_DECLARE(Entity, ent)
BX_DARRAY_SOURCE(Entity, ent)

void test_darray_basic_ops() {
    printf("Running: test_darray_basic_ops\n");
    bx_darray_i32 arr;
    bx_darray_i32_init(&arr);

    bx_darray_i32_push(&arr, 10);
    bx_darray_i32_push(&arr, 20);
    bx_darray_i32_push(&arr, 30);

    assert(arr.size == 3);
    assert(*bx_darray_i32_get(&arr, 0) == 10);
    assert(*bx_darray_i32_get(&arr, 2) == 30);

    int32_t val = bx_darray_i32_pop(&arr);
    assert(val == 30);
    assert(arr.size == 2);

    bx_darray_i32_drop(&arr);
}

void test_darray_growth() {
    printf("Running: test_darray_growth\n");
    bx_darray_i32 arr;
    bx_darray_i32_init(&arr);

    // Push 100 items to force multiple reallocs
    for (int i = 0; i < 100; i++) {
        bx_darray_i32_push(&arr, i);
    }

    assert(arr.size == 100);
    assert(arr.capacity >= 100);

    for (int i = 0; i < 100; i++) {
        assert(*bx_darray_i32_get(&arr, i) == i);
    }

    bx_darray_i32_drop(&arr);
}

void test_darray_remove_swap() {
    printf("Running: test_darray_remove_swap\n");
    bx_darray_i32 arr;
    bx_darray_i32_init(&arr);

    bx_darray_i32_push(&arr, 1);
    bx_darray_i32_push(&arr, 2);
    bx_darray_i32_push(&arr, 3);
    bx_darray_i32_push(&arr, 4);

    // Remove index 1 (value 2). Index 3 (value 4) should move to index 1.
    bx_darray_i32_remove_swap(&arr, 1);

    assert(arr.size == 3);
    assert(*bx_darray_i32_get(&arr, 0) == 1);
    assert(*bx_darray_i32_get(&arr, 1) == 4); // The swap happened
    assert(*bx_darray_i32_get(&arr, 2) == 3);

    bx_darray_i32_drop(&arr);
}

void test_darray_complex_structures() {
    printf("Running: test_darray_complex_structures\n");
    bx_darray_ent entities;
    bx_darray_ent_init(&entities);

    Entity e1 = {"Player", {1, 2, 3}};
    Entity e2 = {"Enemy", {10, 20, 30}};

    bx_darray_ent_push(&entities, e1);
    bx_darray_ent_push(&entities, e2);

    Entity* p = bx_darray_ent_get(&entities, 0);
    assert(strcmp(p->name, "Player") == 0);
    assert(p->position.z == 3.0f);

    bx_darray_ent_drop(&entities);
}


static int g_custom_alloc_calls = 0;
static int g_custom_free_calls = 0;

void* my_test_alloc(size_t size) {
    g_custom_alloc_calls++;
    return malloc(size);
}

void my_test_free(void* ptr) {
    if (ptr) g_custom_free_calls++;
    free(ptr);
}

BX_DARRAY_DECLARE(int32_t, i32_custom)
BX_DARRAY_SOURCE(int32_t, i32_custom, my_test_alloc, my_test_free)

void test_darray_custom_allocator() {
    printf("Running: test_darray_custom_allocator\n");
    
    g_custom_alloc_calls = 0;
    g_custom_free_calls = 0;

    bx_darray_i32_custom arr;
    bx_darray_i32_custom_init(&arr);

    bx_darray_i32_custom_push(&arr, 1);
    
    assert(g_custom_alloc_calls > 0);
    
    bx_darray_i32_custom_drop(&arr);
    assert(g_custom_free_calls > 0);
}

void run_darray_tests() {
    printf("\n--- Starting darray tests ---\n");
    test_darray_basic_ops();
    test_darray_growth();
    test_darray_remove_swap();
    test_darray_complex_structures();
    test_darray_custom_allocator();
    printf("--- darray tests passed ---\n\n");
}
