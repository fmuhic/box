#pragma once

#include <stddef.h>

typedef struct bx_darray
{
    void* data;
    size_t size;
    size_t capacity;
    size_t elem_size;
} bx_darray;

void bx_darray_init(bx_darray* arr, size_t elem_size);
void bx_darray_drop(bx_darray* arr);
void bx_darray_reserve(bx_darray* arr, size_t capacity);
void bx_darray_resize(bx_darray* arr, size_t size);
void bx_darray_grow(bx_darray* arr);
void bx_darray_remove_swap(bx_darray* arr, size_t index);
void bx_darray_clear(bx_darray* arr);
