#include "box/spset_core.h"

#include <string.h>
#include <assert.h>

#include "box/core.h"

void bx_spset_init(bx_spset* set, size_t elem_size)
{
    bx_darray_init(&set->dense, elem_size);
    bx_darray_init(&set->ids, sizeof(uint32_t));
    set->sparse = NULL;
    set->page_count = 0;
}

void bx_spset_init_capacity(bx_spset* set, size_t elem_size, uint32_t capacity)
{
    bx_spset_init(set, elem_size);
    bx_spset_reserve(set, capacity);
}

void bx_spset_drop(bx_spset* set)
{
    if (set->sparse)
    {
        for (uint32_t i = 0; i < set->page_count; ++i)
        {
            if (set->sparse[i])
            {
                bx_free(set->sparse[i]);
            }
        }
        bx_free(set->sparse);
    }

    bx_darray_drop(&set->dense);
    bx_darray_drop(&set->ids);
    set->sparse = NULL;
    set->page_count = 0;
}

// New directory slots are NULL, which already reads as "page not present",
// so the directory needs no separate size/capacity split.
static void bx_spset_grow_directory(bx_spset* set, uint32_t required_pages)
{
    if (required_pages <= set->page_count)
    {
        return;
    }

    uint32_t** new_sparse = (uint32_t**)bx_alloc(required_pages * sizeof(uint32_t*));
    assert(new_sparse != NULL && "bx_spset: allocation failed");
    memset(new_sparse, 0, required_pages * sizeof(uint32_t*));

    if (set->sparse)
    {
        memcpy(new_sparse, set->sparse, set->page_count * sizeof(uint32_t*));
        bx_free(set->sparse);
    }

    set->sparse = new_sparse;
    set->page_count = required_pages;
}

// 0xFF bytes give UINT32_MAX per entry, which is BX_SPSET_INVALID_INDEX.
static void bx_spset_alloc_page(bx_spset* set, uint32_t page)
{
    set->sparse[page] = (uint32_t*)bx_alloc(BX_SPSET_PAGE_SIZE * sizeof(uint32_t));
    assert(set->sparse[page] != NULL && "bx_spset: allocation failed");
    memset(set->sparse[page], 0xFF, BX_SPSET_PAGE_SIZE * sizeof(uint32_t));
}

void bx_spset_reserve(bx_spset* set, uint32_t capacity)
{
    if (capacity == 0)
    {
        return;
    }

    // Both arrays share one dense index, so they must stay the same length
    bx_darray_reserve(&set->dense, capacity);
    bx_darray_reserve(&set->ids, capacity);

    // Materialize the pages up front so inserts in range never allocate
    uint32_t required_pages = ((capacity - 1) >> BX_SPSET_PAGE_SHIFT) + 1;
    bx_spset_grow_directory(set, required_pages);

    for (uint32_t page = 0; page < required_pages; ++page)
    {
        if (!set->sparse[page])
        {
            bx_spset_alloc_page(set, page);
        }
    }
}

void bx_spset_reserve_ids(bx_spset* set, uint32_t max_id)
{
    bx_spset_grow_directory(set, (max_id >> BX_SPSET_PAGE_SHIFT) + 1);
}

uint32_t bx_spset_find(const bx_spset* set, uint32_t id)
{
    uint32_t page = id >> BX_SPSET_PAGE_SHIFT;
    if (page >= set->page_count || !set->sparse[page])
    {
        return BX_SPSET_INVALID_INDEX;
    }

    // Validating against ids[] is what lets erase and clear leave stale sparse
    // entries behind instead of scrubbing them
    uint32_t dense_index = set->sparse[page][id & BX_SPSET_PAGE_MASK];
    if (dense_index < set->dense.size && ((const uint32_t*)set->ids.data)[dense_index] == id)
    {
        return dense_index;
    }

    return BX_SPSET_INVALID_INDEX;
}

uint32_t bx_spset_insert_slot(bx_spset* set, uint32_t id, bool* out_is_new)
{
    uint32_t existing = bx_spset_find(set, id);
    if (existing != BX_SPSET_INVALID_INDEX)
    {
        *out_is_new = false;
        return existing;
    }

    uint32_t page = id >> BX_SPSET_PAGE_SHIFT;
    if (page >= set->page_count)
    {
        bx_spset_reserve_ids(set, id);
    }

    if (!set->sparse[page])
    {
        bx_spset_alloc_page(set, page);
    }

    // Grow both arrays before bumping either size so they stay in lockstep
    if (set->dense.size >= set->dense.capacity)
    {
        bx_darray_grow(&set->dense);
        bx_darray_reserve(&set->ids, set->dense.capacity);
    }

    assert(set->dense.size < BX_SPSET_INVALID_INDEX && "bx_spset: dense index would collide with the invalid sentinel");
    uint32_t new_index = set->dense.size++;
    set->ids.size++;
    set->sparse[page][id & BX_SPSET_PAGE_MASK] = new_index;
    ((uint32_t*)set->ids.data)[new_index] = id;

    *out_is_new = true;
    return new_index;
}

void bx_spset_erase(bx_spset* set, uint32_t id)
{
    uint32_t hole = bx_spset_find(set, id);
    if (hole == BX_SPSET_INVALID_INDEX)
    {
        return;
    }

    uint32_t* ids = (uint32_t*)set->ids.data;
    uint32_t last_index = set->dense.size - 1;
    uint32_t moved_id = ids[last_index];

    // Swap-and-pop keeps dense packed, at the cost of not preserving order
    memcpy((char*)set->dense.data + hole * set->dense.elem_size,
           (char*)set->dense.data + last_index * set->dense.elem_size,
           set->dense.elem_size);
    ids[hole] = moved_id;

    uint32_t moved_page = moved_id >> BX_SPSET_PAGE_SHIFT;
    set->sparse[moved_page][moved_id & BX_SPSET_PAGE_MASK] = hole;

    set->dense.size--;
    set->ids.size--;
}

void bx_spset_clear(bx_spset* set)
{
    // Sparse pages are deliberately left stale
    bx_darray_clear(&set->dense);
    bx_darray_clear(&set->ids);
}
