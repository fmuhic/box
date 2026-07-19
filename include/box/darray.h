#pragma once

#include <assert.h>
#include <stdint.h>

#include "box/darray_core.h"

#define BX_DARRAY_DECLARE(T, NAME)                                                           \
    typedef struct bx_darray_##NAME                                                          \
    {                                                                                        \
        bx_darray base;                                                                      \
    } bx_darray_##NAME;                                                                      \
                                                                                             \
    static inline void bx_darray_##NAME##_init(bx_darray_##NAME* arr)                        \
    {                                                                                        \
        bx_darray_init(&arr->base, sizeof(T));                                               \
    }                                                                                        \
    static inline void bx_darray_##NAME##_init_capacity(bx_darray_##NAME* arr,               \
                                                        uint32_t capacity)                   \
    {                                                                                        \
        bx_darray_init_capacity(&arr->base, sizeof(T), capacity);                            \
    }                                                                                        \
    static inline void bx_darray_##NAME##_drop(bx_darray_##NAME* arr)                        \
    {                                                                                        \
        bx_darray_drop(&arr->base);                                                          \
    }                                                                                        \
    static inline void bx_darray_##NAME##_reserve(bx_darray_##NAME* arr, uint32_t capacity)  \
    {                                                                                        \
        bx_darray_reserve(&arr->base, capacity);                                             \
    }                                                                                        \
    static inline void bx_darray_##NAME##_resize(bx_darray_##NAME* arr, uint32_t size)       \
    {                                                                                        \
        bx_darray_resize(&arr->base, size);                                                  \
    }                                                                                        \
    static inline void bx_darray_##NAME##_remove_swap(bx_darray_##NAME* arr, uint32_t index) \
    {                                                                                        \
        bx_darray_remove_swap(&arr->base, index);                                            \
    }                                                                                        \
    static inline void bx_darray_##NAME##_clear(bx_darray_##NAME* arr)                       \
    {                                                                                        \
        bx_darray_clear(&arr->base);                                                         \
    }                                                                                        \
    static inline uint32_t bx_darray_##NAME##_size(const bx_darray_##NAME* arr)              \
    {                                                                                        \
        return arr->base.size;                                                               \
    }                                                                                        \
    static inline uint32_t bx_darray_##NAME##_capacity(const bx_darray_##NAME* arr)          \
    {                                                                                        \
        return arr->base.capacity;                                                           \
    }                                                                                        \
                                                                                             \
    static inline T* bx_darray_##NAME##_get(bx_darray_##NAME* arr, uint32_t index)           \
    {                                                                                        \
        assert(index < arr->base.size && "bx_darray: index out of bounds");                  \
        return &((T*)arr->base.data)[index];                                                 \
    }                                                                                        \
    static inline T* bx_darray_##NAME##_emplace(bx_darray_##NAME* arr)                       \
    {                                                                                        \
        if (arr->base.size >= arr->base.capacity)                                            \
            bx_darray_grow(&arr->base);                                                      \
        return &((T*)arr->base.data)[arr->base.size++];                                      \
    }                                                                                        \
    static inline void bx_darray_##NAME##_push(bx_darray_##NAME* arr, T value)               \
    {                                                                                        \
        if (arr->base.size >= arr->base.capacity)                                            \
            bx_darray_grow(&arr->base);                                                      \
        ((T*)arr->base.data)[arr->base.size++] = value;                                      \
    }                                                                                        \
    static inline T bx_darray_##NAME##_pop(bx_darray_##NAME* arr)                            \
    {                                                                                        \
        assert(arr->base.size > 0 && "bx_darray: cannot pop from empty darray");             \
        return ((T*)arr->base.data)[--arr->base.size];                                       \
    }
