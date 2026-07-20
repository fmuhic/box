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
`-O3 -DNDEBUG -march=native`, and runs it pinned to a middle physical core (and
its SMT sibling) rather than CPU 0, which carries the interrupt load. Every
argument is forwarded to the binary; `./perf.sh -h` lists them all.

`--repeat-process K` is the exception — `perf.sh` consumes it rather than
forwarding it. See *Run-to-run variance* below.

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

The shim's `b2GrowAlloc` calls `realloc`, which is *not* what Box2D does — the
real one allocates, copies and frees, because it routes through the engine's
arena allocator. The shim deviates deliberately so that it matches
`bx_darray_set_capacity`: with both sides on `realloc`, the `push` rows measure
per-element container overhead rather than allocator strategy, which is the
comparison worth making between two array implementations.

Worth knowing what that hides. `realloc` lets glibc remap pages via `mremap`
instead of copying on every doubling, and darray used to allocate-copy-free like
Box2D does. Switching it was the single largest win in this suite:

| n | allocate-copy-free | `realloc` | |
|---|---|---|---|
| 10,000 | 0.339 | 0.265 | 1.28x |
| 200,000 | 3.959 | 0.256 | **15.5x** |
| 2,000,000 | 3.565 | 0.255 | **14.0x** |

Because the shim now matches, that gap no longer appears in the table — both
sides sit at ~0.25 ns/op. The `raw_realloc` row is what keeps it visible.

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

The `noise` column is the median absolute deviation over the repetitions,
divided by the median. It measures how much the *typical* sample deviates, so a
single descheduled repetition barely moves it — unlike the old `(max-min)/median`
spread, which one preempted sample could swing by hundreds of percent while the
reported figure had not changed. **Above roughly 5% treat the ranking of nearby
rows as unresolved on that run.** To bring it down:

```sh
sudo cpupower frequency-set -g performance          # stop frequency scaling
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo   # stop thermal drift
```

`perf.sh` checks both and warns if they are unset. Beyond that: close other
work, raise `-r`, and compare `best` rather than `ns/op` when noise is high —
the fastest sample is the one least contaminated by interference.

**Under WSL2 (and most VMs) neither knob is reachable** — the `cpufreq` and
`intel_pstate` sysfs trees do not exist in the guest, and frequency, turbo, and
physical-core placement all belong to the host. `perf.sh` detects this and says
so instead of reporting a clean check it never ran. There is no fix from inside
the guest; use `--repeat-process` to measure how much this costs you and treat
anything inside that floor as unresolvable.

### Run-to-run variance

The `noise` column only sees *within-process* variance. Every repetition in one
process shares a heap layout, a binary mapping, and one set of ASLR offsets, and
those shift timings by a few percent while staying fixed for the life of the
process — so no amount of `-r` exposes them. Re-running the binary is the only
way to resample that component, and it is usually the larger one.

```sh
./perf.sh --repeat-process 10 -f darray     # 10 processes, aggregated
```

This runs the binary ten times and reports, per row, the **median across
processes** and a `drift` column: the max-min across processes over that median.
`drift` is the real floor. **A difference between two runs smaller than the
worst-row drift is not a result** — it is layout noise. Use it to decide whether
a change you made actually moved anything: if the delta is under the floor, you
have not measured a difference.

`drift` deliberately uses the extremes, not the MAD: a slow layout is an outcome
you can genuinely land on from one run to the next, not a scheduling artefact to
trim away.

For hardware counters, the binary works under `perf` directly:

```sh
perf stat -e cache-misses,branch-misses ./build-perf/performance/box_perf -f hmap/box
```

### Verifying the darray stays inlined

The darray core is `static inline` in the header on purpose: moving any of it
out of line costs roughly 40% of push throughput, and **every test still
passes** when you do (see the darray invariant in the top-level `CLAUDE.md`).
The test suite cannot catch that regression; the disassembly can. After a perf
build, confirm that `box_push` contains no call back into the darray core and
that no out-of-line `bx_darray_*` symbol was emitted at all:

```sh
# Want: 0. Any call to a bx_darray_* / *_grow symbol means it went out of line.
objdump -d build-perf/performance/box_perf \
  | awk '/<box_push>:/{f=1} f&&/^$/{f=0} f' \
  | grep -cE 'call.*(bx_darray|_grow)'

# Want: only bx_bench_register_darray. Any bx_darray_* entry here is a function
# that should have been inlined but was compiled standalone.
objdump -d build-perf/performance/box_perf | grep -E '<.*darray.*>:'
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
still noisy, and `noise` (relative MAD) qualifies how much to trust the row.

**Cases are interleaved, not batched.** Each pass runs every case once before
the next pass begins, rather than all repetitions of a case back to back. Drift
over a run — thermal, background load, allocator arena warming — then lands on
every case equally and cancels out of the `vs box` comparison, instead of
biasing whichever case happened to be running while the machine changed. A full
untimed warmup pass over all cases precedes the first measured pass, so no case
is timed cold merely because it registered first. One consequence: a filtered
run (`-f hmap`) warms fewer cases than the full suite and can report slightly
different absolute numbers for the same row — the comparison within a run is
unaffected, but do not compare absolute ns/op across different filters.

**Whole-set operations are batched to clear the timer.** `popcount` and `union`
are a single O(n) call, far too short to bracket with two clock reads directly —
the reads would be a large fraction of the measurement. `BX_BENCH_TIME_REPEATED`
runs the operation enough times to exceed a 20 ms floor and divides back out.

**`insert` versus `insert_reserved`** separates the two costs inside insertion:
`insert` grows from empty and pays every rehash, `insert_reserved` sizes the
table up front and measures probing alone. The gap between them is the growth
policy's cost.

**The three darray `push` rows** separate what a push costs in three regimes:

- `push` — grow from empty. Pays every reallocation and the first-touch page
  faults of a buffer built during the timed loop. This is the cold "build a
  darray from scratch" cost, and it carries the group's highest run-to-run
  drift, because that first-touch and reallocation cost is really the
  allocator's, and it swings with heap layout.
- `push_reserved` — size the buffer up front, then fill it. The buffer is
  **paged in before the timer** (a `memset` after the reserve), so this is the
  no-growth store cost with the allocator's first-touch faults kept out. Without
  that pre-fault the row measured mostly the fault-in of a fresh mapping, which
  made it noisier than — and sometimes slower than — `push` itself, which is
  backwards for a row that does strictly less work.
- `push_warm` — one buffer, paged in once, then `clear()`ed and refilled every
  repetition: the game-loop pattern where a darray is kept across frames rather
  than reallocated. With allocation and paging out of the loop entirely this is
  the steady-state throughput, and it has by far the lowest drift, which makes
  it the row to watch for a real regression. The `raw_realloc` `push_warm` row
  is the memory-bandwidth ceiling: with no per-element capacity check it
  vectorizes to bulk stores and runs several times faster than any real push —
  the gap is the price of the bounds check every dynamic array pays, not slack
  in box. `push_warm` reads a touch slower than `push_reserved` only because the
  reused buffer must be `static`, which costs a little codegen; it hits every
  library's row equally, so the comparison across them stays fair.

## What is not measured

- **Memory.** The suite reports time only. Peak RSS across repetitions is too
  noisy to be useful, and per-table accounting is not portable across the
  third-party libraries.
- **String keys.** Integer keys only. String workloads shift the balance toward
  tables whose metadata avoids full key comparisons.
- **hmap iteration.** box's hmap has no iteration API, so there is nothing to
  measure. spset and darray do have one and are benchmarked.
- **Concurrency.** Single-threaded throughout.
