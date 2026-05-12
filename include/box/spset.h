#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "box/core.h"

#define BX_SPSET_PAGE_SHIFT 10
#define BX_SPSET_PAGE_SIZE (1 << BX_SPSET_PAGE_SHIFT)
#define BX_SPSET_PAGE_MASK (BX_SPSET_PAGE_SIZE - 1)
#define BX_SPSET_INVALID_INDEX ((size_t)-1)

#define BX_SPSET_DECLARE(T, NAME)                                                                                      \
    typedef struct bx_spset_##NAME                                                                                     \
    {                                                                                                                  \
        T* dense;           /* Packed data for cache-friendly iteration */                                             \
        uint32_t* ids;      /* Maps dense_idx back to the original ID */                                               \
        size_t** sparse;    /* Paginated lookup table (ID -> dense_idx) */                                             \
        size_t size;        /* Current number of active elements in dense array */                                     \
        size_t capacity;    /* Total allocated slots in dense array */                                                 \
        size_t page_count;  /* Number of allocated pages in sparse table */                                            \
    } bx_spset_##NAME;                                                                                                 \
                                                                                                                       \
    void bx_spset_##NAME##_init(bx_spset_##NAME* set);                                                                 \
    void bx_spset_##NAME##_drop(bx_spset_##NAME* set);                                                                 \
    void bx_spset_##NAME##_reserve_ids(bx_spset_##NAME* set, uint32_t max_id);                                         \
    bool bx_spset_##NAME##_insert(bx_spset_##NAME* set, uint32_t id, T value);                                         \
    void bx_spset_##NAME##_erase(bx_spset_##NAME* set, uint32_t id);                                                   \
    T*   bx_spset_##NAME##_get(const bx_spset_##NAME* set, uint32_t id);                                               \
    void bx_spset_##NAME##_clear(bx_spset_##NAME* set);                                                                \
    bool bx_spset_##NAME##_contains(const bx_spset_##NAME* set, uint32_t id);

