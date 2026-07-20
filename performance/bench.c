#include "bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if !defined(__GNUC__) && !defined(__clang__)
volatile uint64_t bx_bench_black_hole;
#endif

double bx_bench_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

// ---------------------------------------------------------------------------
// Keys
// ---------------------------------------------------------------------------

static uint64_t splitmix64(uint64_t* state)
{
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

uint32_t* bx_keys_make(uint32_t n, uint64_t seed)
{
    uint32_t* keys = (uint32_t*)malloc((size_t)n * sizeof(uint32_t));
    if (keys == NULL)
    {
        return NULL;
    }
    for (uint32_t i = 0; i < n; i++)
    {
        keys[i] = i + 1;
    }
    // Fisher-Yates. Insertion order matters: sorted keys probe very differently
    // from shuffled ones in an open-addressing table.
    uint64_t state = seed;
    for (uint32_t i = n; i > 1; i--)
    {
        uint32_t j = (uint32_t)(splitmix64(&state) % i);
        uint32_t tmp = keys[i - 1];
        keys[i - 1] = keys[j];
        keys[j] = tmp;
    }
    return keys;
}

uint32_t* bx_keys_make_misses(uint32_t n, uint64_t seed)
{
    // Present keys are 1..n, so anything above n is guaranteed absent.
    uint32_t* keys = (uint32_t*)malloc((size_t)n * sizeof(uint32_t));
    if (keys == NULL)
    {
        return NULL;
    }
    uint64_t state = seed ^ 0xdeadbeefULL;
    for (uint32_t i = 0; i < n; i++)
    {
        keys[i] = n + 1 + (uint32_t)(splitmix64(&state) % n);
    }
    return keys;
}

void bx_keys_free(uint32_t* keys)
{
    free(keys);
}

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

#define BX_BENCH_MAX_CASES 256

typedef struct bx_bench_entry
{
    const char* group;
    const char* impl;
    const char* op;
    bx_bench_case fn;
} bx_bench_entry;

static bx_bench_entry g_cases[BX_BENCH_MAX_CASES];
static uint32_t g_case_count;

void bx_bench_add(const char* group, const char* impl, const char* op, bx_bench_case fn)
{
    if (g_case_count >= BX_BENCH_MAX_CASES)
    {
        fprintf(stderr, "bench: case table full, raise BX_BENCH_MAX_CASES\n");
        exit(1);
    }
    bx_bench_entry* e = &g_cases[g_case_count++];
    e->group = group;
    e->impl = impl;
    e->op = op;
    e->fn = fn;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

static int cmp_double(const void* a, const void* b)
{
    double x = *(const double*)a;
    double y = *(const double*)b;
    return (x > y) - (x < y);
}

typedef struct bx_bench_stats
{
    double best; // fastest sample, the least noise-contaminated estimate
    double median;
    double trimmed; // mean after dropping the fastest and slowest 25%
    double noise;   // median absolute deviation / median
} bx_bench_stats;

static double median_of_sorted(const double* sorted, uint32_t count)
{
    return (count % 2) ? sorted[count / 2]
                       : 0.5 * (sorted[count / 2 - 1] + sorted[count / 2]);
}

// `scratch` must have room for `count` doubles; it holds the deviations used
// for the MAD so this stays allocation-free.
static bx_bench_stats summarize(double* samples, double* scratch, uint32_t count)
{
    bx_bench_stats s;
    qsort(samples, count, sizeof(double), cmp_double);

    s.best = samples[0];
    s.median = median_of_sorted(samples, count);

    // Relative MAD, not (max-min)/median. The old spread was a pure
    // extreme-order statistic: a single descheduled sample moved it by
    // hundreds of percent while the trimmed mean it was supposed to qualify
    // did not budge, so it fired constant false alarms. MAD ignores the tails
    // and reports what the bulk of the samples actually did.
    for (uint32_t i = 0; i < count; i++)
    {
        double d = samples[i] - s.median;
        scratch[i] = d < 0.0 ? -d : d;
    }
    qsort(scratch, count, sizeof(double), cmp_double);
    double mad = median_of_sorted(scratch, count);
    s.noise = s.median > 0.0 ? mad / s.median : 0.0;

    // Drop the top and bottom quarter, average the rest. The reference C/C++
    // hash-table benchmark does the same to blunt background scheduling noise.
    uint32_t drop = count / 4;
    uint32_t lo = drop;
    uint32_t hi = count - drop;
    if (hi <= lo)
    {
        lo = 0;
        hi = count;
    }
    double sum = 0.0;
    for (uint32_t i = lo; i < hi; i++)
    {
        sum += samples[i];
    }
    s.trimmed = sum / (double)(hi - lo);
    return s;
}

// ---------------------------------------------------------------------------
// Report
// ---------------------------------------------------------------------------

typedef struct bx_bench_result
{
    const bx_bench_entry* entry;
    bx_bench_stats stats;
    double ns_per_op;
    // Ranks by first appearance, so the report groups every implementation of
    // an operation together no matter what order they registered in.
    uint32_t group_rank;
    uint32_t op_rank;
    uint32_t order;
} bx_bench_result;

static int cmp_result(const void* a, const void* b)
{
    const bx_bench_result* x = (const bx_bench_result*)a;
    const bx_bench_result* y = (const bx_bench_result*)b;
    if (x->group_rank != y->group_rank)
    {
        return (x->group_rank > y->group_rank) - (x->group_rank < y->group_rank);
    }
    if (x->op_rank != y->op_rank)
    {
        return (x->op_rank > y->op_rank) - (x->op_rank < y->op_rank);
    }
    return (x->order > y->order) - (x->order < y->order);
}

// Rank of the first result sharing this group (and op, when `op` is non-NULL).
static uint32_t rank_of(const bx_bench_result* results, uint32_t count, const char* group,
                        const char* op)
{
    uint32_t rank = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        const bx_bench_entry* e = results[i].entry;
        bool same = (op == NULL) ? (strcmp(e->group, group) == 0)
                                 : (strcmp(e->group, group) == 0 && strcmp(e->op, op) == 0);
        if (same)
        {
            return rank;
        }
        bool counted = false;
        for (uint32_t j = 0; j < i; j++)
        {
            const bx_bench_entry* p = results[j].entry;
            if (op == NULL ? (strcmp(p->group, e->group) == 0)
                           : (strcmp(p->group, e->group) == 0 && strcmp(p->op, e->op) == 0))
            {
                counted = true;
                break;
            }
        }
        if (!counted)
        {
            rank++;
        }
    }
    return rank;
}

static bool matches(const bx_bench_entry* e, const char* filter)
{
    if (filter == NULL || filter[0] == '\0')
    {
        return true;
    }
    char path[256];
    snprintf(path, sizeof(path), "%s/%s/%s", e->group, e->impl, e->op);
    return strstr(path, filter) != NULL;
}

// Baseline for a group/op pair is whichever row is BX_BENCH_BASELINE.
static double baseline_for(const bx_bench_result* results, uint32_t count,
                           const bx_bench_entry* e)
{
    for (uint32_t i = 0; i < count; i++)
    {
        const bx_bench_entry* c = results[i].entry;
        if (strcmp(c->impl, BX_BENCH_BASELINE) == 0 && strcmp(c->group, e->group) == 0 &&
            strcmp(c->op, e->op) == 0)
        {
            return results[i].ns_per_op;
        }
    }
    return 0.0;
}

void bx_bench_run_all(const bx_bench_config* cfg)
{
    uint32_t* keys = bx_keys_make(cfg->n, cfg->seed);
    uint32_t* misses = bx_keys_make_misses(cfg->n, cfg->seed);
    if (keys == NULL || misses == NULL)
    {
        fprintf(stderr, "bench: out of memory generating %u keys\n", cfg->n);
        exit(1);
    }

    bx_bench_result* results =
        (bx_bench_result*)malloc(BX_BENCH_MAX_CASES * sizeof(bx_bench_result));
    if (results == NULL)
    {
        fprintf(stderr, "bench: out of memory\n");
        exit(1);
    }

    // Select the matching cases up front so the measurement loop below can walk
    // them by index without re-testing the filter.
    uint32_t result_count = 0;
    for (uint32_t c = 0; c < g_case_count; c++)
    {
        if (matches(&g_cases[c], cfg->filter))
        {
            results[result_count++].entry = &g_cases[c];
        }
    }
    if (result_count == 0)
    {
        fprintf(stderr, "bench: no cases match filter '%s'\n",
                cfg->filter ? cfg->filter : "");
        free(results);
        bx_keys_free(keys);
        bx_keys_free(misses);
        return;
    }

    // samples[case][rep], plus one row of scratch for the MAD and one
    // permutation of the case indices reshuffled each rep (see the run loop).
    double* samples = (double*)malloc((size_t)result_count * cfg->reps * sizeof(double));
    double* scratch = (double*)malloc(cfg->reps * sizeof(double));
    uint32_t* order = (uint32_t*)malloc(result_count * sizeof(uint32_t));
    if (samples == NULL || scratch == NULL || order == NULL)
    {
        fprintf(stderr, "bench: out of memory\n");
        exit(1);
    }
    for (uint32_t c = 0; c < result_count; c++)
    {
        order[c] = c;
    }
    uint64_t order_state = cfg->seed ^ 0x5b3cc1a7f2e9d4bULL;

    // Warmup pulls code and data into cache and lets the CPU settle at a steady
    // clock before anything is recorded.
    for (uint32_t w = 0; w < cfg->warmup; w++)
    {
        for (uint32_t c = 0; c < result_count; c++)
        {
            results[c].entry->fn(keys, misses, cfg->n);
        }
    }

    // Reps are the outer loop and cases the inner one, so every case is sampled
    // once per pass. Running all reps of a case back to back instead would tie
    // each case to one slice of wall time, and any drift over the run -- thermal,
    // background load, page cache warming -- would land on whichever cases
    // happened to be running, confounded with the case itself. Interleaved, drift
    // hits every case equally and cancels out of the comparison.
    //
    // The order of cases *within* a pass is reshuffled every rep. A fixed order
    // cancels time-varying drift but not allocator state: a case registered late
    // always runs on a heap that every earlier case has already fragmented, and
    // for a grow-from-empty case that reallocation cost is real and systematic --
    // fixed order silently penalises whichever container the later benchmark
    // happens to test. Shuffling makes every case see the full spread of heap
    // states across its reps, so the bias averages out instead of attaching to
    // one row. The permutation is seed-derived, so runs stay reproducible.
    for (uint32_t r = 0; r < cfg->reps; r++)
    {
        // Fisher-Yates: a fresh permutation of the case indices for this rep.
        for (uint32_t i = result_count; i > 1; i--)
        {
            uint32_t j = (uint32_t)(splitmix64(&order_state) % i);
            uint32_t tmp = order[i - 1];
            order[i - 1] = order[j];
            order[j] = tmp;
        }

        for (uint32_t k = 0; k < result_count; k++)
        {
            uint32_t c = order[k];
            const bx_bench_entry* e = results[c].entry;
            samples[(size_t)c * cfg->reps + r] = e->fn(keys, misses, cfg->n);

            // Only animate on a terminal: piped into a file the carriage returns
            // would just concatenate every case onto one line.
            if (!cfg->csv && isatty(fileno(stderr)))
            {
                fprintf(stderr, "\r  rep %u/%u  %-28s", r + 1, cfg->reps, e->op);
                fflush(stderr);
            }
        }
    }
    if (!cfg->csv && isatty(fileno(stderr)))
    {
        fprintf(stderr, "\r%-48s\r", "");
    }

    for (uint32_t c = 0; c < result_count; c++)
    {
        results[c].stats = summarize(&samples[(size_t)c * cfg->reps], scratch, cfg->reps);
        results[c].ns_per_op = results[c].stats.trimmed * 1e9 / (double)cfg->n;
    }

    for (uint32_t i = 0; i < result_count; i++)
    {
        const bx_bench_entry* e = results[i].entry;
        results[i].group_rank = rank_of(results, result_count, e->group, NULL);
        results[i].op_rank = rank_of(results, result_count, e->group, e->op);
        results[i].order = i;
    }
    qsort(results, result_count, sizeof(bx_bench_result), cmp_result);

    if (cfg->csv)
    {
        printf("group,impl,op,n,ns_per_op,best_ns_per_op,noise_pct,vs_baseline\n");
        for (uint32_t i = 0; i < result_count; i++)
        {
            const bx_bench_entry* e = results[i].entry;
            double base = baseline_for(results, result_count, e);
            printf("%s,%s,%s,%u,%.6g,%.6g,%.2f,%.4f\n", e->group, e->impl, e->op, cfg->n,
                   results[i].ns_per_op, results[i].stats.best * 1e9 / (double)cfg->n,
                   results[i].stats.noise * 100.0,
                   base > 0.0 ? results[i].ns_per_op / base : 0.0);
        }
        goto done;
    }

    printf("\n  n = %u elements, %u reps (+%u warmup), seed %llu\n", cfg->n, cfg->reps,
           cfg->warmup, (unsigned long long)cfg->seed);
    printf("  ns/op is the trimmed mean; best is the fastest single rep.\n");
    printf("  noise is the median absolute deviation as a fraction of the median.\n");
    printf("  vs box: >1.00 means slower than box, <1.00 means faster.\n\n");

    const char* group = NULL;
    for (uint32_t i = 0; i < result_count; i++)
    {
        const bx_bench_entry* e = results[i].entry;
        if (group == NULL || strcmp(group, e->group) != 0)
        {
            // Blank line between groups, and the table sits one level in from
            // the group name, so a long report stays scannable.
            if (group != NULL)
            {
                printf("\n");
            }
            group = e->group;
            printf("  %s\n", group);
            printf("    %-20s %-14s %10s %10s %8s %9s\n", "operation", "impl", "ns/op", "best",
                   "noise", "vs box");
            printf("    %-20s %-14s %10s %10s %8s %9s\n", "--------------------",
                   "--------------", "----------", "----------", "--------", "---------");
        }

        double base = baseline_for(results, result_count, e);
        double ratio = base > 0.0 ? results[i].ns_per_op / base : 0.0;
        char ratio_buf[16];
        if (strcmp(e->impl, BX_BENCH_BASELINE) == 0 || ratio == 0.0)
        {
            snprintf(ratio_buf, sizeof(ratio_buf), "%s", "--");
        }
        else
        {
            snprintf(ratio_buf, sizeof(ratio_buf), "%.2fx", ratio);
        }

        // %g, not a fixed number of decimals: whole-set ops like popcount touch
        // 64 bits per instruction, so their per-element cost is a thousandth of
        // a nanosecond and any fixed width that suits the per-element rows
        // flattens them to 0.001. Four significant digits suits both.
        printf("    %-20s %-14s %10.4g %10.4g %6.1f%%  %9s\n", e->op, e->impl,
               results[i].ns_per_op, results[i].stats.best * 1e9 / (double)cfg->n,
               results[i].stats.noise * 100.0, ratio_buf);
    }
    printf("\n");

done:
    free(results);
    free(samples);
    free(scratch);
    free(order);
    bx_keys_free(keys);
    bx_keys_free(misses);
}
