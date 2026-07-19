#include "box/hmap_core.h"

#include <string.h>
#include <assert.h>

#include "box/core.h"

static inline void* bx_hmap_entry_at(const bx_hmap* map, size_t index)
{
    return (char*)map->table + index * map->vt->entry_size;
}

static inline void* bx_hmap_key_at(const bx_hmap* map, size_t index)
{
    return (char*)map->table + index * map->vt->entry_size;
}

static inline void* bx_hmap_value_at(const bx_hmap* map, size_t index)
{
    return (char*)map->table + index * map->vt->entry_size + map->vt->value_offset;
}

static size_t bx_hmap_find_bucket(const bx_hmap* map, const void* key, bool* found)
{
    if (map->bucket_count == 0)
    {
        *found = false;
        return 0;
    }

    const bx_hmap_vtable* vt = map->vt;
    uint64_t hash = vt->hash(key);
    size_t mask = map->bucket_count - 1;
    size_t index = hash & mask;
    uint8_t mini_hash = (uint8_t)((hash >> 24) & 0x3fU);
    uint16_t dist = 1;
    *found = false;

    while (dist <= map->meta[index].dist)
    {
        if (map->meta[index].mini_hash == mini_hash && vt->eq(bx_hmap_key_at(map, index), key))
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
    size_t index = bx_hmap_find_bucket(map, key, &found);
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

void bx_hmap_drop(bx_hmap* map)
{
    bx_free(map->table);
    bx_free(map->meta);
    map->table = NULL;
    map->meta = NULL;
    map->size = 0;
    map->bucket_count = 0;
}

void bx_hmap_reserve(bx_hmap* map, size_t capacity)
{
    const bx_hmap_vtable* vt = map->vt;
    size_t new_bucks = (size_t)((float)capacity / BX_HMAP_MAX_LOAD_FACTOR) + 4;
    new_bucks = bx_next_pow2(new_bucks);
    if (new_bucks <= map->bucket_count)
    {
        return;
    }

    bx_hmap old = *map;
    map->bucket_count = new_bucks;
    map->table = bx_alloc(new_bucks * vt->entry_size);
    memset(map->table, 0, new_bucks * vt->entry_size);
    map->meta = bx_alloc((new_bucks + 1) * sizeof(bx_hmap_meta));
    memset(map->meta, 0, (new_bucks + 1) * sizeof(bx_hmap_meta));
    map->size = 0;

    map->meta[new_bucks].dist = BX_HMAP_DIST_MASK;
    for (size_t i = 0; i < old.bucket_count; i++)
    {
        if (old.meta[i].dist > 0)
        {
            void* k = (char*)old.table + i * vt->entry_size;
            void* v = (char*)old.table + i * vt->entry_size + vt->value_offset;
            bx_hmap_insert(map, k, v);
        }
    }

    bx_free(old.table);
    bx_free(old.meta);
}

void bx_hmap_insert(bx_hmap* map, const void* key, const void* value)
{
    const bx_hmap_vtable* vt = map->vt;
    if (map->size >= (size_t)((float)map->bucket_count * BX_HMAP_MAX_LOAD_FACTOR))
    {
        bx_hmap_reserve(map, map->size * 2 + 2);
    }
    bool found;
    size_t index = bx_hmap_find_bucket(map, key, &found);
    if (found)
    {
        memcpy(bx_hmap_value_at(map, index), value, vt->value_size);
        return;
    }
    uint64_t hash = vt->hash(key);
    size_t mask = map->bucket_count - 1;

    bx_hmap_meta new_meta = {
        .mini_hash = (uint16_t)((hash >> 24) & 0x3fU),
        .dist = (uint16_t)(((index - (hash & mask) + map->bucket_count) & mask)) + 1
    };

    assert(vt->entry_size <= BX_HMAP_MAX_ENTRY_SIZE && "bx_hmap: entry exceeds BX_HMAP_MAX_ENTRY_SIZE");
    char cur[BX_HMAP_MAX_ENTRY_SIZE];
    char tmp[BX_HMAP_MAX_ENTRY_SIZE];
    memcpy(cur, key, vt->key_size);
    memcpy(cur + vt->value_offset, value, vt->value_size);

    while (map->meta[index].dist != 0)
    {
        if (map->meta[index].dist < new_meta.dist)
        {
            bx_hmap_meta resident_meta = map->meta[index];
            map->meta[index] = new_meta;
            new_meta = resident_meta;

            void* slot = bx_hmap_entry_at(map, index);
            memcpy(tmp, slot, vt->entry_size);
            memcpy(slot, cur, vt->entry_size);
            memcpy(cur, tmp, vt->entry_size);
        }
        index = (index + 1) & mask;
        new_meta.dist++;
    }

    map->meta[index] = new_meta;
    memcpy(bx_hmap_entry_at(map, index), cur, vt->entry_size);
    map->size++;
}

void bx_hmap_erase(bx_hmap* map, const void* key)
{
    const bx_hmap_vtable* vt = map->vt;
    bool found;
    size_t index = bx_hmap_find_bucket(map, key, &found);
    if (!found)
    {
        return;
    }

    size_t mask = map->bucket_count - 1;
    size_t i = index, j = i;
    for (;;)
    {
        j = (j + 1) & mask;
        if (map->meta[j].dist < 2)
        {
            break;
        }
        memcpy(bx_hmap_entry_at(map, i), bx_hmap_entry_at(map, j), vt->entry_size);
        map->meta[i] = map->meta[j];
        map->meta[i].dist--;
        i = j;
    }
    map->meta[i].dist = 0;
    map->size--;
}
