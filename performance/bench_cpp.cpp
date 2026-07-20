// C++ comparisons. This is the only C++ translation unit in the project, and it
// is compiled only when BOX_BUILD_PERF is on -- the library itself stays C99 and
// the main build never asks for a C++ compiler.
//
// ankerl::unordered_dense is Robin Hood with backward-shift deletion, the same
// scheme as box and STC, but it keeps only (dist, fingerprint, index) in the
// bucket and stores the real data in a dense vector. That is the design point
// worth measuring: Robin Hood shuffling moves 8-byte buckets instead of whole
// entries, and iteration is a flat scan.

#include "bench.h"

#ifdef BOX_PERF_HAS_UNORDERED_DENSE
#include "unordered_dense.h"
#endif

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace
{

// unordered_dense distinguishes high-quality hashes from weak ones and skips
// its own mixing when the hash already avalanches. The MurmurHash3 finalizer
// does, and declaring it keeps every table on the same hash.
struct BenchHash
{
    using is_avalanching = void;
    uint64_t operator()(uint32_t key) const noexcept
    {
        return bx_bench_hash_u32(key);
    }
};

// std::unordered_map wants size_t and has no avalanching trait.
struct StdHash
{
    size_t operator()(uint32_t key) const noexcept
    {
        return static_cast<size_t>(bx_bench_hash_u32(key));
    }
};

using StdMap = std::unordered_map<uint32_t, float, StdHash>;

// ---------------------------------------------------------------------------
// std::unordered_map
//
// The standard mandates bucket interface and reference stability, which forces
// node-based separate chaining: every element is a separate allocation and a
// lookup follows a pointer. It is the slowest design here by construction, and
// it is included precisely because it is what most C++ code actually uses.
// ---------------------------------------------------------------------------

double stdmap_insert(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    StdMap m;

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        m[keys[i]] = static_cast<float>(i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(m.size());
    return t1 - t0;
}

double stdmap_insert_reserved(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    StdMap m;
    m.reserve(n);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        m[keys[i]] = static_cast<float>(i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(m.size());
    return t1 - t0;
}

double stdmap_lookup_hit(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    StdMap m;
    for (uint32_t i = 0; i < n; i++)
    {
        m[keys[i]] = static_cast<float>(i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        auto it = m.find(keys[i]);
        BX_BENCH_SINK(it == m.end());
    }
    double t1 = bx_bench_now();

    return t1 - t0;
}

double stdmap_lookup_miss(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    StdMap m;
    for (uint32_t i = 0; i < n; i++)
    {
        m[keys[i]] = static_cast<float>(i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        auto it = m.find(misses[i]);
        BX_BENCH_SINK(it == m.end());
    }
    double t1 = bx_bench_now();

    return t1 - t0;
}

double stdmap_erase(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    StdMap m;
    for (uint32_t i = 0; i < n; i++)
    {
        m[keys[i]] = static_cast<float>(i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        m.erase(keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(m.size());
    return t1 - t0;
}

double stdmap_replace(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    StdMap m;
    for (uint32_t i = 0; i < n; i++)
    {
        m[keys[i]] = static_cast<float>(i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        m[keys[i]] = static_cast<float>(i + 1);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(m.size());
    return t1 - t0;
}

// ---------------------------------------------------------------------------
// std::vector
// ---------------------------------------------------------------------------

double stdvec_push(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    std::vector<uint32_t> v;

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        v.push_back(keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(v.size());
    return t1 - t0;
}

double stdvec_push_reserved(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    std::vector<uint32_t> v;
    // Page the buffer in before timing; see box_push_reserved in bench_darray.c.
    // reserve() cannot be pre-touched legally -- the capacity past size() is
    // unconstructed -- so resize(n) constructs and faults n elements, then
    // clear() drops the size back to 0 while keeping that capacity resident.
    v.resize(n);
    v.clear();

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        v.push_back(keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(v.size());
    return t1 - t0;
}

// Steady-state push into a reused, paged-in vector; see box_push_warm.
double stdvec_push_warm(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    static std::vector<uint32_t> v;
    if (v.capacity() < n)
    {
        v.resize(n); // constructs + faults the pages
    }
    v.clear(); // size 0, capacity and resident pages kept

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        v.push_back(keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(v.size());
    return t1 - t0;
}

double stdvec_iterate(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    std::vector<uint32_t> v;
    v.reserve(n);
    for (uint32_t i = 0; i < n; i++)
    {
        v.push_back(keys[i]);
    }

    double t0 = bx_bench_now();
    uint64_t sum = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        sum += v[i];
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(sum);
    return t1 - t0;
}

double stdvec_random_get(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    std::vector<uint32_t> v;
    v.reserve(n);
    for (uint32_t i = 0; i < n; i++)
    {
        v.push_back(keys[i]);
    }

    double t0 = bx_bench_now();
    uint64_t sum = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        sum += v[keys[i] % n];
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(sum);
    return t1 - t0;
}

double stdvec_pop(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    std::vector<uint32_t> v;
    v.reserve(n);
    for (uint32_t i = 0; i < n; i++)
    {
        v.push_back(keys[i]);
    }

    double t0 = bx_bench_now();
    uint64_t sum = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        sum += v.back();
        v.pop_back();
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(sum);
    return t1 - t0;
}

#ifdef BOX_PERF_HAS_UNORDERED_DENSE
using DenseMap = ankerl::unordered_dense::map<uint32_t, float, BenchHash>;

double dense_insert(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    DenseMap m;

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        m[keys[i]] = static_cast<float>(i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(m.size());
    return t1 - t0;
}

double dense_insert_reserved(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    DenseMap m;
    m.reserve(n);

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        m[keys[i]] = static_cast<float>(i);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(m.size());
    return t1 - t0;
}

double dense_lookup_hit(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    DenseMap m;
    for (uint32_t i = 0; i < n; i++)
    {
        m[keys[i]] = static_cast<float>(i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        auto it = m.find(keys[i]);
        BX_BENCH_SINK(it == m.end());
    }
    double t1 = bx_bench_now();

    return t1 - t0;
}

double dense_lookup_miss(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    DenseMap m;
    for (uint32_t i = 0; i < n; i++)
    {
        m[keys[i]] = static_cast<float>(i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        auto it = m.find(misses[i]);
        BX_BENCH_SINK(it == m.end());
    }
    double t1 = bx_bench_now();

    return t1 - t0;
}

double dense_erase(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    DenseMap m;
    for (uint32_t i = 0; i < n; i++)
    {
        m[keys[i]] = static_cast<float>(i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        m.erase(keys[i]);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(m.size());
    return t1 - t0;
}

double dense_replace(const uint32_t* keys, const uint32_t* misses, uint32_t n)
{
    (void)misses;
    DenseMap m;
    for (uint32_t i = 0; i < n; i++)
    {
        m[keys[i]] = static_cast<float>(i);
    }

    double t0 = bx_bench_now();
    for (uint32_t i = 0; i < n; i++)
    {
        m[keys[i]] = static_cast<float>(i + 1);
    }
    double t1 = bx_bench_now();

    BX_BENCH_SINK(m.size());
    return t1 - t0;
}

#endif // BOX_PERF_HAS_UNORDERED_DENSE

} // namespace

extern "C" void bx_bench_register_cpp(void)
{
    bx_bench_add("hmap", "std_unord_map", "insert", stdmap_insert);
    bx_bench_add("hmap", "std_unord_map", "insert_reserved", stdmap_insert_reserved);
    bx_bench_add("hmap", "std_unord_map", "lookup_hit", stdmap_lookup_hit);
    bx_bench_add("hmap", "std_unord_map", "lookup_miss", stdmap_lookup_miss);
    bx_bench_add("hmap", "std_unord_map", "erase", stdmap_erase);
    bx_bench_add("hmap", "std_unord_map", "replace", stdmap_replace);

    bx_bench_add("darray", "std_vector", "push", stdvec_push);
    bx_bench_add("darray", "std_vector", "push_reserved", stdvec_push_reserved);
    bx_bench_add("darray", "std_vector", "push_warm", stdvec_push_warm);
    bx_bench_add("darray", "std_vector", "iterate", stdvec_iterate);
    bx_bench_add("darray", "std_vector", "random_get", stdvec_random_get);
    bx_bench_add("darray", "std_vector", "pop", stdvec_pop);

#ifdef BOX_PERF_HAS_UNORDERED_DENSE
    bx_bench_add("hmap", "ankerl_dense", "insert", dense_insert);
    bx_bench_add("hmap", "ankerl_dense", "insert_reserved", dense_insert_reserved);
    bx_bench_add("hmap", "ankerl_dense", "lookup_hit", dense_lookup_hit);
    bx_bench_add("hmap", "ankerl_dense", "lookup_miss", dense_lookup_miss);
    bx_bench_add("hmap", "ankerl_dense", "erase", dense_erase);
    bx_bench_add("hmap", "ankerl_dense", "replace", dense_replace);
#endif
}
