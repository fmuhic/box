#include "bench.h"

#include "box/hmap.h"
#include "box/spset.h"

// A sparse set is a specialised uint32 -> T map, so the honest comparison is
// against general-purpose maps carrying the same workload. The tradeoff is
// memory: spset's sparse array is indexed by ID, so it pays for the ID range
// rather than for the element count.
BX_SPSET_DECLARE(float, bench)
BX_HMAP_DECLARE(uint32_t, float, sp, bx_bench_hash_u32, bx_bench_eq_u32)

static double box_insert(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_spset_bench s;
    bx_spset_bench_init(&s);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        bx_spset_bench_insert(&s, keys[i], (float)i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(bx_spset_bench_size(&s));
    bx_spset_bench_drop(&s);
    return t1 - t0;
}

static double box_lookup_hit(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_spset_bench s;
    bx_spset_bench_init(&s);
    for (uint32_t i = 0; i < n; i++)
    {
        bx_spset_bench_insert(&s, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        float* v = bx_spset_bench_get(&s, keys[i]);
        BX_BENCH_SINK(v);
    }
    double t1 = bx_bench_now();

    bx_spset_bench_drop(&s);
    return t1 - t0;
}

static double box_lookup_miss(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    bx_spset_bench s;
    bx_spset_bench_init(&s);
    for (uint32_t i = 0; i < n; i++)
    {
        bx_spset_bench_insert(&s, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        float* v = bx_spset_bench_get(&s, misses[i]);
        BX_BENCH_SINK(v);
    }
    double t1 = bx_bench_now();

    bx_spset_bench_drop(&s);
    return t1 - t0;
}

static double box_erase(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_spset_bench s;
    bx_spset_bench_init(&s);
    for (uint32_t i = 0; i < n; i++)
    {
        bx_spset_bench_insert(&s, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        bx_spset_bench_erase(&s, keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(bx_spset_bench_size(&s));
    bx_spset_bench_drop(&s);
    return t1 - t0;
}

// The packed dense array is the whole point of a sparse set: iteration is a
// linear scan with no metadata and no holes.
static double box_iterate(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_spset_bench s;
    bx_spset_bench_init(&s);
    for (uint32_t i = 0; i < n; i++)
    {
        bx_spset_bench_insert(&s, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    float* data = bx_spset_bench_data(&s);
    double sum = 0.0;
    uint32_t count = bx_spset_bench_size(&s);
    for (uint32_t i = 0; i < count; i++)
    {
        sum += data[i];
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(sum);
    bx_spset_bench_drop(&s);
    return t1 - t0;
}

// ---------------------------------------------------------------------------
// box hmap carrying the same uint32 -> float workload
// ---------------------------------------------------------------------------

static double hmap_insert(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_hmap_sp m;
    bx_hmap_sp_init(&m);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        bx_hmap_sp_insert(&m, keys[i], (float)i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(bx_hmap_sp_size(&m));
    bx_hmap_sp_drop(&m);
    return t1 - t0;
}

static double hmap_lookup_hit(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_hmap_sp m;
    bx_hmap_sp_init(&m);
    for (uint32_t i = 0; i < n; i++)
    {
        bx_hmap_sp_insert(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        float* v = bx_hmap_sp_get(&m, keys[i]);
        BX_BENCH_SINK(v);
    }
    double t1 = bx_bench_now();

    bx_hmap_sp_drop(&m);
    return t1 - t0;
}

static double hmap_lookup_miss(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    bx_hmap_sp m;
    bx_hmap_sp_init(&m);
    for (uint32_t i = 0; i < n; i++)
    {
        bx_hmap_sp_insert(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        float* v = bx_hmap_sp_get(&m, misses[i]);
        BX_BENCH_SINK(v);
    }
    double t1 = bx_bench_now();

    bx_hmap_sp_drop(&m);
    return t1 - t0;
}

static double hmap_erase(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_hmap_sp m;
    bx_hmap_sp_init(&m);
    for (uint32_t i = 0; i < n; i++)
    {
        bx_hmap_sp_insert(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        bx_hmap_sp_erase(&m, keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(bx_hmap_sp_size(&m));
    bx_hmap_sp_drop(&m);
    return t1 - t0;
}

void bx_bench_register_spset(void)
{
    bx_bench_add("spset", "box", "insert", box_insert);
    bx_bench_add("spset", "box_hmap", "insert", hmap_insert);

    bx_bench_add("spset", "box", "lookup_hit", box_lookup_hit);
    bx_bench_add("spset", "box_hmap", "lookup_hit", hmap_lookup_hit);

    bx_bench_add("spset", "box", "lookup_miss", box_lookup_miss);
    bx_bench_add("spset", "box_hmap", "lookup_miss", hmap_lookup_miss);

    bx_bench_add("spset", "box", "erase", box_erase);
    bx_bench_add("spset", "box_hmap", "erase", hmap_erase);

    bx_bench_add("spset", "box", "iterate", box_iterate);
}
