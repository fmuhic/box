#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct bx_darray
{
    void* data;
    uint32_t size;
    uint32_t capacity;
    // size_t on purpose: it widens every `count * elem_size` to 64-bit before
    // the multiply, so a byte count can never overflow into a short alloc.
    size_t elem_size;
} bx_darray;

void bx_darray_init(bx_darray* arr, size_t elem_size);
void bx_darray_init_capacity(bx_darray* arr, size_t elem_size, uint32_t capacity);
void bx_darray_drop(bx_darray* arr);
void bx_darray_reserve(bx_darray* arr, uint32_t capacity);
void bx_darray_resize(bx_darray* arr, uint32_t size);
void bx_darray_grow(bx_darray* arr);
void bx_darray_remove_swap(bx_darray* arr, uint32_t index);
void bx_darray_clear(bx_darray* arr);
