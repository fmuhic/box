#!/bin/bash
# Builds and runs the performance suite. All arguments are forwarded to the
# benchmark binary, so `./perf.sh -f hmap -n 1000000` works.

set -e

BUILD_DIR="build-perf"
BOX_PERF_NATIVE="${BOX_PERF_NATIVE:-ON}"

if [ ! -d "performance/third_party" ] || \
   [ -z "$(ls -A performance/third_party/*.h 2>/dev/null)" ]; then
    echo "--- No third-party libraries found ---"
    echo "    Comparisons against khash / Verstable / stb_ds will be skipped."
    echo "    To enable them:  ./performance/third_party/fetch.sh"
    echo
fi

echo "--- Configuring ($BUILD_DIR) ---"
cmake -S . -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBOX_BUILD_PERF=ON \
      -DBOX_PERF_NATIVE="$BOX_PERF_NATIVE" \
      > /dev/null

echo "--- Building ---"
cmake --build "$BUILD_DIR" --target box_perf -j"$(nproc 2>/dev/null || echo 4)"

# Measurement hygiene. None of this is fatal; it is advisory so a noisy result
# is not mistaken for a real regression.
if [ -r /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
    GOV=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)
    if [ "$GOV" != "performance" ]; then
        echo
        echo "--- WARNING: cpu governor is '$GOV', not 'performance' ---"
        echo "    Frequency scaling will widen the spread column. To pin it:"
        echo "    sudo cpupower frequency-set -g performance"
    fi
fi
if [ -r /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
    if [ "$(cat /sys/devices/system/cpu/intel_pstate/no_turbo)" = "0" ]; then
        echo
        echo "--- NOTE: turbo boost is on; clocks drift as the CPU heats up ---"
        echo "    For the steadiest numbers:"
        echo "    echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo"
    fi
fi

# Pinning to one core removes migration between cores with cold caches.
RUNNER=""
if command -v taskset >/dev/null 2>&1; then
    RUNNER="taskset -c 0"
    echo
    echo "--- Pinning to CPU 0 via taskset ---"
fi

echo
$RUNNER "./$BUILD_DIR/performance/box_perf" "$@"
