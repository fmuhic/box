#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "box/core.h"
#include "box/hmap.h" // new type-erased implementation
#include "hmap_old.h" // original macro-per-type implementation (renamed)

// ---------------------------------------------------------------------------
// Key / value types chosen to mirror game usage:
//   int32_t          -> entity id / handle (tiny key)
//   vec3             -> position (small struct value, 12B)
//   KeyBig  (32B)    -> composite / GUID-style key
//   Transform (48B)  -> a matrix-ish component value
// ---------------------------------------------------------------------------

typedef struct vec3
{
    float x, y, z;
} vec3;
typedef struct KeyBig
{
    uint64_t a, b, c, d;
} KeyBig;
typedef struct Transform
{
    float m[12];
} Transform;

static uint64_t hash_i32(int32_t key)
{
    uint64_t x = (uint64_t)key;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return x;
}
static bool eq_i32(int32_t a, int32_t b)
{
    return a == b;
}

static uint64_t hash_keybig(KeyBig k)
{
    uint64_t h = k.a;
    h ^= k.b + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= k.c + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    h ^= k.d + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
    return h;
}
static bool eq_keybig(KeyBig a, KeyBig b)
{
    return a.a == b.a && a.b == b.b && a.c == b.c && a.d == b.d;
}

static inline KeyBig make_keybig(int i)
{
    uint64_t x = (uint64_t)i;
    KeyBig k = { x, x * 2654435761u + 1, x ^ 0x9e3779b97f4a7c15ULL, ~x };
    return k;
}
static inline Transform make_transform(int i)
{
    Transform t;
    for (int j = 0; j < 12; j++)
    {
        t.m[j] = (float)(i + j);
    }
    return t;
}

// New and old maps for every benchmarked (K,V) combination.
BX_HMAP_DECLARE(int32_t, int32_t, ni32, hash_i32, eq_i32)
BXOLD_HMAP_DECLARE(int32_t, int32_t, oi32)
BXOLD_HMAP_SOURCE(int32_t, int32_t, oi32, hash_i32, eq_i32)

BX_HMAP_DECLARE(int32_t, vec3, ni32v3, hash_i32, eq_i32)
BXOLD_HMAP_DECLARE(int32_t, vec3, oi32v3)
BXOLD_HMAP_SOURCE(int32_t, vec3, oi32v3, hash_i32, eq_i32)

BX_HMAP_DECLARE(KeyBig, vec3, nbigv3, hash_keybig, eq_keybig)
BXOLD_HMAP_DECLARE(KeyBig, vec3, obigv3)
BXOLD_HMAP_SOURCE(KeyBig, vec3, obigv3, hash_keybig, eq_keybig)

BX_HMAP_DECLARE(KeyBig, Transform, nbigtf, hash_keybig, eq_keybig)
BXOLD_HMAP_DECLARE(KeyBig, Transform, obigtf)
BXOLD_HMAP_SOURCE(KeyBig, Transform, obigtf, hash_keybig, eq_keybig)

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

typedef struct
{
    double insert, get_hit, get_miss, erase;
} result;

// Generates a benchmark routine. PREFIX is the map function prefix, MAPT the
// map type, KT/VT the key/value types, KEXPR builds a key from int _i, VEXPR a
// value from int _i. Reports the best (min) ns/op over `reps` repetitions.
#define GEN_BENCH(FN, PREFIX, MAPT, KT, VT, KEXPR, VEXPR)           \
    static void FN(int N, int reps, uint64_t* sink, result* out)    \
    {                                                               \
        KT* keys = malloc(sizeof(KT) * (size_t)N);                  \
        KT* misses = malloc(sizeof(KT) * (size_t)N);                \
        for (int i = 0; i < N; i++)                                 \
        {                                                           \
            int _i = i;                                             \
            keys[i] = (KEXPR);                                      \
        }                                                           \
        for (int i = 0; i < N; i++)                                 \
        {                                                           \
            int _i = i + N + 777;                                   \
            misses[i] = (KEXPR);                                    \
        }                                                           \
        double b_ins = 1e30, b_gh = 1e30, b_gm = 1e30, b_er = 1e30; \
        uint64_t s = 0;                                             \
        for (int rep = 0; rep < reps; rep++)                        \
        {                                                           \
            MAPT m;                                                 \
            PREFIX##_init(&m);                                      \
            double t0 = now_ns();                                   \
            for (int i = 0; i < N; i++)                             \
            {                                                       \
                int _i = i;                                         \
                VT v = (VEXPR);                                     \
                PREFIX##_insert(&m, keys[i], v);                    \
            }                                                       \
            double t1 = now_ns();                                   \
            if (t1 - t0 < b_ins)                                    \
                b_ins = t1 - t0;                                    \
                                                                    \
            double g0 = now_ns();                                   \
            for (int i = 0; i < N; i++)                             \
            {                                                       \
                VT* p = PREFIX##_get(&m, keys[i]);                  \
                s += (uint64_t)(uintptr_t)p;                        \
            }                                                       \
            double g1 = now_ns();                                   \
            if (g1 - g0 < b_gh)                                     \
                b_gh = g1 - g0;                                     \
                                                                    \
            double m0 = now_ns();                                   \
            for (int i = 0; i < N; i++)                             \
            {                                                       \
                VT* p = PREFIX##_get(&m, misses[i]);                \
                s += (uint64_t)(uintptr_t)p;                        \
            }                                                       \
            double m1 = now_ns();                                   \
            if (m1 - m0 < b_gm)                                     \
                b_gm = m1 - m0;                                     \
                                                                    \
            double e0 = now_ns();                                   \
            for (int i = 0; i < N; i++)                             \
                PREFIX##_erase(&m, keys[i]);                        \
            double e1 = now_ns();                                   \
            if (e1 - e0 < b_er)                                     \
                b_er = e1 - e0;                                     \
            PREFIX##_drop(&m);                                      \
        }                                                           \
        *sink += s;                                                 \
        out->insert = b_ins / N;                                    \
        out->get_hit = b_gh / N;                                    \
        out->get_miss = b_gm / N;                                   \
        out->erase = b_er / N;                                      \
        free(keys);                                                 \
        free(misses);                                               \
    }

