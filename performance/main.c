#include "bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char* argv0)
{
    printf("usage: %s [options]\n\n", argv0);
    printf("  -n <count>     elements per case            (default 200000)\n");
    printf("  -r <count>     timed repetitions            (default 9)\n");
    printf("  -w <count>     warmup repetitions           (default 2)\n");
    printf("  -f <substr>    only run cases whose group/impl/op contains substr\n");
    printf("  -s <seed>      PRNG seed for key generation (default 12345)\n");
    printf("  --csv          emit CSV instead of a table\n");
    printf("  -h, --help     this message\n\n");
    printf("examples:\n");
    printf("  %s -f hmap              only the hash map group\n", argv0);
    printf("  %s -f lookup            every lookup case across all containers\n", argv0);
    printf("  %s -n 2000000 --csv     2M elements, machine-readable\n", argv0);
}

int main(int argc, char** argv)
{
    bx_bench_config cfg;
    cfg.n = 200000;
    cfg.reps = 9;
    cfg.warmup = 2;
    cfg.filter = NULL;
    cfg.csv = false;
    cfg.seed = 12345;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--csv") == 0)
        {
            cfg.csv = true;
        }
        else if (i + 1 < argc && strcmp(argv[i], "-n") == 0)
        {
            cfg.n = (uint32_t)strtoul(argv[++i], NULL, 10);
        }
        else if (i + 1 < argc && strcmp(argv[i], "-r") == 0)
        {
            cfg.reps = (uint32_t)strtoul(argv[++i], NULL, 10);
        }
        else if (i + 1 < argc && strcmp(argv[i], "-w") == 0)
        {
            cfg.warmup = (uint32_t)strtoul(argv[++i], NULL, 10);
        }
        else if (i + 1 < argc && strcmp(argv[i], "-f") == 0)
        {
            cfg.filter = argv[++i];
        }
        else if (i + 1 < argc && strcmp(argv[i], "-s") == 0)
        {
            cfg.seed = strtoull(argv[++i], NULL, 10);
        }
        else
        {
            fprintf(stderr, "unknown or incomplete argument: %s\n\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (cfg.n == 0 || cfg.reps == 0)
    {
        fprintf(stderr, "-n and -r must be greater than zero\n");
        return 1;
    }

    fprintf(stderr, "\n  box performance suite\n");
    fprintf(stderr, "  compiled %s, ", __DATE__);
#if defined(__clang__)
    fprintf(stderr, "clang %d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    fprintf(stderr, "gcc %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
    fprintf(stderr, "unknown compiler");
#endif
    fprintf(stderr, "\n  comparisons: box");
#ifdef BOX_PERF_HAS_KHASH
    fprintf(stderr, ", khash");
#endif
#ifdef BOX_PERF_HAS_VERSTABLE
    fprintf(stderr, ", verstable");
#endif
#ifdef BOX_PERF_HAS_STB_DS
    fprintf(stderr, ", stb_ds");
#endif
#ifdef BOX_PERF_HAS_STC
    fprintf(stderr, ", stc");
#endif
#ifdef BOX_PERF_HAS_UNORDERED_DENSE
    fprintf(stderr, ", ankerl_dense");
#endif
#ifdef BOX_PERF_HAS_FLECS
    fprintf(stderr, ", flecs");
#endif
#ifdef BOX_PERF_HAS_BOX2D
    fprintf(stderr, ", box2d");
#endif
#ifdef BOX_PERF_CXX_ENABLED
    fprintf(stderr, ", std::unordered_map, std::vector");
#endif
#if !defined(BOX_PERF_HAS_KHASH) && !defined(BOX_PERF_HAS_VERSTABLE) && \
    !defined(BOX_PERF_HAS_STB_DS)
    fprintf(stderr, "  (no third-party libraries found: run performance/third_party/fetch.sh)");
#endif
    fprintf(stderr, "\n");

    bx_bench_register_hmap();
    bx_bench_register_stc();
    bx_bench_register_flecs();
    bx_bench_register_box2d();
    bx_bench_register_cpp();
    bx_bench_register_spset();
    bx_bench_register_darray();
    bx_bench_register_bitset();

    bx_bench_run_all(&cfg);
    return 0;
}
