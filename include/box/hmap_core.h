#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define BX_HMAP_MAX_LOAD_FACTOR 0.80f
#define BX_HMAP_DIST_MASK       0x3ffU
#define BX_HMAP_MAX_ENTRY_SIZE  256

typedef struct bx_hmap_meta
{
    uint16_t mini_hash : 6;
    uint16_t dist : 10;
} bx_hmap_meta;

typedef struct bx_hmap_vtable
{
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
    size_t size;
    size_t bucket_count;
    const bx_hmap_vtable* vt;
} bx_hmap;

void bx_hmap_init(bx_hmap* map, const bx_hmap_vtable* vt);
void bx_hmap_drop(bx_hmap* map);
void bx_hmap_reserve(bx_hmap* map, size_t capacity);
void bx_hmap_insert(bx_hmap* map, const void* key, const void* value);
void bx_hmap_erase(bx_hmap* map, const void* key);
void* bx_hmap_get(bx_hmap* map, const void* key);
