#include "box/hmap_core.h"

#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "box/core.h"

static inline void* bx_hmap_entry_at(const bx_hmap* map, uint32_t index)
{
    return (char*)map->table + index * map->vt->entry_size;
}

static inline void* bx_hmap_value_at(const bx_hmap* map, uint32_t index)
{
    return (char*)map->table + index * map->vt->entry_size + map->vt->value_offset;
}

static uint32_t bx_hmap_find_bucket(const bx_hmap* map, const void* key, bool* found)
{
    if (map->bucket_count == 0)
    {
        *found = false;
        return 0;
    }

    const bx_hmap_vtable* vt = map->vt;
    uint64_t hash = vt->hash(key);
    uint32_t mask = map->bucket_count - 1;
    uint32_t index = (uint32_t)(hash & mask);
    uint8_t mini_hash = BX_HMAP_MINI_HASH(hash);
    uint16_t dist = 1;
    *found = false;

    while (dist <= map->meta[index].dist)
    {
        if (map->meta[index].mini_hash == mini_hash && vt->eq(bx_hmap_entry_at(map, index), key))
        {
            *found = true;
            return index;
        }
        index = (index + 1) & mask;
        dist++;
    }
    return index;
}

void* bx_hmap_get(bx_hmap* map, const void* key)
{
    bool found;
    uint32_t index = bx_hmap_find_bucket(map, key, &found);
    return found ? bx_hmap_value_at(map, index) : NULL;
}

void bx_hmap_init(bx_hmap* map, const bx_hmap_vtable* vt)
{
    map->table = NULL;
    map->meta = NULL;
    map->size = 0;
    map->bucket_count = 0;
    map->vt = vt;
}

void bx_hmap_init_capacity(bx_hmap* map, const bx_hmap_vtable* vt, uint32_t capacity)
{
    bx_hmap_init(map, vt);
    bx_hmap_reserve(map, capacity);
}

bool bx_hmap_contains(const bx_hmap* map, const void* key)
{
    bool found;
    bx_hmap_find_bucket(map, key, &found);
    return found;
}

void bx_hmap_clear(bx_hmap* map)
{
    if (map->bucket_count == 0)
    {
        return;
    }

    memset(map->meta, 0, (map->bucket_count + 1) * sizeof(bx_hmap_meta));
    // Reinstall the probe guard
    map->meta[map->bucket_count].dist = BX_HMAP_DIST_MASK;
    map->size = 0;
}

void bx_hmap_drop(bx_hmap* map)
{
    bx_free(map->table);
    bx_free(map->meta);
    map->table = NULL;
    map->meta = NULL;
    map->size = 0;
    map->bucket_count = 0;
}

void bx_hmap_reserve(bx_hmap* map, uint32_t capacity)
{
    const bx_hmap_vtable* vt = map->vt;
    size_t wanted_buckets = bx_next_pow2((size_t)((float)capacity / BX_HMAP_MAX_LOAD_FACTOR) + 4);
    assert(wanted_buckets <= UINT32_MAX && "bx_hmap: bucket count exceeds uint32_t");
    uint32_t new_bucket_count = (uint32_t)wanted_buckets;
    if (new_bucket_count <= map->bucket_count)
    {
        return;
    }

    bx_hmap old = *map;
    map->bucket_count = new_bucket_count;

    // Two extra entries past the buckets: scratch for the insert swap
    uint32_t table_slots = new_bucket_count + BX_HMAP_SCRATCH_SLOTS;
    map->table = bx_alloc(table_slots * vt->entry_size);
    assert(map->table != NULL && "bx_hmap: allocation failed");
    memset(map->table, 0, table_slots * vt->entry_size);
    map->meta = bx_alloc((new_bucket_count + 1) * sizeof(bx_hmap_meta));
    assert(map->meta != NULL && "bx_hmap: allocation failed");
    memset(map->meta, 0, (new_bucket_count + 1) * sizeof(bx_hmap_meta));
    map->size = 0;

    // Guard slot past the last bucket: stops a probe run from walking off the end
    map->meta[new_bucket_count].dist = BX_HMAP_DIST_MASK;

    for (uint32_t i = 0; i < old.bucket_count; i++)
    {
        if (old.meta[i].dist > 0)
        {
            void* old_key = (char*)old.table + i * vt->entry_size;
            void* old_value = (char*)old.table + i * vt->entry_size + vt->value_offset;
            bx_hmap_insert(map, old_key, old_value);
        }
    }

    bx_free(old.table);
    bx_free(old.meta);
}

void bx_hmap_insert(bx_hmap* map, const void* key, const void* value)
{
    const bx_hmap_vtable* vt = map->vt;
    if (map->size >= (uint32_t)((float)map->bucket_count * BX_HMAP_MAX_LOAD_FACTOR))
    {
        bx_hmap_reserve(map, map->size * 2 + 2);
    }
    bool found;
    uint32_t index = bx_hmap_find_bucket(map, key, &found);
    if (found)
    {
        memcpy(bx_hmap_value_at(map, index), value, vt->value_size);
        return;
    }
    uint64_t hash = vt->hash(key);
    uint32_t mask = map->bucket_count - 1;

    bx_hmap_meta new_meta = {
        .mini_hash = BX_HMAP_MINI_HASH(hash),
        .dist = (uint16_t)(((index - (hash & mask) + map->bucket_count) & mask)) + 1
    };

    // Scratch is the tail of the table allocation.
    // It's used to swap elements
    char* scratch_base = (char*)map->table + (size_t)map->bucket_count * vt->entry_size;
    void* carried = scratch_base;
    void* scratch = scratch_base + vt->entry_size;
    memcpy(carried, key, vt->key_size);
    memcpy((char*)carried + vt->value_offset, value, vt->value_size);

    while (map->meta[index].dist != 0)
    {
        // Robin Hood: a resident closer to its ideal bucket than we are gets
        // evicted, and we carry it onward instead. Bounds the worst-case probe
        // length by keeping displacement evenly spread.
        if (map->meta[index].dist < new_meta.dist)
        {
            bx_hmap_meta resident_meta = map->meta[index];
            map->meta[index] = new_meta;
            new_meta = resident_meta;

            void* slot = bx_hmap_entry_at(map, index);
            memcpy(scratch, slot, vt->entry_size);
            memcpy(slot, carried, vt->entry_size);
            memcpy(carried, scratch, vt->entry_size);
        }
        index = (index + 1) & mask;
        new_meta.dist++;
    }

    map->meta[index] = new_meta;
    memcpy(bx_hmap_entry_at(map, index), carried, vt->entry_size);
    map->size++;
}

void bx_hmap_erase(bx_hmap* map, const void* key)
{
    const bx_hmap_vtable* vt = map->vt;
    bool found;
    uint32_t index = bx_hmap_find_bucket(map, key, &found);
    if (!found)
    {
        return;
    }

    uint32_t mask = map->bucket_count - 1;
    uint32_t hole = index, probe = hole;

    // Backward-shift deletion: drag each displaced follower one slot closer to
    // its ideal bucket, stopping at the first entry already home (dist < 2).
    // Leaves no tombstones, so lookups never probe past a dead slot.
    for (;;)
    {
        probe = (probe + 1) & mask;
        if (map->meta[probe].dist < 2)
        {
            break;
        }
        memcpy(bx_hmap_entry_at(map, hole), bx_hmap_entry_at(map, probe), vt->entry_size);
        map->meta[hole] = map->meta[probe];
        map->meta[hole].dist--;
        hole = probe;
    }
    map->meta[hole].dist = 0;
    map->size--;
}
