#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include "box/core.h"

typedef struct bx_bitset
{
    uint64_t* bits;
    uint32_t block_capacity;
    uint32_t block_count;
} bx_bitset;

/* Allocation-free, like every other container's init: a zeroed bx_bitset is a
   valid empty bitset. The first set_safe / set_count_and_clear allocates. */
void bx_bitset_init(bx_bitset* set);
void bx_bitset_init_capacity(bx_bitset* set, uint32_t bit_capacity);
void bx_bitset_drop(bx_bitset* set);
void bx_bitset_set_count_and_clear(bx_bitset* set, uint32_t bit_count);
void bx_bitset_grow_blocks(bx_bitset* set, uint32_t block_count);
void bx_bitset_union(bx_bitset* set_a, const bx_bitset* set_b);

/* Number of bits currently set, not the number of bits addressable. */
uint32_t bx_bitset_popcount(const bx_bitset* set);

static inline void bx_bitset_set_fast(bx_bitset* set, uint32_t bit_index)
{
    uint32_t block_index = bit_index / 64;
    assert(block_index < set->block_count);
    set->bits[block_index] |= ((uint64_t)1 << (bit_index % 64));
}

static inline void bx_bitset_set_safe(bx_bitset* set, uint32_t bit_index)
{
    uint32_t block_index = bit_index / 64;
    if (block_index >= set->block_count)
    {
        bx_bitset_grow_blocks(set, block_index + 1);
    }
    set->bits[block_index] |= ((uint64_t)1 << (bit_index % 64));
}

static inline void bx_bitset_unset(bx_bitset* set, uint32_t bit_index)
{
    uint32_t block_index = bit_index / 64;
    if (block_index >= set->block_count)
    {
        return;
    }
    set->bits[block_index] &= ~((uint64_t)1 << (bit_index % 64));
}

static inline bool bx_bitset_get(const bx_bitset* set, uint32_t bit_index)
{
    uint32_t block_index = bit_index / 64;
    if (block_index >= set->block_count)
    {
        return false;
    }
    return (set->bits[block_index] & ((uint64_t)1 << (bit_index % 64))) != 0;
}
