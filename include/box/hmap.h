#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "box/core.h"
#include "box/hmap_core.h"

#define BX_HMAP_DECLARE(K, V, NAME, HASH_FN, EQ_FN)                                           \
    typedef struct bx_hmap_##NAME##_entry                                                     \
    {                                                                                         \
        K key;                                                                                \
        V value;                                                                              \
    } bx_hmap_##NAME##_entry;                                                                 \
                                                                                              \
    typedef struct bx_hmap_##NAME                                                             \
    {                                                                                         \
        bx_hmap_##NAME##_entry* table;                                                        \
        bx_hmap_meta* meta;                                                                   \
        uint32_t size;                                                                        \
        uint32_t bucket_count;                                                                \
        /* size at which the next resize fires, cached so insert compares two                 \
           integers instead of recomputing the load factor every call */                      \
        uint32_t grow_at;                                                                     \
    } bx_hmap_##NAME;                                                                         \
                                                                                              \
    /* resize rehashes through insert, and insert grows through resize */                     \
    static inline void bx_hmap_##NAME##_insert(bx_hmap_##NAME* map, K key, V value);          \
                                                                                              \
    /* On a miss returns the bucket where the probe run gave out, which is where              \
       insert wants it. `out_hash` is always written so insert need not rehash. */            \
    static inline uint32_t bx_hmap_##NAME##_find_bucket(const bx_hmap_##NAME* map, K key,     \
                                                        uint64_t* out_hash, bool* found)      \
    {                                                                                         \
        uint64_t hash = HASH_FN(key);                                                         \
        *out_hash = hash;                                                                     \
        *found = false;                                                                       \
        if (map->bucket_count == 0)                                                           \
        {                                                                                     \
            return 0;                                                                         \
        }                                                                                     \
                                                                                              \
        uint32_t mask = map->bucket_count - 1;                                                \
        uint32_t index = (uint32_t)(hash & mask);                                             \
        uint8_t mini_hash = BX_HMAP_MINI_HASH(hash);                                          \
        uint16_t dist = 1;                                                                    \
                                                                                              \
        while (dist <= map->meta[index].dist)                                                 \
        {                                                                                     \
            if (map->meta[index].mini_hash == mini_hash && EQ_FN(map->table[index].key, key)) \
            {                                                                                 \
                *found = true;                                                                \
                return index;                                                                 \
            }                                                                                 \
            index = (index + 1) & mask;                                                       \
            dist++;                                                                           \
        }                                                                                     \
        return index;                                                                         \
    }                                                                                         \
                                                                                              \
    static inline void bx_hmap_##NAME##_init(bx_hmap_##NAME* map)                             \
    {                                                                                         \
        map->table = NULL;                                                                    \
        map->meta = NULL;                                                                     \
        map->size = 0;                                                                        \
        map->bucket_count = 0;                                                                \
        map->grow_at = 0;                                                                     \
    }                                                                                         \
                                                                                              \
    static inline void bx_hmap_##NAME##_drop(bx_hmap_##NAME* map)                             \
    {                                                                                         \
        bx_free(map->table);                                                                  \
        bx_free(map->meta);                                                                   \
        bx_hmap_##NAME##_init(map);                                                           \
    }                                                                                         \
                                                                                              \
    static inline void bx_hmap_##NAME##_clear(bx_hmap_##NAME* map)                            \
    {                                                                                         \
        if (map->bucket_count == 0)                                                           \
        {                                                                                     \
            return;                                                                           \
        }                                                                                     \
        memset(map->meta, 0, ((size_t)map->bucket_count + 1) * sizeof(bx_hmap_meta));         \
        /* The memset wiped the guard slot; reinstall it or reinserts walk off the end */     \
        map->meta[map->bucket_count].dist = BX_HMAP_DIST_MASK;                                \
        map->size = 0;                                                                        \
    }                                                                                         \
                                                                                              \
    static inline uint32_t bx_hmap_##NAME##_size(const bx_hmap_##NAME* map)                   \
    {                                                                                         \
        return map->size;                                                                     \
    }                                                                                         \
                                                                                              \
    static inline uint32_t bx_hmap_##NAME##_bucket_count(const bx_hmap_##NAME* map)           \
    {                                                                                         \
        return map->bucket_count;                                                             \
    }                                                                                         \
                                                                                              \
    /* Bucket-space resize: `buckets` is a bucket count, already the size the table           \
       should become. Element-space callers go through _reserve below. */                     \
    BX_HMAP_COLD void bx_hmap_##NAME##_resize(bx_hmap_##NAME* map, uint64_t buckets)          \
    {                                                                                         \
        if (buckets < BX_HMAP_MIN_BUCKETS)                                                    \
        {                                                                                     \
            buckets = BX_HMAP_MIN_BUCKETS;                                                    \
        }                                                                                     \
        uint64_t new_bucket_count = bx_next_pow2(buckets);                                    \
        assert(new_bucket_count <= UINT32_MAX && "bx_hmap: bucket count exceeds uint32_t");   \
        assert(new_bucket_count >= BX_HMAP_MIN_BUCKETS && "bx_hmap: next_pow2 below floor");  \
        if (new_bucket_count <= map->bucket_count)                                            \
        {                                                                                     \
            return;                                                                           \
        }                                                                                     \
                                                                                              \
        bx_hmap_##NAME old = *map;                                                            \
        size_t table_bytes = (size_t)new_bucket_count * sizeof(bx_hmap_##NAME##_entry);       \
        size_t meta_bytes = ((size_t)new_bucket_count + 1) * sizeof(bx_hmap_meta);            \
                                                                                              \
        map->bucket_count = (uint32_t)new_bucket_count;                                       \
        map->table = (bx_hmap_##NAME##_entry*)bx_alloc(table_bytes);                          \
        assert(map->table != NULL && "bx_hmap: allocation failed");                           \
        memset(map->table, 0, table_bytes);                                                   \
        map->meta = (bx_hmap_meta*)bx_alloc(meta_bytes);                                      \
        assert(map->meta != NULL && "bx_hmap: allocation failed");                            \
        memset(map->meta, 0, meta_bytes);                                                     \
        map->size = 0;                                                                        \
        map->grow_at = (uint32_t)((new_bucket_count * BX_HMAP_LOAD_NUM) / BX_HMAP_LOAD_DEN);  \
                                                                                              \
        /* Guard slot past the last bucket: stops a probe run walking off the end */          \
        map->meta[new_bucket_count].dist = BX_HMAP_DIST_MASK;                                 \
                                                                                              \
        for (uint32_t i = 0; i < old.bucket_count; i++)                                       \
        {                                                                                     \
            if (old.meta[i].dist > 0)                                                         \
            {                                                                                 \
                bx_hmap_##NAME##_insert(map, old.table[i].key, old.table[i].value);           \
            }                                                                                 \
        }                                                                                     \
                                                                                              \
        bx_free(old.table);                                                                   \
        bx_free(old.meta);                                                                    \
    }                                                                                         \
                                                                                              \
    /* `capacity` counts elements, not buckets. Converting needs a ceiling: floor             \
       would hand back a table that is already over the load factor and rehashes on           \
       the very inserts the caller just reserved for. */                                      \
    static inline void bx_hmap_##NAME##_reserve(bx_hmap_##NAME* map, uint32_t capacity)       \
    {                                                                                         \
        uint64_t needed = ((uint64_t)capacity * BX_HMAP_LOAD_DEN + (BX_HMAP_LOAD_NUM - 1)) /  \
                          BX_HMAP_LOAD_NUM;                                                   \
        bx_hmap_##NAME##_resize(map, needed);                                                 \
    }                                                                                         \
                                                                                              \
    static inline void bx_hmap_##NAME##_init_capacity(bx_hmap_##NAME* map, uint32_t capacity) \
    {                                                                                         \
        bx_hmap_##NAME##_init(map);                                                           \
        bx_hmap_##NAME##_reserve(map, capacity);                                              \
    }                                                                                         \
                                                                                              \
    static inline void bx_hmap_##NAME##_insert(bx_hmap_##NAME* map, K key, V value)           \
    {                                                                                         \
        if (map->size >= map->grow_at)                                                        \
        {                                                                                     \
            bx_hmap_##NAME##_resize(map, (uint64_t)map->bucket_count* BX_HMAP_GROWTH);        \
        }                                                                                     \
                                                                                              \
        uint64_t hash;                                                                        \
        bool found;                                                                           \
        uint32_t index = bx_hmap_##NAME##_find_bucket(map, key, &hash, &found);               \
        if (found)                                                                            \
        {                                                                                     \
            map->table[index].value = value;                                                  \
            return;                                                                           \
        }                                                                                     \
                                                                                              \
        uint32_t mask = map->bucket_count - 1;                                                \
        bx_hmap_meta new_meta;                                                                \
        new_meta.mini_hash = BX_HMAP_MINI_HASH(hash);                                         \
        new_meta.dist =                                                                       \
            (uint16_t)((((index - (uint32_t)(hash & mask)) + map->bucket_count) & mask) + 1); \
                                                                                              \
        /* The displaced entry rides in a local rather than heap scratch, which caps          \
           usable entry size at what the stack will hold. */                                  \
        bx_hmap_##NAME##_entry carried;                                                       \
        carried.key = key;                                                                    \
        carried.value = value;                                                                \
                                                                                              \
        while (map->meta[index].dist != 0)                                                    \
        {                                                                                     \
            /* Robin Hood: evict any resident sitting closer to home than we are and          \
               carry it onward, which spreads displacement and bounds probe length. */        \
            if (map->meta[index].dist < new_meta.dist)                                        \
            {                                                                                 \
                bx_hmap_meta resident_meta = map->meta[index];                                \
                map->meta[index] = new_meta;                                                  \
                new_meta = resident_meta;                                                     \
                                                                                              \
                bx_hmap_##NAME##_entry resident = map->table[index];                          \
                map->table[index] = carried;                                                  \
                carried = resident;                                                           \
            }                                                                                 \
            index = (index + 1) & mask;                                                       \
            new_meta.dist++;                                                                  \
        }                                                                                     \
                                                                                              \
        map->meta[index] = new_meta;                                                          \
        map->table[index] = carried;                                                          \
        map->size++;                                                                          \
    }                                                                                         \
                                                                                              \
    static inline void bx_hmap_##NAME##_erase(bx_hmap_##NAME* map, K key)                     \
    {                                                                                         \
        uint64_t hash;                                                                        \
        bool found;                                                                           \
        uint32_t index = bx_hmap_##NAME##_find_bucket(map, key, &hash, &found);               \
        if (!found)                                                                           \
        {                                                                                     \
            return;                                                                           \
        }                                                                                     \
                                                                                              \
        uint32_t mask = map->bucket_count - 1;                                                \
        uint32_t hole = index, probe = hole;                                                  \
                                                                                              \
        /* Backward-shift deletion: drag each displaced follower one slot closer to           \
           home, stopping at the first already there (dist < 2). No tombstones, so            \
           lookups never probe past a dead slot. */                                           \
        for (;;)                                                                              \
        {                                                                                     \
            probe = (probe + 1) & mask;                                                       \
            if (map->meta[probe].dist < 2)                                                    \
            {                                                                                 \
                break;                                                                        \
            }                                                                                 \
            map->table[hole] = map->table[probe];                                             \
            map->meta[hole] = map->meta[probe];                                               \
            map->meta[hole].dist--;                                                           \
            hole = probe;                                                                     \
        }                                                                                     \
        map->meta[hole].dist = 0;                                                             \
        map->size--;                                                                          \
    }                                                                                         \
                                                                                              \
    static inline V* bx_hmap_##NAME##_get(bx_hmap_##NAME* map, K key)                         \
    {                                                                                         \
        uint64_t hash;                                                                        \
        bool found;                                                                           \
        uint32_t index = bx_hmap_##NAME##_find_bucket(map, key, &hash, &found);               \
        return found ? &map->table[index].value : NULL;                                       \
    }                                                                                         \
                                                                                              \
    static inline bool bx_hmap_##NAME##_contains(bx_hmap_##NAME* map, K key)                  \
    {                                                                                         \
        return bx_hmap_##NAME##_get(map, key) != NULL;                                        \
    }
