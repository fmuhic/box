#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "box/darray_core.h"

#define BX_SPSET_PAGE_SHIFT    10
#define BX_SPSET_PAGE_SIZE     (1 << BX_SPSET_PAGE_SHIFT)
#define BX_SPSET_PAGE_MASK     (BX_SPSET_PAGE_SIZE - 1)
#define BX_SPSET_INVALID_INDEX UINT32_MAX

typedef struct bx_spset
{
    bx_darray dense;   /* Packed values, iterated linearly */
    bx_darray ids;     /* dense index -> ID, parallel to dense */
    uint32_t** sparse; /* Paged directory, ID -> dense index */
    uint32_t page_count;
} bx_spset;

void bx_spset_init(bx_spset* set, size_t elem_size);
void bx_spset_init_capacity(bx_spset* set, size_t elem_size, uint32_t capacity);
void bx_spset_drop(bx_spset* set);
/* Assumes IDs live in [0, capacity): sizes dense exactly and allocates the
   sparse pages spanning that range, so inserts within it never allocate.
   IDs outside it still work, falling back to lazy growth. */
void bx_spset_reserve(bx_spset* set, uint32_t capacity);

/* Grows the page directory only. Pages stay lazy because an ID range can be
   wide and sparsely populated. */
void bx_spset_reserve_ids(bx_spset* set, uint32_t max_id);
uint32_t bx_spset_find(const bx_spset* set, uint32_t id);
uint32_t bx_spset_insert_slot(bx_spset* set, uint32_t id, bool* out_is_new);
void bx_spset_erase(bx_spset* set, uint32_t id);
void bx_spset_clear(bx_spset* set);

static inline uint32_t bx_spset_size(const bx_spset* set)
{
    return set->dense.size;
}

static inline uint32_t bx_spset_capacity(const bx_spset* set)
{
    return set->dense.capacity;
}

static inline bool bx_spset_contains(const bx_spset* set, uint32_t id)
{
    return bx_spset_find(set, id) != BX_SPSET_INVALID_INDEX;
}
