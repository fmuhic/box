#pragma once

// Verbatim copy of the ORIGINAL macro-per-type hmap implementation,
// with every identifier renamed (bx_ -> bxold_, BX_ -> BXOLD_) so it can be
// compiled side by side with the new type-erased core for benchmarking.

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "box/core.h"

#define BXOLD_HMAP_MAX_LOAD_FACTOR 0.80f
#define BXOLD_HMAP_DIST_MASK       0x3ffU

typedef struct bxold_hmap_meta
{
    uint16_t mini_hash : 6;
    uint16_t dist : 10;
} bxold_hmap_meta;

#define BXOLD_HMAP_DECLARE(K, V, NAME)                                         \
    typedef struct bxold_hmap_##NAME##_entry                                   \
    {                                                                          \
        K key;                                                                 \
        V value;                                                               \
    } bxold_hmap_##NAME##_entry;                                               \
                                                                               \
    typedef struct bxold_hmap_##NAME                                           \
    {                                                                          \
        bxold_hmap_##NAME##_entry* table;                                      \
        bxold_hmap_meta* meta;                                                 \
        size_t size;                                                           \
        size_t bucket_count;                                                   \
    } bxold_hmap_##NAME;                                                       \
                                                                               \
    void bxold_hmap_##NAME##_init(bxold_hmap_##NAME* map);                     \
    void bxold_hmap_##NAME##_drop(bxold_hmap_##NAME* map);                     \
    void bxold_hmap_##NAME##_reserve(bxold_hmap_##NAME* map, size_t capacity); \
    void bxold_hmap_##NAME##_insert(bxold_hmap_##NAME* map, K key, V value);   \
    void bxold_hmap_##NAME##_erase(bxold_hmap_##NAME* map, K key);             \
    V* bxold_hmap_##NAME##_get(const bxold_hmap_##NAME* map, K key);

