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
./perf.sh --repeat-process 10      # re-run the binary 10x, aggregate
```

`perf.sh` configures a Release build in `build-perf/`, compiles the suite at
`-O3 -DNDEBUG -march=native`, and runs it pinned to a middle physical core (and
its SMT sibling) rather than CPU 0. Every argument is forwarded to the binary
(`./perf.sh -h` lists them all) except `--repeat-process K`, which `perf.sh`
consumes itself.

On a bare-metal Linux box, pin the clock first for steadier numbers; `perf.sh`
warns if either is unset (and notes when it is running under a VM, where neither
knob is reachable):

```sh
sudo cpupower frequency-set -g performance
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo
```

## Reading the output

| column   | meaning |
|----------|---------|
| `ns/op`  | time per element, trimmed mean of the repetitions |
| `best`   | fastest single repetition — least contaminated when `noise` is high |
| `noise`  | relative spread of the repetitions; above ~5% treat nearby rows as tied |
| `vs box` | ratio to the `box` row for the same op; `>1` slower, `<1` faster |

`--repeat-process K` re-runs the whole binary K times and reports the median
across processes plus a `drift` column (spread across runs). A difference
between two runs smaller than `drift` is layout noise, not a result.

## Enabling the third-party comparisons

The comparison libraries are **not vendored** — the suite runs box-only without
them and prints what is missing. To pull them in:

```sh
./performance/third_party/fetch.sh
./perf.sh
```

CMake compiles in only the headers that are present, so a partial fetch works.
`std::unordered_map` and `std::vector` need no fetch, only a C++ compiler.

Single-header libraries fetched into `performance/third_party/` (gitignored):

| Library | Lang | Design | License |
|---|---|---|---|
| [khash](https://github.com/attractivechaos/klib) | C | quadratic probing, 2-bit metadata, tombstones | MIT |
| [Verstable](https://github.com/JacksonAllan/Verstable) | C | in-table chaining, 16-bit metadata, no tombstones | MIT |
| [STC](https://github.com/stclib/STC) | C | Robin Hood, 8-bit metadata, 0.85 load, 1.5x growth | MIT |
| [stb_ds](https://github.com/nothings/stb) | C | open addressing, separate key/value storage | public domain / MIT |
| [ankerl::unordered_dense](https://github.com/martinus/unordered_dense) | C++17 | Robin Hood + backward shift over a dense vector | MIT |

Two more come from real engines rather than container libraries:

| Library | Structure | Compared against |
|---|---|---|
| [flecs](https://github.com/SanderMertens/flecs) | `ecs_sparse_t`, `ecs_vec_t`, `ecs_map_t` | spset, darray, hmap |
| [Box2D v3](https://github.com/erincatto/box2d) | `b2BitSet`, `b2Array` | bitset, darray |

⚠️ **`flecs_map` rows are not like-for-like.** `ecs_map_t` accepts no hash
callback, so it is the one table that cannot be handed the shared mixer every
other table gets. Its Fibonacci hashing happens to suit this suite's
consecutive keys, so those rows flatter it.

## Verifying the darray stays inlined

The darray core is `static inline` in the header on purpose: moving any of it
out of line costs roughly 40% of push throughput, and **every test still
passes** when you do (see the darray invariant in the top-level `CLAUDE.md`).
The test suite cannot catch that regression; the disassembly can. After a perf
build, `box_push` should contain no call back into the darray core, and no
out-of-line `bx_darray_*` symbol should be emitted at all:

```sh
# Want: 0. Any call to a bx_darray_* / *_grow symbol means it went out of line.
objdump -d build-perf/performance/box_perf \
  | awk '/<box_push>:/{f=1} f&&/^$/{f=0} f' \
  | grep -cE 'call.*(bx_darray|_grow)'

# Want: only bx_bench_register_darray. Any other bx_darray_* entry should have
# been inlined but was compiled standalone.
objdump -d build-perf/performance/box_perf | grep -E '<.*darray.*>:'
```
