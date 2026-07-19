#include "box/darray_core.h"

#include <string.h>
#include <assert.h>

#include "box/core.h"

#define BX_DARRAY_DEFAULT_CAPACITY 8

void bx_darray_init(bx_darray* arr, size_t elem_size)
{
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
    arr->elem_size = elem_size;
}

void bx_darray_drop(bx_darray* arr)
{
    if (arr->data)
    {
        bx_free(arr->data);
    }
    arr->data = NULL;
    arr->size = 0;
    arr->capacity = 0;
}

void bx_darray_reserve(bx_darray* arr, size_t capacity)
{
    if (capacity <= arr->capacity)
    {
        return;
    }

    void* new_data = bx_alloc(capacity * arr->elem_size);
    assert(new_data != NULL && "bx_darray: allocation failed");

    if (arr->data)
    {
        memcpy(new_data, arr->data, arr->size * arr->elem_size);
        bx_free(arr->data);
    }
    arr->data = new_data;
    arr->capacity = capacity;
}

void bx_darray_resize(bx_darray* arr, size_t size)
{
    bx_darray_reserve(arr, size);
    arr->size = size;
}

void bx_darray_grow(bx_darray* arr)
{
    if (arr->size < arr->capacity)
    {
        return;
    }
    size_t new_cap = arr->capacity == 0
                         ? BX_DARRAY_DEFAULT_CAPACITY
                         : arr->capacity * 2;
    bx_darray_reserve(arr, new_cap);
}

void bx_darray_remove_swap(bx_darray* arr, size_t index)
{
    assert(index < arr->size && "bx_darray: index out of bounds");
    arr->size--;
    if (index != arr->size)
    {
        memcpy((char*)arr->data + index * arr->elem_size,
               (char*)arr->data + arr->size * arr->elem_size,
               arr->elem_size);
    }
}

void bx_darray_clear(bx_darray* arr)
{
    arr->size = 0;
}
