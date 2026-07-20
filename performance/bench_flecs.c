#include "bench.h"

#ifdef BOX_PERF_HAS_FLECS

// flecs is the closest thing to a peer for this whole library: an ECS whose
// internals are a sparse set, a vector and a map, used the same way box's are.
// Its sparse set is the only external comparison spset has.
//
// All three take an ecs_allocator_t*; NULL selects the default OS allocator,
// which is the fair comparison against box calling bx_alloc.

#include "flecs.h"

// ---------------------------------------------------------------------------
// ecs_sparse_t vs spset
// ---------------------------------------------------------------------------

static double fl_sparse_insert(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    ecs_sparse_t s;
    flecs_sparse_init(&s, NULL, NULL, (ecs_size_t)sizeof(float));

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        bool is_new;
        float* v = (float*)flecs_sparse_ensure(&s, (ecs_size_t)sizeof(float), keys[i], &is_new);
        *v = (float)i;
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(flecs_sparse_count(&s));
    flecs_sparse_fini(&s);
    return t1 - t0;
}

static double fl_sparse_lookup_hit(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    ecs_sparse_t s;
    flecs_sparse_init(&s, NULL, NULL, (ecs_size_t)sizeof(float));
    for (uint32_t i = 0; i < n; i++)
    {
        bool is_new;
        float* v = (float*)flecs_sparse_ensure(&s, (ecs_size_t)sizeof(float), keys[i], &is_new);
        *v = (float)i;
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        void* v = flecs_sparse_get(&s, (ecs_size_t)sizeof(float), keys[i]);
        BX_BENCH_SINK(v);
    }
    double t1 = bx_bench_now();

    flecs_sparse_fini(&s);
    return t1 - t0;
}

