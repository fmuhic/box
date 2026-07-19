# box

Type-safe generic data structures for C99.

Each container is a typeless core compiled once, plus a macro that generates
`static inline` typed wrappers. You get type checking without templates and
without recompiling the logic per type.

---

## Build

```sh
cmake -B build && cmake --build build
```

Link against `box` and include `box/<container>.h`.

---

## Conventions

Every container follows the same rules.

| | |
|---|---|
| Declare | `BX_<NAME>_DECLARE(...)` at file scope, once per type |
| Create | `_init(c)` — never allocates |
| Create sized | `_init_capacity(c, n)` — allocates up front |
| Destroy | `_drop(c)` — frees; safe to `_init` again after |
| Empty | `_clear(c)` — keeps the allocation |
| Query | `_size(c)`, `_capacity(c)` |

A zeroed struct is a valid empty container. Counts and indices are `uint32_t`.
`_get` returns a pointer into the container, or `NULL` if absent — that pointer
is invalidated by any insert that grows the container.

---

## darray

A growable array. Elements are stored contiguously in insertion order.

```
push(30)

  data ──► ┌──────┬──────┬──────┬╌╌╌╌╌╌┬╌╌╌╌╌╌┐
           │  10  │  20  │  30  │      │      │
           └──────┴──────┴──────┴╌╌╌╌╌╌┴╌╌╌╌╌╌┘
              0      1      2
                                 size = 3   capacity = 5

remove_swap(0)                   last element fills the hole

  data ──► ┌──────┬──────┬╌╌╌╌╌╌┬╌╌╌╌╌╌┬╌╌╌╌╌╌┐
           │  30  │  20  │      │      │      │
           └──────┴──────┴╌╌╌╌╌╌┴╌╌╌╌╌╌┴╌╌╌╌╌╌┘
                                 size = 2   capacity = 5
```

`BX_DARRAY_DECLARE(T, NAME)`

| Operation | Cost | |
|---|---|---|
| `push(a, v)` | O(1)* | append `v` |
| `emplace(a)` | O(1)* | append uninitialized, return `T*` |
| `pop(a)` | O(1) | remove and return the last element |
| `get(a, i)` | O(1) | `T*` at index `i` |
| `remove_swap(a, i)` | O(1) | erase index `i`, moving the last element into it |
| `resize(a, n)` | O(n) | set size to `n` |
| `reserve(a, n)` | O(n) | grow capacity to `n` |

\* amortized; capacity doubles when full.

`remove_swap` does not preserve order.

```c
BX_DARRAY_DECLARE(int, int)

bx_darray_int a;
bx_darray_int_init(&a);
bx_darray_int_push(&a, 10);
*bx_darray_int_get(&a, 0) = 99;
bx_darray_int_drop(&a);
```

---

## hmap

A hash map. Open addressing with Robin Hood probing and backward-shift
deletion — no tombstones. A parallel `meta` array stores, per bucket, a 6-bit
partial hash and the distance from that key's ideal bucket (`0` = empty), so
most probe steps reject a bucket without touching the key.

```
"cat" and "dog" both hash to bucket 1.
"cat" arrives first and takes it, so "dog" probes forward to bucket 2.

                         ideal bucket
                          for both keys
                                │
                                ▼
  bucket          0        1        2        3
                ┌────────┬────────┬────────┬────────┐
  table         │        │  cat   │  dog   │        │
                │        │   10   │   20   │        │
                └────────┴────────┴────────┴────────┘
                ┌────────┬────────┬────────┬────────┐
  meta.dist     │   0    │   1    │   2    │   0    │
                └────────┴────────┴────────┴────────┘
                  empty   at home   1 past   empty
                                    ideal
```

`dist` is how far a key sits from its ideal bucket, plus one. A lookup walks
forward only while `dist` is at least the number of steps taken, so a miss
stops early instead of scanning the table.

`BX_HMAP_DECLARE(K, V, NAME, HASH_FN, EQ_FN)`

`HASH_FN(K) -> uint64_t` and `EQ_FN(K, K) -> bool` take keys by value.

