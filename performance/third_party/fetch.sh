#!/bin/bash
# Downloads the single-header libraries the performance suite compares against.
# Nothing here is vendored into the repository; this directory is gitignored.
#
# All three are permissively licensed (khash/Verstable MIT, stb_ds public domain
# or MIT). They are downloaded, not redistributed.

set -e
cd "$(dirname "$0")"

fetch()
{
    local name="$1" url="$2" file="$3"
    if [ -f "$file" ]; then
        echo "  $name: already present, skipping"
        return
    fi
    echo "  $name: downloading"
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "$url" -o "$file"
    elif command -v wget >/dev/null 2>&1; then
        wget -q "$url" -O "$file"
    else
        echo "  !! neither curl nor wget found" >&2
        exit 1
    fi
}

echo "Fetching third-party hash table implementations..."
fetch "khash (klib)" \
      "https://raw.githubusercontent.com/attractivechaos/klib/master/khash.h" \
      "khash.h"
fetch "Verstable" \
      "https://raw.githubusercontent.com/JacksonAllan/Verstable/main/verstable.h" \
      "verstable.h"
fetch "stb_ds" \
      "https://raw.githubusercontent.com/nothings/stb/master/stb_ds.h" \
      "stb_ds.h"

# C++17. Only compiled if a C++ compiler is available; see performance/CMakeLists.txt.
# Ships as two headers, not one: unordered_dense.h includes stl.h as a sibling.
fetch "ankerl::unordered_dense" \
      "https://raw.githubusercontent.com/martinus/unordered_dense/main/include/ankerl/unordered_dense.h" \
      "unordered_dense.h"
fetch "ankerl::unordered_dense (stl.h)" \
      "https://raw.githubusercontent.com/martinus/unordered_dense/main/include/ankerl/stl.h" \
      "stl.h"

# STC ships a directory of headers rather than one file, so it arrives as a
# tarball and only include/stc is kept.
if [ -d "stc" ]; then
    echo "  STC: already present, skipping"
else
    echo "  STC: downloading"
    tmp=$(mktemp -d)
    trap 'rm -rf "$tmp"' EXIT
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "https://github.com/stclib/STC/archive/refs/heads/main.tar.gz" \
             -o "$tmp/stc.tar.gz"
    else
        wget -q "https://github.com/stclib/STC/archive/refs/heads/main.tar.gz" \
             -O "$tmp/stc.tar.gz"
    fi
    tar -xzf "$tmp/stc.tar.gz" -C "$tmp"
    mv "$tmp"/STC-main/include/stc ./stc
fi

# Box2D: only bitset.c and its two headers are needed. Box2D's own core.h pulls
# in the whole engine, so a shim providing just the three things bitset.c uses
# is written here instead of vendoring it.
if [ -d "box2d" ]; then
    echo "  Box2D (bitset): already present, skipping"
else
    echo "  Box2D (bitset): downloading"
    mkdir -p box2d
    for f in bitset.h bitset.c ctz.h container.h; do
        fetch "  box2d/$f" "https://raw.githubusercontent.com/erincatto/box2d/main/src/$f" "box2d/$f"
    done
    cat > box2d/core.h <<'SHIM'
// Not from Box2D. Shim supplying the only three things src/bitset.c needs from
// Box2D's core.h, so the file compiles without the rest of the engine.
#pragma once
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define B2_ASSERT(x) assert(x)
#define B2_UNUSED(...) (void)sizeof(__VA_ARGS__)
#define B2_RESTRICT restrict
#define B2_INLINE static inline
#define B2_NULL_INDEX (-1)
static inline void* b2Alloc(int size) { return malloc((size_t)size); }
static inline void b2Free(void* mem, int size) { (void)size; free(mem); }

// Box2D's own b2GrowAlloc allocates, copies and frees rather than calling
// realloc, because it routes through its arena allocator. Reproducing that
// shape matters: bx_darray_reserve does the same, so swapping in realloc here
// would hand Box2D an advantage the real engine does not have.
static inline void* b2GrowAlloc(void* oldMem, size_t oldSize, size_t newSize)
{
    void* mem = malloc(newSize);
    if (oldMem != NULL)
    {
        memcpy(mem, oldMem, oldSize);
        free(oldMem);
    }
    return mem;
}
SHIM
fi

# flecs: the amalgamated distribution. Large (~2.5 MB of C) and it noticeably
# lengthens the perf build, but it is the only source of a sparse-set comparison.
fetch "flecs (header)" \
      "https://raw.githubusercontent.com/SanderMertens/flecs/master/distr/flecs.h" \
      "flecs.h"
fetch "flecs (source)" \
      "https://raw.githubusercontent.com/SanderMertens/flecs/master/distr/flecs.c" \
      "flecs.c"

echo
echo "Done. Re-run cmake so the build picks them up:"
echo "  ./perf.sh"
