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

echo
echo "Done. Re-run cmake so the build picks them up:"
echo "  ./perf.sh"
