#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define BX_HMAP_MAX_LOAD_FACTOR 0.80f
#define BX_HMAP_DIST_MASK       0x3ffU
#define BX_HMAP_MAX_ENTRY_SIZE  256

/* Shared by the core probe loop and the devirtualized _get in hmap.h. If the
   two ever derive this differently, lookups miss silently. */
#define BX_HMAP_MINI_HASH(h) ((uint8_t)(((h) >> 24) & 0x3fU))

typedef struct bx_hmap_meta
{
    uint16_t mini_hash : 6;
    uint16_t dist : 10;
} bx_hmap_meta;

typedef struct bx_hmap_vtable
{
    /* size_t on purpose: keeps every `count * entry_size` 64-bit so a byte
       count can never overflow into a short alloc. */
    size_t entry_size;
    size_t key_size;
    size_t value_offset;
    size_t value_size;
    uint64_t (*hash)(const void* key);
    bool (*eq)(const void* a, const void* b);
} bx_hmap_vtable;

typedef struct bx_hmap
{
    void* table;
    bx_hmap_meta* meta;
    uint32_t size;
    uint32_t bucket_count;
    const bx_hmap_vtable* vt;
} bx_hmap;

void bx_hmap_init(bx_hmap* map, const bx_hmap_vtable* vt);

/* `capacity` counts elements, not buckets: the bucket count is derived by
   dividing through BX_HMAP_MAX_LOAD_FACTOR and rounding up to a power of two,
   so init_capacity(map, vt, 100) allocates 256 buckets. */
void bx_hmap_init_capacity(bx_hmap* map, const bx_hmap_vtable* vt, uint32_t capacity);
void bx_hmap_drop(bx_hmap* map);
void bx_hmap_reserve(bx_hmap* map, uint32_t capacity);

/* Empties the map but keeps the bucket allocation for reuse. */
void bx_hmap_clear(bx_hmap* map);
void bx_hmap_insert(bx_hmap* map, const void* key, const void* value);
void bx_hmap_erase(bx_hmap* map, const void* key);
void* bx_hmap_get(bx_hmap* map, const void* key);
bool bx_hmap_contains(const bx_hmap* map, const void* key);
