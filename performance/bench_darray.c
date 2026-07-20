#include "bench.h"

#include "box/darray.h"

#include <stdlib.h>

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

// Sequential read. Mostly a memory-bandwidth measurement; it exists to confirm
// the accessor does not get in the way of the prefetcher.
static double box_iterate(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_darray_bench a;
    bx_darray_bench_init(&a);
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
    bx_darray_bench_init(&a);
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
    bx_darray_bench_init(&a);
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
        }
        a[size++] = keys[i];
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(size);
    free(a);
    return t1 - t0;
}

static double raw_iterate(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    uint32_t* a = (uint32_t*)malloc((size_t)n * sizeof(uint32_t));
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

    bx_bench_add("darray", "box", "iterate", box_iterate);
    bx_bench_add("darray", "raw_realloc", "iterate", raw_iterate);
#ifdef BOX_PERF_HAS_STB_DS
    bx_bench_add("darray", "stb_ds", "iterate", stb_iterate);
#endif

    bx_bench_add("darray", "box", "random_get", box_random_get);
    bx_bench_add("darray", "box", "pop", box_pop);
}
