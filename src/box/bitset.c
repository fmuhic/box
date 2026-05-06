#include <string.h>
#include "box/core.h"
#include "box/bitset.h"

void bx_bitset_init(bx_bitset* set, uint32_t bit_capacity)
{
    set->block_capacity = (bit_capacity + 63) / 64;
    set->block_count = 0;
    
    size_t bytes = set->block_capacity * sizeof(uint64_t);
    set->bits = (uint64_t*)bx_alloc(bytes);
    memset(set->bits, 0, bytes);
}

void bx_bitset_drop(bx_bitset* set)
{
    if (set->bits)
    {
        bx_free(set->bits);
    }
    
    set->bits = nullptr;
    set->block_capacity = 0;
    set->block_count = 0;
}

void bx_bitset_set_count_and_clear(bx_bitset* set, uint32_t bit_count)
{
    uint32_t block_count = (bit_count + 63) / 64;
    if (set->block_capacity < block_count)
    {
        bx_bitset_drop(set);
        
        // 1.5x growth
        uint32_t new_bit_capacity = bit_count + (bit_count >> 1);
        bx_bitset_init(set, new_bit_capacity);
    }

    set->block_count = block_count;
    memset(set->bits, 0, set->block_count * sizeof(uint64_t));
}

void bx_bitset_grow(bx_bitset* set, uint32_t block_count)
{
    assert(block_count > set->block_count);
    
    if (block_count > set->block_capacity)
    {
        uint32_t old_capacity = set->block_capacity;
        set->block_capacity = block_count + (block_count >> 1);
        
        size_t new_bytes = set->block_capacity * sizeof(uint64_t);
        uint64_t* new_bits = (uint64_t*)bx_alloc(new_bytes);
        memset(new_bits, 0, new_bytes);
        
        if (set->bits)
        {
            memcpy(new_bits, set->bits, old_capacity * sizeof(uint64_t));
            bx_free(set->bits);
        }
        
        set->bits = new_bits;
    }

    set->block_count = block_count;
}

void bx_bitset_union(bx_bitset* restrict set_a, const bx_bitset* restrict set_b)
{
    assert(set_a->block_count == set_b->block_count);
    for (uint32_t i = 0; i < set_a->block_count; ++i)
    {
        set_a->bits[i] |= set_b->bits[i];
    }
}

int bx_bitset_count(const bx_bitset* set)
{
    int count = 0;
    for (uint32_t i = 0; i < set->block_count; ++i)
    {
        count += bx_pop_count_64(set->bits[i]);
    }
    return count;
}