static double fl_sparse_erase(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    ecs_sparse_t s;
    flecs_sparse_init(&s, NULL, NULL, (ecs_size_t)sizeof(float));
    for (uint32_t i = 0; i < n; i++)
    {
        bool is_new;
        float* v = (float*)flecs_sparse_ensure(&s, (ecs_size_t)sizeof(float), keys[i], &is_new);
        *v = (float)i;
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        flecs_sparse_remove(&s, (ecs_size_t)sizeof(float), keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(flecs_sparse_count(&s));
    flecs_sparse_fini(&s);
    return t1 - t0;
}

static double fl_sparse_iterate(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    ecs_sparse_t s;
    flecs_sparse_init(&s, NULL, NULL, (ecs_size_t)sizeof(float));
    for (uint32_t i = 0; i < n; i++)
    {
        bool is_new;
        float* v = (float*)flecs_sparse_ensure(&s, (ecs_size_t)sizeof(float), keys[i], &is_new);
        *v = (float)i;
    }

    double t0 = bx_bench_now();
    double sum = 0.0;
    int32_t count = flecs_sparse_count(&s);
    for (int32_t i = 0; i < count; i++)
    {
        sum += *(float*)flecs_sparse_get_dense(&s, (ecs_size_t)sizeof(float), i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(sum);
    flecs_sparse_fini(&s);
    return t1 - t0;
}

// ---------------------------------------------------------------------------
// ecs_map_t vs hmap
//
// READ THESE ROWS WITH CARE. ecs_map_t takes no hash callback, so unlike every
// other table here it cannot be given the shared mixer. It applies Fibonacci
// hashing internally -- `(11400714819323198485 * key) >> shift` -- which spreads
// *consecutive* integers almost perfectly. This benchmark's keys are 1..n
// shuffled, so they are consecutive values, i.e. precisely its best case.
//
// Measured directly: 2.4 ns/lookup on sequential keys against 8.6 ns on
// scattered keys, a 3.5x swing from the key distribution alone. That is not a
// flaw in flecs -- entity IDs really are sequential, so this is its design
// target -- but it means these numbers describe a table plus a hash on a
// favourable workload, not a table.
// ---------------------------------------------------------------------------

static double fl_map_insert(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    ecs_map_t m = { 0 };
    ecs_map_init(&m, NULL);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        ecs_map_insert(&m, keys[i], (ecs_map_val_t)i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(ecs_map_count(&m));
    ecs_map_fini(&m);
    return t1 - t0;
}

static double fl_map_lookup_hit(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    ecs_map_t m = { 0 };
    ecs_map_init(&m, NULL);
    for (uint32_t i = 0; i < n; i++)
    {
        ecs_map_insert(&m, keys[i], (ecs_map_val_t)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        ecs_map_val_t* v = ecs_map_get(&m, keys[i]);
        BX_BENCH_SINK(v);
    }
    double t1 = bx_bench_now();

    ecs_map_fini(&m);
    return t1 - t0;
}

static double fl_map_lookup_miss(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    ecs_map_t m = { 0 };
    ecs_map_init(&m, NULL);
    for (uint32_t i = 0; i < n; i++)
    {
        ecs_map_insert(&m, keys[i], (ecs_map_val_t)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        ecs_map_val_t* v = ecs_map_get(&m, misses[i]);
        BX_BENCH_SINK(v);
    }
    double t1 = bx_bench_now();

    ecs_map_fini(&m);
    return t1 - t0;
}

static double fl_map_erase(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    ecs_map_t m = { 0 };
    ecs_map_init(&m, NULL);
    for (uint32_t i = 0; i < n; i++)
    {
        ecs_map_insert(&m, keys[i], (ecs_map_val_t)i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        ecs_map_remove(&m, keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(ecs_map_count(&m));
    ecs_map_fini(&m);
    return t1 - t0;
}

// ---------------------------------------------------------------------------
// ecs_vec_t vs darray
// ---------------------------------------------------------------------------

static double fl_vec_push(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    ecs_vec_t v;
    ecs_vec_init(NULL, &v, (ecs_size_t)sizeof(uint32_t), 0);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t* slot = (uint32_t*)ecs_vec_append(NULL, &v, (ecs_size_t)sizeof(uint32_t));
        *slot = keys[i];
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(ecs_vec_count(&v));
    ecs_vec_fini(NULL, &v, (ecs_size_t)sizeof(uint32_t));
    return t1 - t0;
}

static double fl_vec_push_reserved(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    ecs_vec_t v;
    ecs_vec_init(NULL, &v, (ecs_size_t)sizeof(uint32_t), (int32_t)n);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t* slot = (uint32_t*)ecs_vec_append(NULL, &v, (ecs_size_t)sizeof(uint32_t));
        *slot = keys[i];
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(ecs_vec_count(&v));
    ecs_vec_fini(NULL, &v, (ecs_size_t)sizeof(uint32_t));
    return t1 - t0;
}

static double fl_vec_iterate(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    ecs_vec_t v;
    ecs_vec_init(NULL, &v, (ecs_size_t)sizeof(uint32_t), (int32_t)n);
    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t* slot = (uint32_t*)ecs_vec_append(NULL, &v, (ecs_size_t)sizeof(uint32_t));
        *slot = keys[i];
    }

    double t0 = bx_bench_now();
    uint64_t sum = 0;
    uint32_t* data = (uint32_t*)ecs_vec_first(&v);
    for (uint32_t i = 0; i < n; i++)
    {
        sum += data[i];
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(sum);
    ecs_vec_fini(NULL, &v, (ecs_size_t)sizeof(uint32_t));
    return t1 - t0;
}

void bx_bench_register_flecs(void)
{
    // flecs allocations go through its OS abstraction, which has to be
    // initialised before any of these are used.
    ecs_os_init();
    ecs_os_set_api_defaults();

    bx_bench_add("spset", "flecs_sparse", "insert", fl_sparse_insert);
    bx_bench_add("spset", "flecs_sparse", "lookup_hit", fl_sparse_lookup_hit);
    bx_bench_add("spset", "flecs_sparse", "erase", fl_sparse_erase);
    bx_bench_add("spset", "flecs_sparse", "iterate", fl_sparse_iterate);

    bx_bench_add("hmap", "flecs_map", "insert", fl_map_insert);
    bx_bench_add("hmap", "flecs_map", "lookup_hit", fl_map_lookup_hit);
    bx_bench_add("hmap", "flecs_map", "lookup_miss", fl_map_lookup_miss);
    bx_bench_add("hmap", "flecs_map", "erase", fl_map_erase);

    bx_bench_add("darray", "flecs_vec", "push", fl_vec_push);
    bx_bench_add("darray", "flecs_vec", "push_reserved", fl_vec_push_reserved);
    bx_bench_add("darray", "flecs_vec", "iterate", fl_vec_iterate);
}

#else
void bx_bench_register_flecs(void)
{
}
#endif
