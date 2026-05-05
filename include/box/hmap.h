#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "box/core.h"

#define BX_HMAP_MAX_LOAD_FACTOR 0.80f
#define BX_HMAP_DIST_MASK 0x3ffU

static inline size_t next_pow2(size_t x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x + 1;
}

typedef struct bx_hmap_meta
{
    uint16_t mini_hash: 6; // Quick check before calling the expensive equality function
    uint16_t dist: 10; // Distance from ideal bucket. 0 means the slot is empty.
} bx_hmap_meta;

#define BX_HMAP_DECLARE(K, V, NAME)                                                                                    \
    typedef struct bx_hmap_##NAME##_entry                                                                              \
    {                                                                                                                  \
        K key;                                                                                                         \
        V value;                                                                                                       \
    } bx_hmap_##NAME##_entry;                                                                                          \
                                                                                                                       \
    typedef struct bx_hmap_##NAME                                                                                      \
    {                                                                                                                  \
        bx_hmap_##NAME##_entry* table;                                                                                 \
        bx_hmap_meta* meta;                                                                                            \
        size_t size;                                                                                                   \
        size_t bucket_count;                                                                                           \
    } bx_hmap_##NAME;                                                                                                  \
                                                                                                                       \
    void bx_hmap_##NAME##_init(bx_hmap_##NAME* map);                                                                   \
    void bx_hmap_##NAME##_drop(bx_hmap_##NAME* map);                                                                   \
    void bx_hmap_##NAME##_reserve(bx_hmap_##NAME* map, size_t capacity);                                               \
    void bx_hmap_##NAME##_insert(bx_hmap_##NAME* map, K key, V value);                                                 \
    void bx_hmap_##NAME##_erase(bx_hmap_##NAME* map, K key);                                                           \
    V* bx_hmap_##NAME##_get(const bx_hmap_##NAME* map, K key);

