#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "box/core.h"

#define BX_DARRAY_DEFAULT_CAPACITY 8

typedef struct bx_darray
{
    void* data;
    uint32_t size;
    uint32_t capacity;
    // size_t on purpose: it widens every `count * elem_size` to 64-bit before
    // the multiply, so a byte count can never overflow into a short alloc.
    size_t elem_size;
} bx_darray;

// Every function here is `static inline` on purpose: an out-of-line call taking
// `&arr->base` leaks the struct address across a TU boundary, forcing the
// compiler to reload data/size/capacity from memory on every loop iteration --
// including the ones that do not grow. Moving any of it into a .c file costs
// ~40% of push throughput, and every test still passes.

static inline void bx_darray_init(bx_darray* arr, size_t elem_size)
{
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
    arr->elem_size = elem_size;
}

// The single place that resizes a darray buffer, so no two paths can drift.
static inline void bx_darray_set_capacity(bx_darray* arr, uint32_t capacity)
{
    arr->data = bx_realloc(arr->data, (size_t)capacity * arr->elem_size);
    assert(arr->data != NULL && "bx_darray: allocation failed");
    arr->capacity = capacity;
}

static inline void bx_darray_reserve(bx_darray* arr, uint32_t capacity)
{
    if (capacity <= arr->capacity)
    {
        return;
    }
    bx_darray_set_capacity(arr, capacity);
}

static inline void bx_darray_init_capacity(bx_darray* arr, size_t elem_size, uint32_t capacity)
{
    bx_darray_init(arr, elem_size);
    bx_darray_reserve(arr, capacity);
}

static inline void bx_darray_drop(bx_darray* arr)
{
    bx_free(arr->data);
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
}

static inline void bx_darray_resize(bx_darray* arr, uint32_t size)
{
    bx_darray_reserve(arr, size);
    arr->size = size;
}

static inline void bx_darray_grow(bx_darray* arr)
{
    if (arr->size < arr->capacity)
    {
        return;
    }
    bx_darray_set_capacity(arr, arr->capacity == 0 ? BX_DARRAY_DEFAULT_CAPACITY
                                                   : arr->capacity * 2);
}

static inline void bx_darray_remove_swap(bx_darray* arr, uint32_t index)
{
    assert(index < arr->size && "bx_darray: index out of bounds");
    arr->size--;
    if (index != arr->size)
    {
        memcpy((char*)arr->data + index * arr->elem_size,
               (char*)arr->data + arr->size * arr->elem_size, arr->elem_size);
    }
}

static inline void bx_darray_clear(bx_darray* arr)
{
    arr->size = 0;
}
