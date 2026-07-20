#include "bench.h"

#ifdef BOX_PERF_HAS_STC

// STC's hmap is Robin Hood too, which is the point of including it: it is the
// only comparison here that shares box's probing scheme. Where it differs is
// the tuning -- 1 byte of metadata per bucket against box's 2, a 0.85 max load
// against 0.80, and 1.5x growth against 2x. A gap against STC is a gap in the
// implementation; a gap they share is a property of Robin Hood.

// STC hands the hash and equality callbacks a pointer to the key.
static size_t stc_hash_u32(const uint32_t* k)
{
    return (size_t)bx_bench_hash_u32(*k);
}

static bool stc_eq_u32(const uint32_t* a, const uint32_t* b)
{
    return *a == *b;
}

#define T      stcmap, uint32_t, float
#define i_hash stc_hash_u32
#define i_eq   stc_eq_u32
#include <stc/hmap.h>

static double stc_insert(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    stcmap m = { 0 };

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        stcmap_put(&m, keys[i], (float)i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(stcmap_size(&m));
    stcmap_drop(&m);
    return t1 - t0;
}

static double stc_insert_reserved(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    stcmap m = { 0 };
    stcmap_reserve(&m, (isize_t)n);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        stcmap_put(&m, keys[i], (float)i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(stcmap_size(&m));
    stcmap_drop(&m);
    return t1 - t0;
}

static double stc_lookup_hit(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    stcmap m = { 0 };
    for (uint32_t i = 0; i < n; i++)
    {
        stcmap_put(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        const stcmap_value* v = stcmap_get(&m, keys[i]);
        BX_BENCH_SINK(v);
    }
    double t1 = bx_bench_now();

    stcmap_drop(&m);
    return t1 - t0;
}

static double stc_lookup_miss(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    stcmap m = { 0 };
    for (uint32_t i = 0; i < n; i++)
    {
        stcmap_put(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        const stcmap_value* v = stcmap_get(&m, misses[i]);
        BX_BENCH_SINK(v);
    }
    double t1 = bx_bench_now();

    stcmap_drop(&m);
    return t1 - t0;
}

static double stc_erase(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    stcmap m = { 0 };
    for (uint32_t i = 0; i < n; i++)
    {
        stcmap_put(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        stcmap_erase(&m, keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(stcmap_size(&m));
    stcmap_drop(&m);
    return t1 - t0;
}

static double stc_replace(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    stcmap m = { 0 };
    for (uint32_t i = 0; i < n; i++)
    {
        stcmap_put(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        stcmap_put(&m, keys[i], (float)(i + 1));
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(stcmap_size(&m));
    stcmap_drop(&m);
    return t1 - t0;
}

void bx_bench_register_stc(void)
{
    bx_bench_add("hmap", "stc", "insert", stc_insert);
    bx_bench_add("hmap", "stc", "insert_reserved", stc_insert_reserved);
    bx_bench_add("hmap", "stc", "lookup_hit", stc_lookup_hit);
    bx_bench_add("hmap", "stc", "lookup_miss", stc_lookup_miss);
    bx_bench_add("hmap", "stc", "erase", stc_erase);
    bx_bench_add("hmap", "stc", "replace", stc_replace);
}

#else
void bx_bench_register_stc(void)
{
}
#endif
