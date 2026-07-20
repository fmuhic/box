#pragma once

#include <stdint.h>

// Max load factor as an exact ratio, 4/5 == 0.80. Integer form keeps the grow
// threshold and the element->bucket conversion off the FPU and exactly
// reproducible; absl and khash both apply their load factor the same way.
#define BX_HMAP_LOAD_NUM 4
#define BX_HMAP_LOAD_DEN 5

// Floor on a table that has been allocated at all, so reserve(0) and the first
// insert into an empty map have somewhere to go.
#define BX_HMAP_MIN_BUCKETS 8

// Bucket-space growth per resize. Applied to bucket_count directly: routing it
// through the element-capacity API instead would re-divide by the load factor
// and silently land on 4x after next_pow2 rounding.
#define BX_HMAP_GROWTH 2

#define BX_HMAP_DIST_MASK 0x3ffU

// Shared by every generated probe loop. If two of them ever derive this
// differently, lookups miss silently.
#define BX_HMAP_MINI_HASH(h) ((uint8_t)(((h) >> 24) & 0x3fU))

// For reserve only. It rehashes through insert, so leaving it inlinable puts a
// copy of insert-inside-reserve at every insert call site: 91K vs 25K of .text
// across the hmap tests. Reserve runs once per doubling, so out-of-line is free.
// `inline` is absent on purpose, it contradicts `noinline`; `unused` covers TUs
// that never reserve.
#if defined(__GNUC__) || defined(__clang__)
#define BX_HMAP_COLD __attribute__((noinline, unused)) static
#else
#define BX_HMAP_COLD static inline
#endif

typedef struct bx_hmap_meta
{
    uint16_t mini_hash : 6;
    uint16_t dist : 10;
} bx_hmap_meta;
