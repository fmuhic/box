#pragma once

#include <stddef.h>
#include <stdint.h>

#include "box/hmap_core.h"

#define BX_HMAP_DECLARE(K, V, NAME, HASH_FN, EQ_FN)                                           \
    typedef struct bx_hmap_##NAME##_entry                                                     \
    {                                                                                         \
        K key;                                                                                \
        V value;                                                                              \
    } bx_hmap_##NAME##_entry;                                                                 \
                                                                                              \
    static inline uint64_t bx_hmap_##NAME##_hash_adapter(const void* k)                       \
    {                                                                                         \
        return HASH_FN(*(K const*)k);                                                         \
    }                                                                                         \
    static inline bool bx_hmap_##NAME##_eq_adapter(const void* a, const void* b)              \
    {                                                                                         \
        return EQ_FN(*(K const*)a, *(K const*)b);                                             \
    }                                                                                         \
                                                                                              \
    static const bx_hmap_vtable bx_hmap_##NAME##_vtable = {                                   \
        sizeof(bx_hmap_##NAME##_entry),                                                       \
        sizeof(K),                                                                            \
        offsetof(bx_hmap_##NAME##_entry, value),                                              \
        sizeof(V),                                                                            \
        bx_hmap_##NAME##_hash_adapter,                                                        \
        bx_hmap_##NAME##_eq_adapter,                                                          \
    };                                                                                        \
                                                                                              \
    typedef struct bx_hmap_##NAME                                                             \
    {                                                                                         \
        bx_hmap base;                                                                         \
    } bx_hmap_##NAME;                                                                         \
                                                                                              \
    static inline void bx_hmap_##NAME##_init(bx_hmap_##NAME* typed_map)                       \
    {                                                                                         \
        bx_hmap_init(&typed_map->base, &bx_hmap_##NAME##_vtable);                             \
    }                                                                                         \
    static inline void bx_hmap_##NAME##_init_capacity(bx_hmap_##NAME* typed_map,              \
                                                      uint32_t capacity)                      \
    {                                                                                         \
        bx_hmap_init_capacity(&typed_map->base, &bx_hmap_##NAME##_vtable, capacity);          \
    }                                                                                         \
    static inline void bx_hmap_##NAME##_drop(bx_hmap_##NAME* typed_map)                       \
    {                                                                                         \
        bx_hmap_drop(&typed_map->base);                                                       \
    }                                                                                         \
    static inline void bx_hmap_##NAME##_clear(bx_hmap_##NAME* typed_map)                      \
    {                                                                                         \
        bx_hmap_clear(&typed_map->base);                                                      \
    }                                                                                         \
    static inline uint32_t bx_hmap_##NAME##_size(const bx_hmap_##NAME* typed_map)             \
    {                                                                                         \
        return typed_map->base.size;                                                          \
    }                                                                                         \
    static inline uint32_t bx_hmap_##NAME##_bucket_count(const bx_hmap_##NAME* typed_map)     \
    {                                                                                         \
        return typed_map->base.bucket_count;                                                  \
    }                                                                                         \
    static inline void bx_hmap_##NAME##_reserve(bx_hmap_##NAME* typed_map, uint32_t capacity) \
    {                                                                                         \
        bx_hmap_reserve(&typed_map->base, capacity);                                          \
    }                                                                                         \
    static inline void bx_hmap_##NAME##_insert(bx_hmap_##NAME* typed_map, K key, V value)     \
    {                                                                                         \
        bx_hmap_insert(&typed_map->base, &key, &value);                                       \
    }                                                                                         \
    static inline void bx_hmap_##NAME##_erase(bx_hmap_##NAME* typed_map, K key)               \
    {                                                                                         \
        bx_hmap_erase(&typed_map->base, &key);                                                \
    }                                                                                         \
    static inline V* bx_hmap_##NAME##_get(bx_hmap_##NAME* typed_map, K key)                   \
    {                                                                                         \
        bx_hmap* map = &typed_map->base;                                                      \
        if (map->bucket_count == 0)                                                           \
            return NULL;                                                                      \
        uint64_t hash = HASH_FN(key);                                                         \
        uint32_t mask = map->bucket_count - 1;                                                \
        uint32_t index = (uint32_t)(hash & mask);                                             \
        uint8_t mini_hash = BX_HMAP_MINI_HASH(hash);                                          \
        uint16_t dist = 1;                                                                    \
        bx_hmap_##NAME##_entry* table = (bx_hmap_##NAME##_entry*)map->table;                  \
        while (dist <= map->meta[index].dist)                                                 \
        {                                                                                     \
            if (map->meta[index].mini_hash == mini_hash && EQ_FN(table[index].key, key))      \
            {                                                                                 \
                return &table[index].value;                                                   \
            }                                                                                 \
            index = (index + 1) & mask;                                                       \
            dist++;                                                                           \
        }                                                                                     \
        return NULL;                                                                          \
    }                                                                                         \
    static inline bool bx_hmap_##NAME##_contains(bx_hmap_##NAME* typed_map, K key)            \
    {                                                                                         \
        return bx_hmap_##NAME##_get(typed_map, key) != NULL;                                  \
    }