#define BX_SPSET_SOURCE_INTERNAL(T, NAME, ALLOC_FN, FREE_FN)                                                           \
    void bx_spset_##NAME##_init(bx_spset_##NAME* set)                                                                  \
    {                                                                                                                  \
        memset(set, 0, sizeof(bx_spset_##NAME));                                                                       \
    }                                                                                                                  \
                                                                                                                       \
    void bx_spset_##NAME##_drop(bx_spset_##NAME* set)                                                                  \
    {                                                                                                                  \
        if (set->sparse)                                                                                               \
        {                                                                                                              \
            for (size_t i = 0; i < set->page_count; ++i)                                                               \
            {                                                                                                          \
                if (set->sparse[i]) FREE_FN(set->sparse[i]);                                                           \
            }                                                                                                          \
            FREE_FN(set->sparse);                                                                                      \
        }                                                                                                              \
                                                                                                                       \
        if (set->dense) FREE_FN(set->dense);                                                                           \
        if (set->ids) FREE_FN(set->ids);                                                                               \
                                                                                                                       \
        memset(set, 0, sizeof(bx_spset_##NAME));                                                                       \
    }                                                                                                                  \
                                                                                                                       \
    void bx_spset_##NAME##_reserve_ids(bx_spset_##NAME* set, uint32_t max_id)                                          \
    {                                                                                                                  \
        size_t required_pages = ((size_t)max_id >> BX_SPSET_PAGE_SHIFT) + 1;                                           \
        if (required_pages > set->page_count)                                                                          \
        {                                                                                                              \
            size_t** new_sparse = (size_t**)ALLOC_FN(required_pages * sizeof(size_t*));                                \
            memset(new_sparse, 0, required_pages * sizeof(size_t*));                                                   \
                                                                                                                       \
            if (set->sparse)                                                                                           \
            {                                                                                                          \
                memcpy(new_sparse, set->sparse, set->page_count * sizeof(size_t*));                                    \
                FREE_FN(set->sparse);                                                                                  \
            }                                                                                                          \
                                                                                                                       \
            set->sparse = new_sparse;                                                                                  \
            set->page_count = required_pages;                                                                          \
        }                                                                                                              \
    }                                                                                                                  \
                                                                                                                       \
    static size_t bx_spset_##NAME##_internal_idx(const bx_spset_##NAME* set, uint32_t id)                              \
    {                                                                                                                  \
        size_t p = (size_t)id >> BX_SPSET_PAGE_SHIFT;                                                                  \
        if (p >= set->page_count || !set->sparse[p])                                                                   \
        {                                                                                                              \
            return BX_SPSET_INVALID_INDEX;                                                                             \
        }                                                                                                              \
                                                                                                                       \
        size_t idx = set->sparse[p][id & BX_SPSET_PAGE_MASK];                                                          \
        if (idx < set->size && set->ids[idx] == id)                                                                    \
        {                                                                                                              \
            return idx;                                                                                                \
        }                                                                                                              \
                                                                                                                       \
        return BX_SPSET_INVALID_INDEX;                                                                                 \
    }                                                                                                                  \
                                                                                                                       \
    T* bx_spset_##NAME##_get(const bx_spset_##NAME* set, uint32_t id)                                                  \
    {                                                                                                                  \
        size_t idx = bx_spset_##NAME##_internal_idx(set, id);                                                          \
        return (idx == BX_SPSET_INVALID_INDEX) ? NULL : &set->dense[idx];                                              \
    }                                                                                                                  \
                                                                                                                       \
    bool bx_spset_##NAME##_contains(const bx_spset_##NAME* set, uint32_t id)                                           \
    {                                                                                                                  \
        return bx_spset_##NAME##_internal_idx(set, id) != BX_SPSET_INVALID_INDEX;                                      \
    }                                                                                                                  \
                                                                                                                       \
    bool bx_spset_##NAME##_insert(bx_spset_##NAME* set, uint32_t id, T value)                                          \
    {                                                                                                                  \
        size_t existing = bx_spset_##NAME##_internal_idx(set, id);                                                     \
        if (existing != BX_SPSET_INVALID_INDEX)                                                                        \
        {                                                                                                              \
            set->dense[existing] = value;                                                                              \
            return false;                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        size_t p = (size_t)id >> BX_SPSET_PAGE_SHIFT;                                                                  \
        if (p >= set->page_count)                                                                                      \
        {                                                                                                              \
            bx_spset_##NAME##_reserve_ids(set, id);                                                                    \
        }                                                                                                              \
                                                                                                                       \
        if (!set->sparse[p])                                                                                           \
        {                                                                                                              \
            set->sparse[p] = (size_t*)ALLOC_FN(BX_SPSET_PAGE_SIZE * sizeof(size_t));                                   \
            memset(set->sparse[p], 0xFF, BX_SPSET_PAGE_SIZE * sizeof(size_t));                                         \
        }                                                                                                              \
                                                                                                                       \
        /* Check if we need to grow the dense and ID arrays */                                                         \
        if (set->size >= set->capacity)                                                                                \
        {                                                                                                              \
            size_t new_cap = set->capacity == 0 ? 8 : set->capacity * 2;                                               \
            T* new_dense = (T*)ALLOC_FN(new_cap * sizeof(T));                                                          \
            uint32_t* new_ids = (uint32_t*)ALLOC_FN(new_cap * sizeof(uint32_t));                                       \
                                                                                                                       \
            if (set->dense)                                                                                            \
            {                                                                                                          \
                memcpy(new_dense, set->dense, set->size * sizeof(T));                                                  \
                memcpy(new_ids, set->ids, set->size * sizeof(uint32_t));                                               \
                FREE_FN(set->dense);                                                                                   \
                FREE_FN(set->ids);                                                                                     \
            }                                                                                                          \
                                                                                                                       \
            set->dense = new_dense;                                                                                    \
            set->ids = new_ids;                                                                                        \
            set->capacity = new_cap;                                                                                   \
        }                                                                                                              \
                                                                                                                       \
        size_t new_idx = set->size++;                                                                                  \
        set->sparse[p][id & BX_SPSET_PAGE_MASK] = new_idx;                                                             \
        set->dense[new_idx] = value;                                                                                   \
        set->ids[new_idx] = id;                                                                                        \
                                                                                                                       \
        return true;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    void bx_spset_##NAME##_erase(bx_spset_##NAME* set, uint32_t id)                                                    \
    {                                                                                                                  \
        size_t idx = bx_spset_##NAME##_internal_idx(set, id);                                                          \
        if (idx == BX_SPSET_INVALID_INDEX)                                                                             \
        {                                                                                                              \
            return;                                                                                                    \
        }                                                                                                              \
                                                                                                                       \
        size_t last_idx = set->size - 1;                                                                               \
        uint32_t last_id = set->ids[last_idx];                                                                         \
                                                                                                                       \
        /* Swap-and-pop: Move the last element into the hole to keep dense array packed */                             \
        set->dense[idx] = set->dense[last_idx];                                                                        \
        set->ids[idx] = last_id;                                                                                       \
                                                                                                                       \
        /* Update the sparse pointer for the element we just moved */                                                  \
        size_t p_last = (size_t)last_id >> BX_SPSET_PAGE_SHIFT;                                                        \
        set->sparse[p_last][last_id & BX_SPSET_PAGE_MASK] = idx;                                                       \
                                                                                                                       \
        set->size--;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    void bx_spset_##NAME##_clear(bx_spset_##NAME* set)                                                                 \
    {                                                                                                                  \
        /* We keep the memory allocated for reuse, just reset the logical size */                                      \
        set->size = 0;                                                                                                 \
    }


#define BX_SPSET_SOURCE_2(T, NAME)                                                                                     \
    BX_SPSET_SOURCE_INTERNAL(T, NAME, bx_alloc, bx_free)

#define BX_SPSET_SOURCE_4(T, NAME, ALLOC, FREE)                                                                        \
    BX_SPSET_SOURCE_INTERNAL(T, NAME, ALLOC, FREE)

#define BX_SPSET_SOURCE_GET_MACRO(_1, _2, _3, _4, NAME, ...) NAME
#define BX_SPSET_SOURCE(...)                                                                                           \
    BX_SPSET_SOURCE_GET_MACRO(__VA_ARGS__, BX_SPSET_SOURCE_4, BX_SPSET_SOURCE_3_UNUSED, BX_SPSET_SOURCE_2)(__VA_ARGS__)
