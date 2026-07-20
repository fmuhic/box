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

echo
echo "Done. Re-run cmake so the build picks them up:"
echo "  ./perf.sh"
