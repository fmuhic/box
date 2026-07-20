#include "bench.h"

#include "box/hmap.h"

BX_HMAP_DECLARE(uint32_t, float, bench, bx_bench_hash_u32, bx_bench_eq_u32)

// ---------------------------------------------------------------------------
// box
// ---------------------------------------------------------------------------

static double box_insert(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_hmap_bench m;
    bx_hmap_bench_init(&m);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        bx_hmap_bench_insert(&m, keys[i], (float)i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(bx_hmap_bench_size(&m));
    bx_hmap_bench_drop(&m);
    return t1 - t0;
}

// Same work with the growth cost removed, which isolates how much of insert is
// rehashing versus probing.
static double box_insert_reserved(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_hmap_bench m;
    bx_hmap_bench_init_capacity(&m, n);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        bx_hmap_bench_insert(&m, keys[i], (float)i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(bx_hmap_bench_size(&m));
    bx_hmap_bench_drop(&m);
    return t1 - t0;
}

static double box_lookup_hit(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_hmap_bench m;
    bx_hmap_bench_init(&m);
    for (uint32_t i = 0; i < n; i++)
    {
        bx_hmap_bench_insert(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        float* v = bx_hmap_bench_get(&m, keys[i]);
        BX_BENCH_SINK(v);
    }
    double t1 = bx_bench_now();

    bx_hmap_bench_drop(&m);
    return t1 - t0;
}

static double box_lookup_miss(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    bx_hmap_bench m;
    bx_hmap_bench_init(&m);
    for (uint32_t i = 0; i < n; i++)
    {
        bx_hmap_bench_insert(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        float* v = bx_hmap_bench_get(&m, misses[i]);
        BX_BENCH_SINK(v);
    }
    double t1 = bx_bench_now();

    bx_hmap_bench_drop(&m);
    return t1 - t0;
}

static double box_erase(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_hmap_bench m;
    bx_hmap_bench_init(&m);
    for (uint32_t i = 0; i < n; i++)
    {
        bx_hmap_bench_insert(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        bx_hmap_bench_erase(&m, keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(bx_hmap_bench_size(&m));
    bx_hmap_bench_drop(&m);
    return t1 - t0;
}

// Overwriting an existing key: hits the found-path of insert, no growth.
static double box_replace(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    bx_hmap_bench m;
    bx_hmap_bench_init(&m);
    for (uint32_t i = 0; i < n; i++)
    {
        bx_hmap_bench_insert(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        bx_hmap_bench_insert(&m, keys[i], (float)(i + 1));
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(bx_hmap_bench_size(&m));
    bx_hmap_bench_drop(&m);
    return t1 - t0;
}

// ---------------------------------------------------------------------------
// khash (klib)
// ---------------------------------------------------------------------------

#ifdef BOX_PERF_HAS_KHASH
#include "khash.h"

// khash's built-in integer hash is the identity function. Giving it the same
// mixer as everyone else is what makes this a comparison of tables rather than
// of hash functions.
static inline khint_t kh_bench_hash(uint32_t k)
{
    return (khint_t)bx_bench_hash_u32(k);
}
#define kh_bench_eq(a, b) ((a) == (b))

KHASH_INIT(bench, uint32_t, float, 1, kh_bench_hash, kh_bench_eq)

static double kh_insert(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    khash_t(bench)* m = kh_init(bench);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        int ret;
        khiter_t it = kh_put(bench, m, keys[i], &ret);
        kh_value(m, it) = (float)i;
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(kh_size(m));
    kh_destroy(bench, m);
    return t1 - t0;
}

static double kh_insert_reserved(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    khash_t(bench)* m = kh_init(bench);
    kh_resize(bench, m, n);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        int ret;
        khiter_t it = kh_put(bench, m, keys[i], &ret);
        kh_value(m, it) = (float)i;
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(kh_size(m));
    kh_destroy(bench, m);
    return t1 - t0;
}

static double kh_lookup_hit(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    khash_t(bench)* m = kh_init(bench);
    for (uint32_t i = 0; i < n; i++)
    {
        int ret;
        khiter_t it = kh_put(bench, m, keys[i], &ret);
        kh_value(m, it) = (float)i;
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        khiter_t it = kh_get(bench, m, keys[i]);
        BX_BENCH_SINK(it);
    }
    double t1 = bx_bench_now();

    kh_destroy(bench, m);
    return t1 - t0;
}

static double kh_lookup_miss(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    khash_t(bench)* m = kh_init(bench);
    for (uint32_t i = 0; i < n; i++)
    {
        int ret;
        khiter_t it = kh_put(bench, m, keys[i], &ret);
        kh_value(m, it) = (float)i;
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        khiter_t it = kh_get(bench, m, misses[i]);
        BX_BENCH_SINK(it);
    }
    double t1 = bx_bench_now();

    kh_destroy(bench, m);
    return t1 - t0;
}

static double kh_erase(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    khash_t(bench)* m = kh_init(bench);
    for (uint32_t i = 0; i < n; i++)
    {
        int ret;
        khiter_t it = kh_put(bench, m, keys[i], &ret);
        kh_value(m, it) = (float)i;
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        khiter_t it = kh_get(bench, m, keys[i]);
        if (it != kh_end(m))
        {
            kh_del(bench, m, it);
        }
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(kh_size(m));
    kh_destroy(bench, m);
    return t1 - t0;
}

static double kh_replace(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    khash_t(bench)* m = kh_init(bench);
    for (uint32_t i = 0; i < n; i++)
    {
        int ret;
        khiter_t it = kh_put(bench, m, keys[i], &ret);
        kh_value(m, it) = (float)i;
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        int ret;
        khiter_t it = kh_put(bench, m, keys[i], &ret);
        kh_value(m, it) = (float)(i + 1);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(kh_size(m));
    kh_destroy(bench, m);
    return t1 - t0;
}
#endif // BOX_PERF_HAS_KHASH

// ---------------------------------------------------------------------------
// Verstable
// ---------------------------------------------------------------------------

#ifdef BOX_PERF_HAS_VERSTABLE
#define NAME    vt_bench
#define KEY_TY  uint32_t
#define VAL_TY  float
#define HASH_FN bx_bench_hash_u32
#define CMPR_FN bx_bench_eq_u32
#include "verstable.h"

static double vt_insert(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    vt_bench m;
    vt_bench_init(&m);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        vt_bench_insert(&m, keys[i], (float)i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(vt_bench_size(&m));
    vt_bench_cleanup(&m);
    return t1 - t0;
}

static double vt_lookup_hit(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    vt_bench m;
    vt_bench_init(&m);
    for (uint32_t i = 0; i < n; i++)
    {
        vt_bench_insert(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        vt_bench_itr it = vt_bench_get(&m, keys[i]);
        BX_BENCH_SINK(it.data);
    }
    double t1 = bx_bench_now();

    vt_bench_cleanup(&m);
    return t1 - t0;
}

static double vt_lookup_miss(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    vt_bench m;
    vt_bench_init(&m);
    for (uint32_t i = 0; i < n; i++)
    {
        vt_bench_insert(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        vt_bench_itr it = vt_bench_get(&m, misses[i]);
        BX_BENCH_SINK(it.data);
    }
    double t1 = bx_bench_now();

    vt_bench_cleanup(&m);
    return t1 - t0;
}

static double vt_erase(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    vt_bench m;
    vt_bench_init(&m);
    for (uint32_t i = 0; i < n; i++)
    {
        vt_bench_insert(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        vt_bench_erase(&m, keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(vt_bench_size(&m));
    vt_bench_cleanup(&m);
    return t1 - t0;
}

static double vt_replace(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    vt_bench m;
    vt_bench_init(&m);
    for (uint32_t i = 0; i < n; i++)
    {
        vt_bench_insert(&m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        vt_bench_insert(&m, keys[i], (float)(i + 1));
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(vt_bench_size(&m));
    vt_bench_cleanup(&m);
    return t1 - t0;
}
#endif // BOX_PERF_HAS_VERSTABLE

// ---------------------------------------------------------------------------
// stb_ds
// ---------------------------------------------------------------------------

#ifdef BOX_PERF_HAS_STB_DS
#include "stb_ds.h"

typedef struct stb_entry
{
    uint32_t key;
    float value;
} stb_entry;

static double stb_insert(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    stb_entry* m = NULL;

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        hmput(m, keys[i], (float)i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(hmlen(m));
    hmfree(m);
    return t1 - t0;
}

static double stb_lookup_hit(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    stb_entry* m = NULL;
    for (uint32_t i = 0; i < n; i++)
    {
        hmput(m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        ptrdiff_t idx = hmgeti(m, keys[i]);
        BX_BENCH_SINK(idx);
    }
    double t1 = bx_bench_now();

    hmfree(m);
    return t1 - t0;
}

static double stb_lookup_miss(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    stb_entry* m = NULL;
    for (uint32_t i = 0; i < n; i++)
    {
        hmput(m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        ptrdiff_t idx = hmgeti(m, misses[i]);
        BX_BENCH_SINK(idx);
    }
    double t1 = bx_bench_now();

    hmfree(m);
    return t1 - t0;
}

static double stb_erase(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    stb_entry* m = NULL;
    for (uint32_t i = 0; i < n; i++)
    {
        hmput(m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        hmdel(m, keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(hmlen(m));
    hmfree(m);
    return t1 - t0;
}

static double stb_replace(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    stb_entry* m = NULL;
    for (uint32_t i = 0; i < n; i++)
    {
        hmput(m, keys[i], (float)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        hmput(m, keys[i], (float)(i + 1));
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(hmlen(m));
    hmfree(m);
    return t1 - t0;
}
#endif // BOX_PERF_HAS_STB_DS

// ---------------------------------------------------------------------------

void bx_bench_register_hmap(void)
{
    bx_bench_add("hmap", "box", "insert", box_insert);
#ifdef BOX_PERF_HAS_KHASH
    bx_bench_add("hmap", "khash", "insert", kh_insert);
#endif
#ifdef BOX_PERF_HAS_VERSTABLE
    bx_bench_add("hmap", "verstable", "insert", vt_insert);
#endif
#ifdef BOX_PERF_HAS_STB_DS
    bx_bench_add("hmap", "stb_ds", "insert", stb_insert);
#endif

    bx_bench_add("hmap", "box", "insert_reserved", box_insert_reserved);
#ifdef BOX_PERF_HAS_KHASH
    bx_bench_add("hmap", "khash", "insert_reserved", kh_insert_reserved);
#endif

    bx_bench_add("hmap", "box", "lookup_hit", box_lookup_hit);
#ifdef BOX_PERF_HAS_KHASH
    bx_bench_add("hmap", "khash", "lookup_hit", kh_lookup_hit);
#endif
#ifdef BOX_PERF_HAS_VERSTABLE
    bx_bench_add("hmap", "verstable", "lookup_hit", vt_lookup_hit);
#endif
#ifdef BOX_PERF_HAS_STB_DS
    bx_bench_add("hmap", "stb_ds", "lookup_hit", stb_lookup_hit);
#endif

    bx_bench_add("hmap", "box", "lookup_miss", box_lookup_miss);
#ifdef BOX_PERF_HAS_KHASH
    bx_bench_add("hmap", "khash", "lookup_miss", kh_lookup_miss);
#endif
#ifdef BOX_PERF_HAS_VERSTABLE
    bx_bench_add("hmap", "verstable", "lookup_miss", vt_lookup_miss);
#endif
#ifdef BOX_PERF_HAS_STB_DS
    bx_bench_add("hmap", "stb_ds", "lookup_miss", stb_lookup_miss);
#endif

    bx_bench_add("hmap", "box", "erase", box_erase);
#ifdef BOX_PERF_HAS_KHASH
    bx_bench_add("hmap", "khash", "erase", kh_erase);
#endif
#ifdef BOX_PERF_HAS_VERSTABLE
    bx_bench_add("hmap", "verstable", "erase", vt_erase);
#endif
#ifdef BOX_PERF_HAS_STB_DS
    bx_bench_add("hmap", "stb_ds", "erase", stb_erase);
#endif

    bx_bench_add("hmap", "box", "replace", box_replace);
#ifdef BOX_PERF_HAS_KHASH
    bx_bench_add("hmap", "khash", "replace", kh_replace);
#endif
#ifdef BOX_PERF_HAS_VERSTABLE
    bx_bench_add("hmap", "verstable", "replace", vt_replace);
#endif
#ifdef BOX_PERF_HAS_STB_DS
    bx_bench_add("hmap", "stb_ds", "replace", stb_replace);
#endif
}
