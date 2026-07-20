#include "bench.h"

#include "box/bitset.h"

#include <stdlib.h>
#include <string.h>

// There is no third-party C bitset worth vendoring, so the comparison is
// against a hand-rolled uint64 block array: the code you would otherwise write
// inline. Anything box adds over this is the cost of the abstraction.

static double box_set(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_bitset s;
    bx_bitset_init_capacity(&s, n);
    bx_bitset_set_count_and_clear(&s, n);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        bx_bitset_set_fast(&s, keys[i] % n);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(bx_bitset_popcount(&s));
    bx_bitset_drop(&s);
    return t1 - t0;
}

static double box_get(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_bitset s;
    bx_bitset_init_capacity(&s, n);
    bx_bitset_set_count_and_clear(&s, n);
    for (uint32_t i = 0; i < n; i += 2)
    {
        bx_bitset_set_fast(&s, i);
    }

    double t0 = bx_bench_now();
    uint32_t hits = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        hits += bx_bitset_get(&s, keys[i] % n) ? 1u : 0u;
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(hits);
    bx_bitset_drop(&s);
    return t1 - t0;
}

static double box_popcount(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_bitset s;
    bx_bitset_init_capacity(&s, n);
    bx_bitset_set_count_and_clear(&s, n);
    for (uint32_t i = 0; i < n; i += 3)
    {
        bx_bitset_set_fast(&s, i);
    }

    // One popcount is a whole-set scan, so it is already O(n); repeating it n
    // times would measure minutes. Amortise a single pass over n for a
    // per-element figure comparable to the other rows.
    double t0 = bx_bench_now();
    uint32_t total = bx_bitset_popcount(&s);
    double t1 = bx_bench_now();

    BX_BENCH_SINK(total);
    bx_bitset_drop(&s);
    return t1 - t0;
}

static double box_union(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_bitset a, b;
    bx_bitset_init_capacity(&a, n);
    bx_bitset_set_count_and_clear(&a, n);
    bx_bitset_init_capacity(&b, n);
    bx_bitset_set_count_and_clear(&b, n);
    for (uint32_t i = 0; i < n; i += 2)
    {
        bx_bitset_set_fast(&a, i);
    }
    for (uint32_t i = 1; i < n; i += 2)
    {
        bx_bitset_set_fast(&b, keys[i] % n);
    }

    double t0 = bx_bench_now();
    bx_bitset_union(&a, &b);
    double t1 = bx_bench_now();

    BX_BENCH_SINK(bx_bitset_popcount(&a));
    bx_bitset_drop(&a);
    bx_bitset_drop(&b);
    return t1 - t0;
}

// ---------------------------------------------------------------------------
// Hand-rolled uint64 block array
// ---------------------------------------------------------------------------

static double raw_set(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    uint32_t blocks = (n + 63) / 64;
    uint64_t* bits = (uint64_t*)calloc(blocks, sizeof(uint64_t));

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t bit = keys[i] % n;
        bits[bit >> 6] |= (uint64_t)1 << (bit & 63);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(bits[0]);
    free(bits);
    return t1 - t0;
}

static double raw_get(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    uint32_t blocks = (n + 63) / 64;
    uint64_t* bits = (uint64_t*)calloc(blocks, sizeof(uint64_t));
    for (uint32_t i = 0; i < n; i += 2)
    {
        bits[i >> 6] |= (uint64_t)1 << (i & 63);
    }

    double t0 = bx_bench_now();
    uint32_t hits = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t bit = keys[i] % n;
        hits += (bits[bit >> 6] >> (bit & 63)) & 1u;
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(hits);
    free(bits);
    return t1 - t0;
}

void bx_bench_register_bitset(void)
{
    bx_bench_add("bitset", "box", "set", box_set);
    bx_bench_add("bitset", "raw_u64", "set", raw_set);

    bx_bench_add("bitset", "box", "get", box_get);
    bx_bench_add("bitset", "raw_u64", "get", raw_get);

    bx_bench_add("bitset", "box", "popcount_scan", box_popcount);
    bx_bench_add("bitset", "box", "union_scan", box_union);
}