#define BXOLD_HMAP_SOURCE_INTERNAL(K, V, NAME, BX_HASH_FN, BX_EQUALITY_FN, ALLOC_FN, FREE_FN)             \
    static size_t bxold_hmap_##NAME##_find_bucket(const bxold_hmap_##NAME* map, K key, bool* found)       \
    {                                                                                                     \
        if (map->bucket_count == 0)                                                                       \
        {                                                                                                 \
            *found = false;                                                                               \
            return 0;                                                                                     \
        }                                                                                                 \
                                                                                                          \
        uint64_t hash = BX_HASH_FN(key);                                                                  \
        size_t mask = map->bucket_count - 1;                                                              \
        size_t index = hash & mask;                                                                       \
        uint8_t mini_hash = (uint8_t)((hash >> 24) & 0x3fU);                                              \
        uint16_t dist = 1;                                                                                \
        *found = false;                                                                                   \
                                                                                                          \
        while (dist <= map->meta[index].dist)                                                             \
        {                                                                                                 \
            if (map->meta[index].mini_hash == mini_hash && BX_EQUALITY_FN(map->table[index].key, key))    \
            {                                                                                             \
                *found = true;                                                                            \
                return index;                                                                             \
            }                                                                                             \
            index = (index + 1) & mask;                                                                   \
            dist++;                                                                                       \
        }                                                                                                 \
        return index;                                                                                     \
    }                                                                                                     \
                                                                                                          \
    V* bxold_hmap_##NAME##_get(const bxold_hmap_##NAME* map, K key)                                       \
    {                                                                                                     \
        bool found;                                                                                       \
        size_t index = bxold_hmap_##NAME##_find_bucket(map, key, &found);                                 \
        return found ? &map->table[index].value : NULL;                                                   \
    }                                                                                                     \
                                                                                                          \
    void bxold_hmap_##NAME##_init(bxold_hmap_##NAME* map)                                                 \
    {                                                                                                     \
        memset(map, 0, sizeof(bxold_hmap_##NAME));                                                        \
    }                                                                                                     \
                                                                                                          \
    void bxold_hmap_##NAME##_drop(bxold_hmap_##NAME* map)                                                 \
    {                                                                                                     \
        FREE_FN(map->table);                                                                              \
        FREE_FN(map->meta);                                                                               \
        memset(map, 0, sizeof(bxold_hmap_##NAME));                                                        \
    }                                                                                                     \
                                                                                                          \
    void bxold_hmap_##NAME##_reserve(bxold_hmap_##NAME* map, size_t capacity)                             \
    {                                                                                                     \
        size_t new_bucks = (size_t)((float)capacity / BXOLD_HMAP_MAX_LOAD_FACTOR) + 4;                    \
        new_bucks = bx_next_pow2(new_bucks);                                                              \
        if (new_bucks <= map->bucket_count)                                                               \
            return;                                                                                       \
                                                                                                          \
        bxold_hmap_##NAME old = *map;                                                                     \
        map->bucket_count = new_bucks;                                                                    \
        map->table = (bxold_hmap_##NAME##_entry*)ALLOC_FN(new_bucks * sizeof(bxold_hmap_##NAME##_entry)); \
        memset(map->table, 0, new_bucks * sizeof(bxold_hmap_##NAME##_entry));                             \
        map->meta = (bxold_hmap_meta*)ALLOC_FN((new_bucks + 1) * sizeof(bxold_hmap_meta));                \
        memset(map->meta, 0, (new_bucks + 1) * sizeof(bxold_hmap_meta));                                  \
        map->size = 0;                                                                                    \
                                                                                                          \
        map->meta[new_bucks].dist = BXOLD_HMAP_DIST_MASK;                                                 \
        for (size_t i = 0; i < old.bucket_count; i++)                                                     \
        {                                                                                                 \
            if (old.meta[i].dist > 0)                                                                     \
            {                                                                                             \
                bxold_hmap_##NAME##_insert(map, old.table[i].key, old.table[i].value);                    \
            }                                                                                             \
        }                                                                                                 \
                                                                                                          \
        FREE_FN(old.table);                                                                               \
        FREE_FN(old.meta);                                                                                \
    }                                                                                                     \
                                                                                                          \
    void bxold_hmap_##NAME##_insert(bxold_hmap_##NAME* map, K key, V value)                               \
    {                                                                                                     \
        if (map->size >= (size_t)((float)map->bucket_count * BXOLD_HMAP_MAX_LOAD_FACTOR))                 \
        {                                                                                                 \
            bxold_hmap_##NAME##_reserve(map, map->size * 2 + 2);                                          \
        }                                                                                                 \
        bool found;                                                                                       \
        size_t index = bxold_hmap_##NAME##_find_bucket(map, key, &found);                                 \
        if (found)                                                                                        \
        {                                                                                                 \
            map->table[index].value = value;                                                              \
            return;                                                                                       \
        }                                                                                                 \
        uint64_t hash = BX_HASH_FN(key);                                                                  \
        bxold_hmap_meta new_meta = {                                                                      \
            .mini_hash = (uint16_t)((hash >> 24) & 0x3fU),                                                \
            .dist = (uint16_t)((index - (hash & (map->bucket_count - 1)) + map->bucket_count) &           \
                               (map->bucket_count - 1)) +                                                 \
                    1                                                                                     \
        };                                                                                                \
        bxold_hmap_##NAME##_entry new_entry = { key, value };                                             \
        size_t mask = map->bucket_count - 1;                                                              \
                                                                                                          \
        while (map->meta[index].dist != 0)                                                                \
        {                                                                                                 \
            if (map->meta[index].dist < new_meta.dist)                                                    \
            {                                                                                             \
                bxold_hmap_meta resident_meta = map->meta[index];                                         \
                map->meta[index] = new_meta;                                                              \
                new_meta = resident_meta;                                                                 \
                                                                                                          \
                bxold_hmap_##NAME##_entry resident_entry = map->table[index];                             \
                map->table[index] = new_entry;                                                            \
                new_entry = resident_entry;                                                               \
            }                                                                                             \
            index = (index + 1) & mask;                                                                   \
            new_meta.dist++;                                                                              \
        }                                                                                                 \
                                                                                                          \
        map->meta[index] = new_meta;                                                                      \
        map->table[index] = new_entry;                                                                    \
        map->size++;                                                                                      \
    }                                                                                                     \
                                                                                                          \
    void bxold_hmap_##NAME##_erase(bxold_hmap_##NAME* map, K key)                                         \
    {                                                                                                     \
        bool found;                                                                                       \
        size_t index = bxold_hmap_##NAME##_find_bucket(map, key, &found);                                 \
        if (!found)                                                                                       \
            return;                                                                                       \
                                                                                                          \
        size_t mask = map->bucket_count - 1;                                                              \
        size_t i = index, j = i;                                                                          \
        for (;;)                                                                                          \
        {                                                                                                 \
            j = (j + 1) & mask;                                                                           \
            if (map->meta[j].dist < 2)                                                                    \
            {                                                                                             \
                break;                                                                                    \
            }                                                                                             \
            map->table[i] = map->table[j];                                                                \
            map->meta[i] = map->meta[j];                                                                  \
            map->meta[i].dist--;                                                                          \
            i = j;                                                                                        \
        }                                                                                                 \
        map->meta[i].dist = 0;                                                                            \
        map->size--;                                                                                      \
    }

#define BXOLD_HMAP_SOURCE_5(K, V, NAME, HASH, EQ) \
    BXOLD_HMAP_SOURCE_INTERNAL(K, V, NAME, HASH, EQ, bx_alloc, bx_free)

#define BXOLD_HMAP_SOURCE_7(K, V, NAME, HASH, EQ, ALLOC_FN, FREE_FN) \
    BXOLD_HMAP_SOURCE_INTERNAL(K, V, NAME, HASH, EQ, ALLOC_FN, FREE_FN)

#define BXOLD_HMAP_SOURCE_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, NAME, ...) NAME
#define BXOLD_HMAP_SOURCE(...)                                                                                     \
    BXOLD_HMAP_SOURCE_GET_MACRO(__VA_ARGS__, BXOLD_HMAP_SOURCE_7, BXOLD_HMAP_SOURCE_6_UNUSED, BXOLD_HMAP_SOURCE_5) \
    (__VA_ARGS__)
