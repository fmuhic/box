# Performance suite

Compares box's containers against each other and against the fastest
open-source C implementations, across insert / lookup / erase / iterate.

## Running

```sh
./perf.sh                          # everything, 200k elements
./perf.sh -f hmap                  # one group
./perf.sh -f lookup                # one operation across all groups
./perf.sh -n 2000000 -r 15         # bigger working set, more repetitions
./perf.sh --csv > results.csv      # machine-readable
```

`perf.sh` configures a Release build in `build-perf/`, compiles the suite at
`-O3 -DNDEBUG -march=native`, and runs it pinned to CPU 0. Every argument is
forwarded to the binary; `./perf.sh -h` lists them all.

## Enabling the third-party comparisons

The comparison libraries are **not vendored** — the suite runs box-only without
them and prints what is missing. To pull them in:

```sh
./performance/third_party/fetch.sh
./perf.sh
```

That downloads three single-header libraries into `performance/third_party/`,
which is gitignored:

| Library | Lang | Design | License |
|---|---|---|---|
| [khash](https://github.com/attractivechaos/klib) | C | quadratic probing, 2-bit metadata, tombstones | MIT |
| [Verstable](https://github.com/JacksonAllan/Verstable) | C | in-table chaining, 16-bit metadata, no tombstones | MIT |
| [STC](https://github.com/stclib/STC) | C | **Robin Hood**, 8-bit metadata, 0.85 load, 1.5x growth | MIT |
| [stb_ds](https://github.com/nothings/stb) | C | open addressing, separate key/value storage | public domain / MIT |
| [ankerl::unordered_dense](https://github.com/martinus/unordered_dense) | C++17 | **Robin Hood** + backward shift, buckets hold an index into a dense vector | MIT |

Two more come from real engines rather than container libraries:

| Library | Structure | Compared against |
|---|---|---|
| [flecs](https://github.com/SanderMertens/flecs) | `ecs_sparse_t`, `ecs_vec_t`, `ecs_map_t` | spset, darray, hmap |
| [Box2D v3](https://github.com/erincatto/box2d) | `b2BitSet`, `b2Array` | bitset, darray |

**flecs' sparse set is the only external comparison spset has.** Only the
amalgamated `flecs.c` is compiled, and it is the dominant cost of a perf build
once fetched (~2.5 MB of C).

Box2D's `b2BitSet` is near-identical to `bx_bitset` — same `uint64` blocks, same
`blockCapacity`/`blockCount`, same operation set. `b2Array` is the same idea as
`darray` down to the constants: an inline capacity check plus a typed store,
growing 2x from an initial 8. Only `src/bitset.c` and the header-only
`container.h` are compiled, against a shim `core.h` that `fetch.sh` writes.
`b2HashSet` is left out — it stores `uint64` keys with no values and exists to
track shape pairs, so it is not a map comparison.

The shim's `b2GrowAlloc` allocates, copies and frees rather than calling
`realloc`, matching what Box2D actually does. That detail is load-bearing:
`bx_darray_reserve` does the same, and substituting `realloc` would hand Box2D
an advantage the real engine does not have.

⚠️ **`flecs_map` rows are not like-for-like.** `ecs_map_t` accepts no hash
callback, so it is the one table that cannot be given the shared mixer. It
applies Fibonacci hashing, which spreads consecutive integers near-perfectly —
and this suite's keys are `1..n` shuffled, i.e. consecutive. Measured directly:
**2.4 ns/lookup on sequential keys against 8.6 ns on scattered keys.** Those
rows describe a table plus a hash on a favourable workload.

`std::unordered_map` and `std::vector` are also benchmarked and need no fetch —
only a C++ compiler. `std::unordered_map` is node-based separate chaining
because the standard mandates reference stability and a bucket interface, so
every element is a separate allocation; it is the slowest map design here by
construction, and it is included because it is the default most code reaches
for.

CMake detects whichever headers are present and compiles in only those
comparisons, so a partial fetch works fine.

**STC and ankerl are the load-bearing comparisons.** Both use Robin Hood, the
same scheme as box, so they separate two things the other libraries conflate: a
gap against them is a gap in box's implementation, while a gap they *share*
against Verstable is a property of Robin Hood itself.

### The C++ comparisons

`performance/CMakeLists.txt` calls `enable_language(CXX)` itself rather than the
root project declaring it. Since the perf directory is only added when
`BOX_BUILD_PERF=ON`, a normal `cmake -B build` never probes for a C++ compiler —
the library stays C99 with no C++ toolchain requirement.

C++ is enabled whenever a compiler exists, which is enough for the `std`
containers; `ankerl::unordered_dense` is an extra layered on top when its header
is present. With no C++ compiler at all, a stub replaces the C++ translation
unit and everything else still builds.

`bench.h` is wrapped in `extern "C"` so the one C++ file can share the harness.

## Getting numbers you can trust

The `spread` column is `(max-min)/median` across repetitions. **Above roughly
20% the ranking is noise, not signal.** To bring it down:

```sh
sudo cpupower frequency-set -g performance          # stop frequency scaling
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo   # stop thermal drift
```

`perf.sh` checks both and warns if they are unset. Beyond that: close other
work, raise `-r`, and compare `best` rather than `ns/op` when spread is high —
the fastest sample is the one least contaminated by interference.

For hardware counters, the binary works under `perf` directly:

```sh
perf stat -e cache-misses,branch-misses ./build-perf/performance/box_perf -f hmap/box
```

## Methodology

Modelled on the [C/C++ hash table
benchmark](https://jacksonallan.github.io/c_cpp_hash_tables_benchmark/), the
most thorough public comparison in this space.

**Keys** are `1..n` shuffled with a seeded Fisher-Yates. Sequential-but-shuffled
is the standard workload: raw sequential keys flatter tables whose hash barely
mixes, and pure random keys hide the clustering that real ID-like data causes.
Miss keys are drawn from above `n`, so they are guaranteed absent. The seed is
fixed, so two runs on the same machine are directly comparable.

**The hash function is held constant.** Every table under test is handed the
same MurmurHash3 finalizer, including khash and Verstable, whose defaults are
overridden. Without this the benchmark would be comparing hash functions rather
than tables — khash's built-in integer hash is the identity function, which
would flatter or wreck it depending on the key distribution. stb_ds is the one
exception: its hash is not overridable, which is noted here because it affects
how its rows should be read.

**Only the operation is timed.** Setup — allocation, filling the table for a
lookup benchmark — happens inside the case but outside its timer, so no result
includes work that is not the operation being measured.

**Results are sunk.** Every measured loop feeds its result through an inline-asm
barrier. Without it the optimizer deletes loops whose results are unused, and
the benchmark reports a few nanoseconds for work that never happened — the most
common way microbenchmarks lie.

**Aggregation** discards the fastest and slowest quarter of repetitions and
averages the rest, which is what the reference benchmark does to blunt
scheduling noise. `best` is reported alongside for when the trimmed mean is
still noisy.

**`insert` versus `insert_reserved`** separates the two costs inside insertion:
`insert` grows from empty and pays every rehash, `insert_reserved` sizes the
table up front and measures probing alone. The gap between them is the growth
policy's cost.

## What is not measured

- **Memory.** The suite reports time only. Peak RSS across repetitions is too
  noisy to be useful, and per-table accounting is not portable across the
  third-party libraries.
- **String keys.** Integer keys only. String workloads shift the balance toward
  tables whose metadata avoids full key comparisons.
- **hmap iteration.** box's hmap has no iteration API, so there is nothing to
  measure. spset and darray do have one and are benchmarked.
- **Concurrency.** Single-threaded throughout.
