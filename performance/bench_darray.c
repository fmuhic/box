#include "bench.h"

#include "box/darray.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

BX_DARRAY_DECLARE(uint32_t, bench)

static double box_push(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_darray_bench a;
    bx_darray_bench_init(&a);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        bx_darray_bench_push(&a, keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(bx_darray_bench_size(&a));
    bx_darray_bench_drop(&a);
    return t1 - t0;
}

static double box_push_reserved(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_darray_bench a;
    bx_darray_bench_init(&a);
    bx_darray_bench_reserve(&a, n);
    // Page the buffer in before timing. reserve() only maps the pages; their
    // first write faults them one by one, and left inside the loop that fault-in
    // is most of what this row measures -- it swamped the stores and made the
    // fresh mmap's cost, not the push, the variable. This row is meant to be the
    // no-growth store cost, so the faults belong outside the timed region.
    memset(a.base.data, 0, (size_t)n * sizeof(uint32_t));

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        bx_darray_bench_push(&a, keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(bx_darray_bench_size(&a));
    bx_darray_bench_drop(&a);
    return t1 - t0;
}

// Steady-state push: one buffer, reserved and paged in once, then cleared and
// refilled every rep. This is the game-loop pattern -- a darray kept across
// frames and cleared, not reallocated -- and it is the pure push throughput:
// no growth, no allocation, no first-touch faults inside the timed region.
//
// The `push` row above still measures the cold build (grow from empty, faults
// and all); this isolates the store itself. Reused across reps, the buffer's
// pages are hot after the first warmup pass, so the per-run variance that
// allocation and paging inflict on the cold rows drops away.
//
// The buffer is deliberately `static` so it survives between the harness's
// per-rep calls; it is never dropped, so the process leaks one buffer at exit.
// That is fine for a benchmark and keeps the fast path free of a per-rep init.
static double box_push_warm(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    static bx_darray_bench a;
    static uint32_t provisioned = 0;
    if (provisioned < n)
    {
        if (provisioned > 0)
        {
            bx_darray_bench_drop(&a);
        }
        bx_darray_bench_init(&a);
        bx_darray_bench_reserve(&a, n);
        memset(a.base.data, 0, (size_t)n * sizeof(uint32_t));
        provisioned = n;
    }
    bx_darray_bench_clear(&a); // keeps the capacity and the resident pages

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        bx_darray_bench_push(&a, keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(bx_darray_bench_size(&a));
    return t1 - t0;
}

// Sequential read. Mostly a memory-bandwidth measurement; it exists to confirm
// the accessor does not get in the way of the prefetcher.
static double box_iterate(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_darray_bench a;
    // Pre-reserved to match box2d's b2Array_CreateN -- otherwise the two sides
    // differ in buffer provenance and this compares setup, not the timed op.
    bx_darray_bench_init_capacity(&a, n);
    for (uint32_t i = 0; i < n; i++)
    {
        bx_darray_bench_push(&a, keys[i]);
    }

    double t0 = bx_bench_now();
    uint64_t sum = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        sum += *bx_darray_bench_get(&a, i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(sum);
    bx_darray_bench_drop(&a);
    return t1 - t0;
}

// Random access defeats the prefetcher, so this is the cache-miss-bound case.
static double box_random_get(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_darray_bench a;
    // Pre-reserved to match box2d's b2Array_CreateN -- otherwise the two sides
    // differ in buffer provenance and this compares setup, not the timed op.
    bx_darray_bench_init_capacity(&a, n);
    for (uint32_t i = 0; i < n; i++)
    {
        bx_darray_bench_push(&a, keys[i]);
    }

    double t0 = bx_bench_now();
    uint64_t sum = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        sum += *bx_darray_bench_get(&a, keys[i] % n);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(sum);
    bx_darray_bench_drop(&a);
    return t1 - t0;
}

static double box_pop(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_darray_bench a;
    // Pre-reserved to match box2d's b2Array_CreateN -- otherwise the two sides
    // differ in buffer provenance and this compares setup, not the timed op.
    bx_darray_bench_init_capacity(&a, n);
    for (uint32_t i = 0; i < n; i++)
    {
        bx_darray_bench_push(&a, keys[i]);
    }

    double t0 = bx_bench_now();
    uint64_t sum = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        sum += bx_darray_bench_pop(&a);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(sum);
    bx_darray_bench_drop(&a);
    return t1 - t0;
}

// ---------------------------------------------------------------------------
// Hand-rolled realloc array: the floor any dynamic array should beat or match.
// ---------------------------------------------------------------------------

static double raw_push(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    uint32_t* a = NULL;
    uint32_t size = 0;
    uint32_t cap = 0;

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        if (size == cap)
        {
            cap = cap ? cap * 2 : 8;
            a = (uint32_t*)realloc(a, (size_t)cap * sizeof(uint32_t));
            // Matches box's bx_realloc: an assert, so it compiles out under the
            // perf build's NDEBUG and the timed loop stays identical codegen to
            // box's. A checkless raw floor would segfault instead of diagnosing.
            assert(a != NULL && "raw_push: allocation failed");
        }
        a[size++] = keys[i];
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(size);
    free(a);
    return t1 - t0;
}

// Memory-bandwidth ceiling for push_warm: a resident, pre-sized buffer written
// straight through with no per-element capacity check. With no branch to block
// it the compiler vectorizes this to bulk SIMD stores, so it runs several times
// faster than any real push -- that gap is the price of the per-element bounds
// check every dynamic array pays, not slack in box specifically. It is the
// ceiling the warm rows are measured against, not a like-for-like push.
static double raw_push_warm(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    static uint32_t* a = NULL;
    static uint32_t provisioned = 0;
    if (provisioned < n)
    {
        free(a);
        a = (uint32_t*)malloc((size_t)n * sizeof(uint32_t));
        assert(a != NULL && "raw_push_warm: allocation failed");
        memset(a, 0, (size_t)n * sizeof(uint32_t));
        provisioned = n;
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        a[i] = keys[i];
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(a[n - 1]);
    return t1 - t0;
}

static double raw_iterate(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    uint32_t* a = (uint32_t*)malloc((size_t)n * sizeof(uint32_t));
    assert(a != NULL && "raw_iterate: allocation failed");
    for (uint32_t i = 0; i < n; i++)
    {
        a[i] = keys[i];
    }

    double t0 = bx_bench_now();
    uint64_t sum = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        sum += a[i];
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(sum);
    free(a);
    return t1 - t0;
}

// ---------------------------------------------------------------------------
// stb_ds arrays
// ---------------------------------------------------------------------------

#ifdef BOX_PERF_HAS_STB_DS
#include "stb_ds.h"

static double stb_push(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    uint32_t* a = NULL;

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        arrput(a, keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(arrlen(a));
    arrfree(a);
    return t1 - t0;
}

static double stb_iterate(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    uint32_t* a = NULL;
    for (uint32_t i = 0; i < n; i++)
    {
        arrput(a, keys[i]);
    }

    double t0 = bx_bench_now();
    uint64_t sum = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        sum += a[i];
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(sum);
    arrfree(a);
    return t1 - t0;
}
#endif

void bx_bench_register_darray(void)
{
    bx_bench_add("darray", "box", "push", box_push);
    bx_bench_add("darray", "raw_realloc", "push", raw_push);
#ifdef BOX_PERF_HAS_STB_DS
    bx_bench_add("darray", "stb_ds", "push", stb_push);
#endif

    bx_bench_add("darray", "box", "push_reserved", box_push_reserved);

    bx_bench_add("darray", "box", "push_warm", box_push_warm);
    bx_bench_add("darray", "raw_realloc", "push_warm", raw_push_warm);

    bx_bench_add("darray", "box", "iterate", box_iterate);
    bx_bench_add("darray", "raw_realloc", "iterate", raw_iterate);
#ifdef BOX_PERF_HAS_STB_DS
    bx_bench_add("darray", "stb_ds", "iterate", stb_iterate);
#endif

    bx_bench_add("darray", "box", "random_get", box_random_get);
    bx_bench_add("darray", "box", "pop", box_pop);
}
