#pragma once

#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "box/core.h"

#define BX_DARRAY_DEFAULT_CAPACITY 8

#define BX_DARRAY_DECLARE(T, NAME)                                                                                     \
    typedef struct bx_darray_##NAME                                                                                    \
    {                                                                                                                  \
        T* data;                                                                                                       \
        size_t size;                                                                                                   \
        size_t capacity;                                                                                               \
    } bx_darray_##NAME;                                                                                                \
                                                                                                                       \
    void bx_darray_##NAME##_init(bx_darray_##NAME* arr);                                                               \
    void bx_darray_##NAME##_drop(bx_darray_##NAME* arr);                                                               \
    void bx_darray_##NAME##_reserve(bx_darray_##NAME* arr, size_t capacity);                                           \
    void bx_darray_##NAME##_push(bx_darray_##NAME* arr, T value);                                                      \
    T bx_darray_##NAME##_pop(bx_darray_##NAME* arr);                                                                   \
    void bx_darray_##NAME##_remove_swap(bx_darray_##NAME* arr, size_t index);                                          \
    void bx_darray_##NAME##_clear(bx_darray_##NAME* arr);                                                              \
                                                                                                                       \
    static inline T* bx_darray_##NAME##_get(bx_darray_##NAME* arr, size_t index)                                       \
    {                                                                                                                  \
        assert(index < arr->size && "bx_darray: index out of bounds");                                                 \
        return &arr->data[index];                                                                                      \
    }

#define BX_DARRAY_SOURCE_INTERNAL(T, NAME, ALLOC_FN, FREE_FN)                                                          \
    void bx_darray_##NAME##_init(bx_darray_##NAME* arr) {                                                              \
        memset(arr, 0, sizeof(bx_darray_##NAME));                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    void bx_darray_##NAME##_drop(bx_darray_##NAME* arr) {                                                              \
        if (arr->data) FREE_FN(arr->data);                                                                             \
        memset(arr, 0, sizeof(bx_darray_##NAME));                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    void bx_darray_##NAME##_reserve(bx_darray_##NAME* arr, size_t capacity)                                            \
    {                                                                                                                  \
        if (capacity <= arr->capacity) return;                                                                         \
        size_t old_size = arr->capacity * sizeof(T);                                                                   \
        size_t new_size = capacity * sizeof(T);                                                                        \
                                                                                                                       \
        T* new_data = (T*)ALLOC_FN(new_size);                                                                          \
        assert(new_data != NULL && "bx_darray: allocation failed");                                                    \
                                                                                                                       \
        if (arr->data) {                                                                                               \
            memcpy(new_data, arr->data, old_size);                                                                     \
            FREE_FN(arr->data);                                                                                        \
        }                                                                                                              \
        arr->data = new_data;                                                                                          \
        arr->capacity = capacity;                                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    void bx_darray_##NAME##_push(bx_darray_##NAME* arr, T value)                                                       \
    {                                                                                                                  \
        if (arr->size >= arr->capacity) {                                                                              \
            size_t new_cap = arr->capacity == 0                                                                        \
                ? BX_DARRAY_DEFAULT_CAPACITY                                                                           \
                : arr->capacity * 2;                                                                                   \
            bx_darray_##NAME##_reserve(arr, new_cap);                                                                  \
        }                                                                                                              \
        arr->data[arr->size++] = value;                                                                                \
    }                                                                                                                  \
                                                                                                                       \
    T bx_darray_##NAME##_pop(bx_darray_##NAME* arr) {                                                                  \
        assert(arr->size > 0 && "bx_darray: cannot pop from empty darray");                                            \
        return arr->data[--arr->size];                                                                                 \
    }                                                                                                                  \
                                                                                                                       \
    void bx_darray_##NAME##_remove_swap(bx_darray_##NAME* arr, size_t index)                                           \
    {                                                                                                                  \
        assert(index < arr->size && "bx_darray: index out of bounds");                                                 \
        arr->size--;                                                                                                   \
        if (index != arr->size) {                                                                                      \
            memcpy(&arr->data[index], &arr->data[arr->size], sizeof(T));                                               \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    void bx_darray_##NAME##_clear(bx_darray_##NAME* arr) {                                                             \
        arr->size = 0;                                                                                                 \
    }

// This allows us to call BX_DARRAY_SOURCE with 2 or 4 arguments
// EX: BX_DARRAY_SOURCE(float, f32) or BX_DARRAY_SOURCE(float, f32, custom_alloc, custom_free) 
#define BX_DARRAY_SOURCE_2(T, NAME)                                                                                    \
    BX_DARRAY_SOURCE_INTERNAL(T, NAME, bx_alloc, bx_free)

#define BX_DARRAY_SOURCE_4(T, NAME, ALLOC_FN, FREE_FN)                                                                 \
    BX_DARRAY_SOURCE_INTERNAL(T, NAME, ALLOC_FN, FREE_FN)

#define BX_DARRAY_SOURCE_GET(_1, _2, _3, _4, NAME, ...) NAME
#define BX_DARRAY_SOURCE(...)                                                                                          \
    BX_DARRAY_SOURCE_GET(__VA_ARGS__, BX_DARRAY_SOURCE_4, _unused, BX_DARRAY_SOURCE_2)(__VA_ARGS__)