| Operation | Cost | |
|---|---|---|
| `insert(m, k, v)` | O(1)* | insert, or overwrite the value if `k` exists |
| `get(m, k)` | O(1) | `V*`, or `NULL` |
| `contains(m, k)` | O(1) | |
| `erase(m, k)` | O(1) | no-op if absent |
| `reserve(m, n)` | O(n) | fit `n` elements without rehashing |
| `bucket_count(m)` | O(1) | |

\* amortized; rehashes at 80% load.

`capacity` counts elements, not buckets: `init_capacity(m, 100)` allocates 256
buckets. Bucket count is always a power of two.

```c
static uint64_t hash_i32(int32_t k) { return (uint64_t)k * 0x9e3779b97f4a7c15ULL; }
static bool     eq_i32(int32_t a, int32_t b) { return a == b; }

BX_HMAP_DECLARE(int32_t, float, i32f, hash_i32, eq_i32)

bx_hmap_i32f m;
bx_hmap_i32f_init(&m);
bx_hmap_i32f_insert(&m, 7, 1.5f);
float* v = bx_hmap_i32f_get(&m, 7);
bx_hmap_i32f_drop(&m);
```

---

## spset

A sparse set: a map from `uint32_t` ID to a value, where the values live in a
packed array you can iterate linearly.

Three arrays. `dense` holds the values, `ids` holds each value's ID at the same
index, and `sparse` maps an ID back to its dense index. `sparse` is paged —
1024 entries per page, allocated only when an ID lands in that page.

```
Holding ID 5 -> A and ID 1500 -> B.

An ID splits into a page and a slot inside that page:

    ID 1500  ──►  page = 1500 >> 10 = 1,  slot = 1500 & 1023 = 476

  sparse — one page per 1024 IDs, allocated only when first used

    page 0  ┌ ─ ─ ─ ┬───────┬ ─ ─ ─ ─ ─ ─ ─ ─ ┐
            │  ~0   │   0   │  ~0   ...       │   slot 5   -> dense 0
            └ ─ ─ ─ ┴───╥───┴ ─ ─ ─ ─ ─ ─ ─ ─ ┘
                        ║
    page 1  ┌ ─ ─ ─ ─ ─ ║ ─ ┬───────┬ ─ ─ ─ ─ ┐
            │  ~0  ...  ║   │   1   │   ~0    │   slot 476 -> dense 1
            └ ─ ─ ─ ─ ─ ║ ─ ┴───╥───┴ ─ ─ ─ ─ ┘
                        ║       ║                 ~0 = empty slot
                        ▼       ▼
                ┌───────────┬───────────┐
    dense       │     A     │     B     │   values, packed for iteration
                ├───────────┼───────────┤
    ids         │     5     │   1500    │   the ID owning each slot
                └───────────┴───────────┘
                      0           1
```

`ids` is what makes a lookup safe: a stale `sparse` slot is rejected because
the ID stored at that dense index will not match.

`BX_SPSET_DECLARE(T, NAME)`

| Operation | Cost | |
|---|---|---|
| `insert(s, id, v)` | O(1)* | returns `false` if `id` existed (value overwritten) |
| `get(s, id)` | O(1) | `T*`, or `NULL` |
| `contains(s, id)` | O(1) | |
| `erase(s, id)` | O(1) | swap-and-pop; no-op if absent |
| `data(s)` | O(1) | `T*` to the packed values, `size` long |
| `ids(s)` | O(1) | `uint32_t*` to the matching IDs |
| `reserve(s, n)` | O(n) | fit `n` elements with IDs in `[0, n)` |
| `reserve_ids(s, id)` | O(n) | fit IDs up to `id` |

\* amortized.

`erase` does not preserve order. Iterate with `data()` and `ids()`:

```c
BX_SPSET_DECLARE(float, hp)

bx_spset_hp s;
bx_spset_hp_init(&s);
bx_spset_hp_insert(&s, 1500, 42.0f);

float*    v = bx_spset_hp_data(&s);
uint32_t* k = bx_spset_hp_ids(&s);
for (uint32_t i = 0; i < bx_spset_hp_size(&s); i++)
    printf("%u -> %f\n", k[i], v[i]);

bx_spset_hp_drop(&s);
```

---

## bitset

A growable array of bits, packed 64 to a `uint64_t` block. Not generic — use
`bx_bitset` directly.

