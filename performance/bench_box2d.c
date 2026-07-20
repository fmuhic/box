#include "bench.h"

#ifdef BOX_PERF_HAS_BOX2D

// Box2D v3's b2BitSet is close to a twin of bx_bitset: uint64 blocks, the same
// blockCapacity/blockCount pair, and the same operation set down to
// b2SetBitCountAndClear and b2InPlaceUnion. It is the most direct external
// comparison anything in box has.
//
// container.h is the other close analog: b2Array_Push is an inline macro doing
// a capacity check and a typed store, growing 2x from an initial 8 -- the same
// shape and the same constants as bx_darray_push.
//
// b2HashSet is left out; it stores uint64 keys with no values and exists only
// to track shape pairs, so it is not a map comparison.

#include "bitset.h"
#include "container.h"
#include "ctz.h"

b2DeclareArrayNative(uint32_t);

static double b2_set(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    b2BitSet s = b2CreateBitSet(n);
    b2SetBitCountAndClear(&s, n);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        b2SetBit(&s, keys[i] % n);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(s.bits[0]);
    b2DestroyBitSet(&s);
    return t1 - t0;
}

static double b2_get(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    b2BitSet s = b2CreateBitSet(n);
    b2SetBitCountAndClear(&s, n);
    for (uint32_t i = 0; i < n; i += 2)
    {
        b2SetBit(&s, i);
    }

    double t0 = bx_bench_now();
    uint32_t hits = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        hits += b2GetBit(&s, keys[i] % n) ? 1u : 0u;
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(hits);
    b2DestroyBitSet(&s);
    return t1 - t0;
}

// Box2D declares b2CountSetBits but does not define it in bitset.c, so the
// scan is written out here against its own b2PopCount64. That makes this row a
// comparison of the block layout, not of Box2D's own popcount routine.
static double b2_popcount(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    b2BitSet s = b2CreateBitSet(n);
    b2SetBitCountAndClear(&s, n);
    for (uint32_t i = 0; i < n; i += 3)
    {
        b2SetBit(&s, i);
    }

    // Batched to clear the timer floor, matching bench_bitset.c's box row.
    double per_scan;
    BX_BENCH_TIME_REPEATED(per_scan, {
        int total = 0;
        for (uint32_t i = 0; i < s.blockCount; i++)
        {
            total += b2PopCount64(s.bits[i]);
        }
        BX_BENCH_SINK(total);
    });

    b2DestroyBitSet(&s);
    return per_scan;
}

static double b2_union(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    b2BitSet a = b2CreateBitSet(n);
    b2BitSet b = b2CreateBitSet(n);
    b2SetBitCountAndClear(&a, n);
    b2SetBitCountAndClear(&b, n);
    for (uint32_t i = 0; i < n; i += 2)
    {
        b2SetBit(&a, i);
    }
    for (uint32_t i = 1; i < n; i += 2)
    {
        b2SetBit(&b, keys[i] % n);
    }

    // Batched to clear the timer floor, matching bench_bitset.c's box row.
    double per_scan;
    BX_BENCH_TIME_REPEATED(per_scan, {
        b2InPlaceUnion(&a, &b);
        BX_BENCH_SINK(a.bits[0]);
    });

    b2DestroyBitSet(&a);
    b2DestroyBitSet(&b);
    return per_scan;
}

// ---------------------------------------------------------------------------
// b2Array vs darray
// ---------------------------------------------------------------------------

static double b2_push(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    b2Array(uint32_t) a;
    b2Array_Create(a);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        b2Array_Push(a, keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(a.count);
    b2Array_Destroy(a);
    return t1 - t0;
}

static double b2_push_reserved(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    b2Array(uint32_t) a;
    b2Array_CreateN(a, (int)n);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        b2Array_Push(a, keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(a.count);
    b2Array_Destroy(a);
    return t1 - t0;
}

static double b2_iterate(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    b2Array(uint32_t) a;
    b2Array_CreateN(a, (int)n);
    for (uint32_t i = 0; i < n; i++)
    {
        b2Array_Push(a, keys[i]);
    }

    double t0 = bx_bench_now();
    uint64_t sum = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        sum += *b2Array_Get(a, (int)i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(sum);
    b2Array_Destroy(a);
    return t1 - t0;
}

static double b2_random_get(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    b2Array(uint32_t) a;
    b2Array_CreateN(a, (int)n);
    for (uint32_t i = 0; i < n; i++)
    {
        b2Array_Push(a, keys[i]);
    }

    double t0 = bx_bench_now();
    uint64_t sum = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        sum += *b2Array_Get(a, (int)(keys[i] % n));
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(sum);
    b2Array_Destroy(a);
    return t1 - t0;
}

static double b2_pop(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    b2Array(uint32_t) a;
    b2Array_CreateN(a, (int)n);
    for (uint32_t i = 0; i < n; i++)
    {
        b2Array_Push(a, keys[i]);
    }

    double t0 = bx_bench_now();
    uint64_t sum = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        sum += b2Array_Pop(a);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(sum);
    b2Array_Destroy(a);
    return t1 - t0;
}

void bx_bench_register_box2d(void)
{
    bx_bench_add("bitset", "box2d", "set", b2_set);
    bx_bench_add("bitset", "box2d", "get", b2_get);
    bx_bench_add("bitset", "box2d", "popcount_scan", b2_popcount);
    bx_bench_add("bitset", "box2d", "union_scan", b2_union);

    bx_bench_add("darray", "box2d", "push", b2_push);
    bx_bench_add("darray", "box2d", "push_reserved", b2_push_reserved);
    bx_bench_add("darray", "box2d", "iterate", b2_iterate);
    bx_bench_add("darray", "box2d", "random_get", b2_random_get);
    bx_bench_add("darray", "box2d", "pop", b2_pop);
}

#else
void bx_bench_register_box2d(void)
{
}
#endif