GEN_BENCH(bench_new_i32, bx_hmap_ni32, bx_hmap_ni32, int32_t, int32_t, (int32_t)_i, (int32_t)(_i * 3))
GEN_BENCH(bench_old_i32, bxold_hmap_oi32, bxold_hmap_oi32, int32_t, int32_t, (int32_t)_i, (int32_t)(_i * 3))
GEN_BENCH(bench_new_i32v3, bx_hmap_ni32v3, bx_hmap_ni32v3, int32_t, vec3, (int32_t)_i, ((vec3){ (float)_i, (float)_i + 1, (float)_i + 2 }))
GEN_BENCH(bench_old_i32v3, bxold_hmap_oi32v3, bxold_hmap_oi32v3, int32_t, vec3, (int32_t)_i, ((vec3){ (float)_i, (float)_i + 1, (float)_i + 2 }))
GEN_BENCH(bench_new_bigv3, bx_hmap_nbigv3, bx_hmap_nbigv3, KeyBig, vec3, make_keybig(_i), ((vec3){ (float)_i, (float)_i + 1, (float)_i + 2 }))
GEN_BENCH(bench_old_bigv3, bxold_hmap_obigv3, bxold_hmap_obigv3, KeyBig, vec3, make_keybig(_i), ((vec3){ (float)_i, (float)_i + 1, (float)_i + 2 }))
GEN_BENCH(bench_new_bigtf, bx_hmap_nbigtf, bx_hmap_nbigtf, KeyBig, Transform, make_keybig(_i), make_transform(_i))
GEN_BENCH(bench_old_bigtf, bxold_hmap_obigtf, bxold_hmap_obigtf, KeyBig, Transform, make_keybig(_i), make_transform(_i))

static void row(const char* op, double oldv, double newv)
{
    double ratio = newv / oldv;
    printf("  %-9s %10.2f %10.2f    %5.2fx %s\n",
           op, oldv, newv, ratio, ratio <= 1.05 ? "" : (ratio <= 1.25 ? "(minor)" : "(!)"));
}

static void report(const char* title, size_t key_b, size_t val_b, int N,
                   result* o, result* n)
{
    printf("\n%s  (key %zuB, val %zuB, N=%d)\n", title, key_b, val_b, N);
    printf("  %-9s %10s %10s    %6s\n", "op", "old ns", "new ns", "new/old");
    row("insert", o->insert, n->insert);
    row("get_hit", o->get_hit, n->get_hit);
    row("get_miss", o->get_miss, n->get_miss);
    row("erase", o->erase, n->erase);
}

int main(int argc, char** argv)
{
    int N = argc > 1 ? atoi(argv[1]) : 1000000;
    int reps = argc > 2 ? atoi(argv[2]) : 5;
    uint64_t sink = 0;
    result o, n;

    printf("hmap benchmark: old (macro-per-type, inlined) vs new (type-erased core)\n");
    printf("N=%d, reps=%d, reporting best-of ns/op\n", N, reps);

    bench_old_i32(N, reps, &sink, &o);
    bench_new_i32(N, reps, &sink, &n);
    report("int32 -> int32", sizeof(int32_t), sizeof(int32_t), N, &o, &n);

    bench_old_i32v3(N, reps, &sink, &o);
    bench_new_i32v3(N, reps, &sink, &n);
    report("int32 -> vec3", sizeof(int32_t), sizeof(vec3), N, &o, &n);

    bench_old_bigv3(N, reps, &sink, &o);
    bench_new_bigv3(N, reps, &sink, &n);
    report("KeyBig -> vec3", sizeof(KeyBig), sizeof(vec3), N, &o, &n);

    bench_old_bigtf(N, reps, &sink, &o);
    bench_new_bigtf(N, reps, &sink, &n);
    report("KeyBig -> Transform", sizeof(KeyBig), sizeof(Transform), N, &o, &n);

    printf("\n(checksum %llu)\n", (unsigned long long)sink);
    return 0;
}