#define BX_HMAP_SOURCE_INTERNAL(K, V, NAME, BX_HASH_FN, BX_EQUALITY_FN, ALLOC_FN, FREE_FN)                             \
    static size_t bx_hmap_##NAME##_find_bucket(const bx_hmap_##NAME* map, K key, bool* found)                          \
    {                                                                                                                  \
        if (map->bucket_count == 0) { *found = false; return 0; }                                                      \
                                                                                                                       \
        uint64_t hash = BX_HASH_FN(key);                                                                               \
        size_t mask = map->bucket_count - 1;                                                                           \
        size_t index = hash & mask;                                                                                    \
        uint8_t mini_hash = (uint8_t)((hash >> 24) & 0x3fU);                                                           \
        uint16_t dist = 1;                                                                                             \
        *found = false;                                                                                                \
                                                                                                                       \
        while (dist <= map->meta[index].dist)                                                                          \
        {                                                                                                              \
            if (map->meta[index].mini_hash == mini_hash && BX_EQUALITY_FN(map->table[index].key, key))                 \
            {                                                                                                          \
                *found = true;                                                                                         \
                return index;                                                                                          \
            }                                                                                                          \
            index = (index + 1) & mask;                                                                                \
            dist++;                                                                                                    \
        }                                                                                                              \
        return index;                                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    V* bx_hmap_##NAME##_get(const bx_hmap_##NAME* map, K key)                                                          \
    {                                                                                                                  \
        bool found;                                                                                                    \
        size_t index = bx_hmap_##NAME##_find_bucket(map, key, &found);                                                 \
        return found ? &map->table[index].value : NULL;                                                                \
    }                                                                                                                  \
                                                                                                                       \
    void bx_hmap_##NAME##_init(bx_hmap_##NAME* map) {                                                                  \
        memset(map, 0, sizeof(bx_hmap_##NAME));                                                                        \
    }                                                                                                                  \
                                                                                                                       \
    void bx_hmap_##NAME##_drop(bx_hmap_##NAME* map) {                                                                  \
        FREE_FN(map->table);                                                                                           \
        FREE_FN(map->meta);                                                                                            \
        memset(map, 0, sizeof(bx_hmap_##NAME));                                                                        \
    }                                                                                                                  \
                                                                                                                       \
    void bx_hmap_##NAME##_reserve(bx_hmap_##NAME* map, size_t capacity) {                                              \
        size_t new_bucks = (size_t)((float)capacity / BX_HMAP_MAX_LOAD_FACTOR) + 4;                                    \
        new_bucks = next_pow2(new_bucks);                                                                              \
        if (new_bucks <= map->bucket_count) return;                                                                    \
                                                                                                                       \
        bx_hmap_##NAME old = *map;                                                                                     \
        map->bucket_count = new_bucks;                                                                                 \
        map->table = (bx_hmap_##NAME##_entry*) ALLOC_FN(new_bucks * sizeof(bx_hmap_##NAME##_entry));                   \
        memset(map->table, 0, new_bucks * sizeof(bx_hmap_##NAME##_entry));                                             \
        map->meta = (bx_hmap_meta*) ALLOC_FN((new_bucks + 1) * sizeof(bx_hmap_meta));                                  \
        memset(map->meta, 0, (new_bucks + 1) * sizeof(bx_hmap_meta));                                                  \
        map->size = 0;                                                                                                 \
                                                                                                                       \
        /* Sentinel value at the end of meta to prevent overflow during probing. */                                    \
        map->meta[new_bucks].dist = BX_HMAP_DIST_MASK;                                                                 \
        for (size_t i = 0; i < old.bucket_count; i++)                                                                  \
        {                                                                                                              \
            if (old.meta[i].dist > 0)                                                                                  \
            {                                                                                                          \
                bx_hmap_##NAME##_insert(map, old.table[i].key, old.table[i].value);                                    \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        FREE_FN(old.table);                                                                                            \
        FREE_FN(old.meta);                                                                                             \
    }                                                                                                                  \
                                                                                                                       \
    void bx_hmap_##NAME##_insert(bx_hmap_##NAME* map, K key, V value) {                                                \
        if (map->size >= (size_t)((float)map->bucket_count * BX_HMAP_MAX_LOAD_FACTOR))                                 \
        {                                                                                                              \
            bx_hmap_##NAME##_reserve(map, map->size * 2 + 2);                                                          \
        }                                                                                                              \
        bool found;                                                                                                    \
        size_t index = bx_hmap_##NAME##_find_bucket(map, key, &found);                                                 \
        if (found)                                                                                                     \
        {                                                                                                              \
            map->table[index].value = value;                                                                           \
            return;                                                                                                    \
        }                                                                                                              \
        uint64_t hash = BX_HASH_FN(key);                                                                               \
        bx_hmap_meta new_meta =                                                                                        \
        {                                                                                                              \
            .mini_hash = (uint16_t)((hash >> 24) & 0x3fU),                                                             \
            .dist = (uint16_t)(                                                                                        \
                    (index - (hash & (map->bucket_count - 1)) + map->bucket_count) &                                   \
                    (map->bucket_count - 1)                                                                            \
                ) + 1                                                                                                  \
        };                                                                                                             \
        bx_hmap_##NAME##_entry new_entry = {key, value};                                                               \
        size_t mask = map->bucket_count - 1;                                                                           \
                                                                                                                       \
        /* Robin Hood insert: If we find a "richer" element (one closer to its home) */                                \
        /* we kick it out and take its spot, then continue trying to insert the displaced element. */                  \
        while (map->meta[index].dist != 0) {                                                                           \
            if (map->meta[index].dist < new_meta.dist) {                                                               \
                bx_hmap_meta resident_meta = map->meta[index];                                                         \
                map->meta[index] = new_meta;                                                                           \
                new_meta = resident_meta;                                                                              \
                                                                                                                       \
                bx_hmap_##NAME##_entry resident_entry = map->table[index];                                             \
                map->table[index] = new_entry;                                                                         \
                new_entry = resident_entry;                                                                            \
            }                                                                                                          \
            index = (index + 1) & mask;                                                                                \
            new_meta.dist++;                                                                                           \
        }                                                                                                              \
                                                                                                                       \
        map->meta[index] = new_meta;                                                                                   \
        map->table[index] = new_entry;                                                                                 \
        map->size++;                                                                                                   \
    }                                                                                                                  \
                                                                                                                       \
    void bx_hmap_##NAME##_erase(bx_hmap_##NAME* map, K key) {                                                          \
        bool found;                                                                                                    \
        size_t index = bx_hmap_##NAME##_find_bucket(map, key, &found);                                                 \
        if (!found) return;                                                                                            \
                                                                                                                       \
        /* Backward Shift Erasure: Instead of using "tombstones" (which clutter the map), */                           \
        /* we shift neighboring elements back to fill the hole, as long as they aren't at their home slot. */          \
        size_t mask = map->bucket_count - 1;                                                                           \
        size_t i = index, j = i;                                                                                       \
        for (;;) {                                                                                                     \
            j = (j + 1) & mask;                                                                                        \
            /* If the next element is at its ideal spot (dist == 1) or empty (dist == 0), stop. */                     \
            if (map->meta[j].dist < 2)                                                                                 \
            {                                                                                                          \
                break;                                                                                                 \
            }                                                                                                          \
            map->table[i] = map->table[j];                                                                             \
            map->meta[i] = map->meta[j];                                                                               \
            map->meta[i].dist--;                                                                                       \
            i = j;                                                                                                     \
        }                                                                                                              \
        map->meta[i].dist = 0; /* Mark the final shifted slot as empty. */                                             \
        map->size--;                                                                                                   \
    }


// This allows us to call BX_HMAP_SOURCE with extra alloc and free arguments
#define BX_HMAP_SOURCE_5(K, V, NAME, HASH, EQ)                                                                         \
    BX_HMAP_SOURCE_INTERNAL(K, V, NAME, HASH, EQ, bx_alloc, bx_free)

#define BX_HMAP_SOURCE_7(K, V, NAME, HASH, EQ, ALLOC_FN, FREE_FN)                                                      \
    BX_HMAP_SOURCE_INTERNAL(K, V, NAME, HASH, EQ, ALLOC_FN, FREE_FN)

#define BX_HMAP_SOURCE_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, NAME, ...) NAME
#define BX_HMAP_SOURCE(...)                                                                                            \
    BX_HMAP_SOURCE_GET_MACRO(__VA_ARGS__, BX_HMAP_SOURCE_7, BX_HMAP_SOURCE_6_UNUSED, BX_HMAP_SOURCE_5)(__VA_ARGS__)
