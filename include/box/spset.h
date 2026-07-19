#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "box/spset_core.h"

#define BX_SPSET_DECLARE(T, NAME)                                                         \
    typedef struct bx_spset_##NAME                                                        \
    {                                                                                     \
        bx_spset base;                                                                    \
    } bx_spset_##NAME;                                                                    \
                                                                                          \
    static inline void bx_spset_##NAME##_init(bx_spset_##NAME* set)                       \
    {                                                                                     \
        bx_spset_init(&set->base, sizeof(T));                                             \
    }                                                                                     \
    static inline void bx_spset_##NAME##_init_capacity(bx_spset_##NAME* set,              \
                                                       uint32_t capacity)                 \
    {                                                                                     \
        bx_spset_init_capacity(&set->base, sizeof(T), capacity);                          \
    }                                                                                     \
    static inline void bx_spset_##NAME##_drop(bx_spset_##NAME* set)                       \
    {                                                                                     \
        bx_spset_drop(&set->base);                                                        \
    }                                                                                     \
    static inline void bx_spset_##NAME##_reserve(bx_spset_##NAME* set, uint32_t capacity) \
    {                                                                                     \
        bx_spset_reserve(&set->base, capacity);                                           \
    }                                                                                     \
    static inline void bx_spset_##NAME##_reserve_ids(bx_spset_##NAME* set,                \
                                                     uint32_t max_id)                     \
    {                                                                                     \
        bx_spset_reserve_ids(&set->base, max_id);                                         \
    }                                                                                     \
    static inline void bx_spset_##NAME##_erase(bx_spset_##NAME* set, uint32_t id)         \
    {                                                                                     \
        bx_spset_erase(&set->base, id);                                                   \
    }                                                                                     \
    static inline void bx_spset_##NAME##_clear(bx_spset_##NAME* set)                      \
    {                                                                                     \
        bx_spset_clear(&set->base);                                                       \
    }                                                                                     \
    static inline bool bx_spset_##NAME##_contains(const bx_spset_##NAME* set,             \
                                                  uint32_t id)                            \
    {                                                                                     \
        return bx_spset_contains(&set->base, id);                                         \
    }                                                                                     \
    static inline uint32_t bx_spset_##NAME##_size(const bx_spset_##NAME* set)             \
    {                                                                                     \
        return bx_spset_size(&set->base);                                                 \
    }                                                                                     \
    static inline uint32_t bx_spset_##NAME##_capacity(const bx_spset_##NAME* set)         \
    {                                                                                     \
        return bx_spset_capacity(&set->base);                                             \
    }                                                                                     \
                                                                                          \
    static inline T* bx_spset_##NAME##_data(bx_spset_##NAME* set)                         \
    {                                                                                     \
        return (T*)set->base.dense.data;                                                  \
    }                                                                                     \
    static inline uint32_t* bx_spset_##NAME##_ids(bx_spset_##NAME* set)                   \
    {                                                                                     \
        return (uint32_t*)set->base.ids.data;                                             \
    }                                                                                     \
    static inline T* bx_spset_##NAME##_get(const bx_spset_##NAME* set, uint32_t id)       \
    {                                                                                     \
        uint32_t idx = bx_spset_find(&set->base, id);                                     \
        return (idx == BX_SPSET_INVALID_INDEX) ? NULL : &((T*)set->base.dense.data)[idx]; \
    }                                                                                     \
    static inline bool bx_spset_##NAME##_insert(bx_spset_##NAME* set, uint32_t id,        \
                                                T value)                                  \
    {                                                                                     \
        bool is_new;                                                                      \
        uint32_t idx = bx_spset_insert_slot(&set->base, id, &is_new);                     \
        ((T*)set->base.dense.data)[idx] = value;                                          \
        return is_new;                                                                    \
    }