```
set_safe(70)

   block 0                    block 1
  ┌─────────────────────────┬─────────────────────────┐
  │ 0 0 0 0 0 0 ... 0 0 0 0 │ 0 0 0 0 0 0 1 0 ... 0 0 │
  └─────────────────────────┴─────────────────────────┘
    bits 0 .. 63               bits 64 .. 127
                                           ▲
                                        bit 70
                                    block 70/64 = 1
                                    offset 70%64 = 6
```

| Operation | Cost | |
|---|---|---|
| `bx_bitset_set_fast(s, i)` | O(1) | set bit `i`; asserts `i` is in range |
| `bx_bitset_set_safe(s, i)` | O(1)* | set bit `i`, growing if needed |
| `bx_bitset_unset(s, i)` | O(1) | clear bit `i`; ignores out-of-range |
| `bx_bitset_get(s, i)` | O(1) | `false` if out of range |
| `bx_bitset_popcount(s)` | O(n) | number of bits set |
| `bx_bitset_union(a, b)` | O(n) | `a |= b`; both must be the same length |
| `bx_bitset_set_count_and_clear(s, n)` | O(n) | resize to `n` bits, all zero |
| `bx_bitset_grow_blocks(s, n)` | O(n) | grow to `n` **blocks**, preserving bits |

\* amortized.

`init_capacity` takes **bits**; `grow_blocks` takes **blocks** (1 block = 64
bits). Bit count must be set with `set_count_and_clear` or `set_safe` before
`set_fast` and `popcount` see anything.

```c
bx_bitset s;
bx_bitset_init(&s);
bx_bitset_set_safe(&s, 70);
bool on = bx_bitset_get(&s, 70);
bx_bitset_drop(&s);
```

---

## Example

```c
#include <stdio.h>
#include "box/darray.h"
#include "box/hmap.h"
#include "box/spset.h"
#include "box/bitset.h"

typedef struct Vec2
{
    float x, y;
} Vec2;

static uint64_t hash_u32(uint32_t k) { return (uint64_t)k * 0x9e3779b97f4a7c15ULL; }
static bool     eq_u32(uint32_t a, uint32_t b) { return a == b; }

BX_DARRAY_DECLARE(int32_t, i32)
BX_HMAP_DECLARE(uint32_t, float, u32f, hash_u32, eq_u32)
BX_SPSET_DECLARE(Vec2, pos)

int main(void)
{
    bx_darray_i32 scores;
    bx_darray_i32_init(&scores);
    bx_darray_i32_push(&scores, 10);
    bx_darray_i32_push(&scores, 20);
    int32_t last = bx_darray_i32_pop(&scores);
    printf("darray: size=%u last=%d\n", bx_darray_i32_size(&scores), last);
    bx_darray_i32_drop(&scores);

    bx_hmap_u32f health;
    bx_hmap_u32f_init(&health);
    bx_hmap_u32f_insert(&health, 42, 87.5f);
    printf("hmap:   id 42 -> %.1f\n", *bx_hmap_u32f_get(&health, 42));
    bx_hmap_u32f_drop(&health);

    bx_spset_pos positions;
    bx_spset_pos_init(&positions);
    bx_spset_pos_insert(&positions, 7, (Vec2){ 1.0f, 2.0f });
    bx_spset_pos_insert(&positions, 1500, (Vec2){ 3.0f, 4.0f });

    Vec2*     values = bx_spset_pos_data(&positions);
    uint32_t* ids    = bx_spset_pos_ids(&positions);
    for (uint32_t i = 0; i < bx_spset_pos_size(&positions); i++)
    {
        printf("spset:  id %u -> (%.1f, %.1f)\n", ids[i], values[i].x, values[i].y);
    }
    bx_spset_pos_drop(&positions);

    bx_bitset alive;
    bx_bitset_init(&alive);
    bx_bitset_set_safe(&alive, 7);
    bx_bitset_set_safe(&alive, 70);
    printf("bitset: %u set, bit 70 = %d\n",
           bx_bitset_popcount(&alive), bx_bitset_get(&alive, 70));
    bx_bitset_drop(&alive);

    return 0;
}
```

Output:

```
darray: size=1 last=20
hmap:   id 42 -> 87.5
spset:  id 7 -> (1.0, 2.0)
spset:  id 1500 -> (3.0, 4.0)
bitset: 2 set, bit 70 = 1
```
