#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// The C++ comparisons (see bench_cpp.cpp) include this header, so everything
// declared here has to keep C linkage.
#ifdef __cplusplus
extern "C"
{
#endif

// A benchmark case times one operation over `n` elements and returns the
// seconds spent inside the measured region only. Setup and teardown happen
// inside the case but outside its timer, so allocation and key generation never
// land in the result.
typedef double (*bx_bench_case)(const uint32_t* keys, const uint32_t* misses, uint32_t n);

// `impl` names the implementation ("box", "khash", ...). The implementation
// named BX_BENCH_BASELINE is what every other row in the same group/op is
// reported relative to.
#define BX_BENCH_BASELINE "box"

void bx_bench_add(const char* group, const char* impl, const char* op, bx_bench_case fn);

// Registration entry points, one per container.
void bx_bench_register_hmap(void);
void bx_bench_register_darray(void);
void bx_bench_register_spset(void);
void bx_bench_register_bitset(void);
void bx_bench_register_stc(void);
void bx_bench_register_flecs(void);
void bx_bench_register_box2d(void);
// Defined in bench_cpp.cpp. Compiled only when a C++ compiler and the C++
// headers are both available; a no-op stub stands in otherwise.
void bx_bench_register_cpp(void);

typedef struct bx_bench_config
{
    uint32_t n;         // elements per case
    uint32_t reps;      // timed repetitions
    uint32_t warmup;    // untimed repetitions before measuring
    const char* filter; // substring match on "group/impl/op", NULL for all
    bool csv;
    uint64_t seed;
} bx_bench_config;

// Runs every registered case matching the filter and prints a report.
void bx_bench_run_all(const bx_bench_config* cfg);

// ---------------------------------------------------------------------------
// Measurement primitives
// ---------------------------------------------------------------------------

// Monotonic seconds. Not wall-clock; only differences are meaningful.
double bx_bench_now(void);

// Forces `value` to be materialized so the optimizer cannot delete the work
// that produced it. Every case must sink its results or the compiler is free to
// delete the entire loop and report a few nanoseconds.
#if defined(__GNUC__) || defined(__clang__)
#define BX_BENCH_SINK(value)                                    \
    do                                                          \
    {                                                           \
        __typeof__(value) bx_sink_tmp = (value);                \
        __asm__ volatile("" : : "r,m"(bx_sink_tmp) : "memory"); \
    } while (0)
#else
extern volatile uint64_t bx_bench_black_hole;
#define BX_BENCH_SINK(value)                                 \
    do                                                       \
    {                                                        \
        bx_bench_black_hole ^= (uint64_t)(uintptr_t)(value); \
    } while (0)
#endif

// Shortest region `BX_BENCH_TIME_REPEATED` will accept. Two clock_gettime calls
// cost tens of nanoseconds between them, so a region has to be several orders of
// magnitude longer than that before the timer stops being a visible part of the
// measurement. 20 ms leaves ~6 orders of headroom.
#define BX_BENCH_MIN_SECONDS 0.02

// Times `body` repeatedly until the measured region is long enough to resolve,
// then assigns the per-execution time to `out_seconds`.
//
// Use this for whole-set operations -- popcount, union -- which are a single
// O(n) call rather than a loop of n calls. Timing one such call directly
// reports mostly timer overhead: the scan takes a few hundred nanoseconds and
// the two clock reads around it cost a good fraction of that. The batch doubles
// until the region clears the floor, so the total stays within 2x of it.
//
// `body` MUST sink its result. It runs in a loop with no other observable
// effect, so without a sink the optimizer is free to hoist it out entirely and
// the calibration loop spins forever.
#define BX_BENCH_TIME_REPEATED(out_seconds, body)            \
    do                                                       \
    {                                                        \
        uint64_t bx_iters = 1;                               \
        double bx_elapsed = 0.0;                             \
        for (;;)                                             \
        {                                                    \
            double bx_t0 = bx_bench_now();                   \
            for (uint64_t bx_i = 0; bx_i < bx_iters; bx_i++) \
            {                                                \
                body;                                        \
            }                                                \
            bx_elapsed = bx_bench_now() - bx_t0;             \
            if (bx_elapsed >= BX_BENCH_MIN_SECONDS)          \
            {                                                \
                break;                                       \
            }                                                \
            bx_iters *= 2;                                   \
        }                                                    \
        (out_seconds) = bx_elapsed / (double)bx_iters;       \
    } while (0)

// ---------------------------------------------------------------------------
// Keys
// ---------------------------------------------------------------------------

// Deterministic given the seed, so two runs are comparable.
// `keys`   : 1..n shuffled. Sequential-but-shuffled is the standard workload:
//            raw sequential flatters tables whose hash barely mixes.
// `misses` : n keys guaranteed absent from `keys`.
uint32_t* bx_keys_make(uint32_t n, uint64_t seed);
uint32_t* bx_keys_make_misses(uint32_t n, uint64_t seed);
void bx_keys_free(uint32_t* keys);

// MurmurHash3 finalizer. Every implementation under test is given this same
// hash so the benchmark compares tables, not hash functions.
static inline uint64_t bx_bench_hash_u32(uint32_t key)
{
    uint64_t x = key;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x = x ^ (x >> 31);
    return x;
}

static inline bool bx_bench_eq_u32(uint32_t a, uint32_t b)
{
    return a == b;
}

#ifdef __cplusplus
} // extern "C"
#endif
