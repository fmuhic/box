#!/bin/bash
# Builds and runs the performance suite. All arguments are forwarded to the
# benchmark binary, so `./perf.sh -f hmap -n 1000000` works.
#
# Except --repeat-process K, which this script consumes itself: it runs the
# binary K times and aggregates across processes. See "Run-to-run variance"
# below for why that is a different measurement from raising -r.

set -e

BUILD_DIR="build-perf"
BOX_PERF_NATIVE="${BOX_PERF_NATIVE:-ON}"

# --- argument split -------------------------------------------------------
REPEAT=1
ARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --repeat-process)
            REPEAT="$2"
            shift 2
            ;;
        --repeat-process=*)
            REPEAT="${1#*=}"
            shift
            ;;
        *)
            ARGS+=("$1")
            shift
            ;;
    esac
done

if ! [ "$REPEAT" -ge 1 ] 2>/dev/null; then
    echo "perf.sh: --repeat-process needs a positive integer, got '$REPEAT'" >&2
    exit 1
fi

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

# --- measurement hygiene --------------------------------------------------
# None of this is fatal; it is advisory so a noisy result is not mistaken for a
# real regression. The checks below are also the place where a virtualised host
# has to say so out loud: silently skipping them reads as a clean bill of health
# when in fact nothing was verified.

IS_WSL=0
if grep -qi microsoft /proc/sys/kernel/osrelease 2>/dev/null; then
    IS_WSL=1
fi

if [ "$IS_WSL" = "1" ]; then
    echo
    echo "--- NOTE: running under WSL2; clock stability is capped by the host ---"
    echo "    /sys/devices/system/cpu/*/cpufreq and intel_pstate do not exist in"
    echo "    this VM, so governor and turbo cannot be read let alone pinned --"
    echo "    Windows owns both. taskset pins to a vCPU, which the hypervisor is"
    echo "    still free to migrate across physical cores."
    echo "    Treat small deltas as unresolvable here. --repeat-process tells you"
    echo "    how small: anything inside the reported noise floor is not a signal."
elif [ -r /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
    GOV=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)
    if [ "$GOV" != "performance" ]; then
        echo
        echo "--- WARNING: cpu governor is '$GOV', not 'performance' ---"
        echo "    Frequency scaling will widen the noise column. To pin it:"
        echo "    sudo cpupower frequency-set -g performance"
    fi
fi

if [ "$IS_WSL" = "0" ] && [ -r /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
    if [ "$(cat /sys/devices/system/cpu/intel_pstate/no_turbo)" = "0" ]; then
        echo
        echo "--- NOTE: turbo boost is on; clocks drift as the CPU heats up ---"
        echo "    For the steadiest numbers:"
        echo "    echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo"
    fi
fi

# --- pinning --------------------------------------------------------------
# Pinning removes migration between cores with cold caches. Deliberately not
# CPU 0: it takes the bulk of the interrupt load on most systems. Pick the first
# thread of a middle physical core instead, and pin its SMT sibling too -- a
# sibling running unrelated work shares the core's execution units and shows up
# as noise we cannot see.
PIN_CPUS=""
if command -v lscpu >/dev/null 2>&1; then
    # "CPU CORE" pairs, skipping the header. Choose the core halfway up the list
    # so we avoid both CPU 0 and whatever the scheduler favours at the top end.
    MID_CORE=$(lscpu -e=CORE 2>/dev/null | tail -n +2 | sort -un | awk '{c[NR]=$1} END {if (NR) print c[int((NR+1)/2)]}')
    if [ -n "$MID_CORE" ]; then
        PIN_CPUS=$(lscpu -e=CPU,CORE 2>/dev/null | tail -n +2 | awk -v c="$MID_CORE" '$2==c {printf "%s%s", (n++?",":""), $1}')
    fi
fi

RUNNER=""
if command -v taskset >/dev/null 2>&1; then
    if [ -z "$PIN_CPUS" ]; then
        PIN_CPUS=0
    fi
    RUNNER="taskset -c $PIN_CPUS"
    echo
    echo "--- Pinning to CPU(s) $PIN_CPUS via taskset ---"
fi

BIN="./$BUILD_DIR/performance/box_perf"

# --- single run -----------------------------------------------------------
if [ "$REPEAT" = "1" ]; then
    echo
    $RUNNER "$BIN" "${ARGS[@]}"
    exit 0
fi

# --- run-to-run variance --------------------------------------------------
# Every repetition inside one process shares a heap layout, a binary mapping and
# one set of ASLR offsets. Code alignment and allocation addresses shift timings
# by a few percent and are fixed for the life of a process, so that component of
# the error is invisible to the in-process noise column no matter how high -r
# goes. Only re-executing the binary resamples it. The dispersion across
# processes is the real floor: a delta smaller than it is not a result.

TMPDIR_RUN=$(mktemp -d)
trap 'rm -rf "$TMPDIR_RUN"' EXIT

echo
echo "--- Running $REPEAT processes ---"
for i in $(seq 1 "$REPEAT"); do
    printf "\r    process %s/%s" "$i" "$REPEAT"
    # Per-run banner goes to stderr; suppress it here since the aggregate prints
    # its own header, but keep it to replay if a run fails.
    if ! $RUNNER "$BIN" "${ARGS[@]}" --csv \
            > "$TMPDIR_RUN/run_$i.csv" 2> "$TMPDIR_RUN/run_$i.err"; then
        printf "\r"
        echo "perf.sh: process $i failed:" >&2
        cat "$TMPDIR_RUN/run_$i.err" >&2
        exit 1
    fi
done
printf "\r                              \r"

cat "$TMPDIR_RUN"/run_*.csv | awk -F, '
    # Skip the header repeated once per process.
    $1 == "group" { next }
    {
        key = $1 "," $2 "," $3
        if (!(key in seen)) { order[++nkeys] = key; seen[key] = 1 }
        v[key, ++cnt[key]] = $5 + 0
        n_elems = $4
    }
    function median(key, c,   a, i, j, t) {
        for (i = 1; i <= c; i++) { a[i] = v[key, i] }
        for (i = 1; i < c; i++) {
            for (j = i + 1; j <= c; j++) {
                if (a[j] < a[i]) { t = a[i]; a[i] = a[j]; a[j] = t }
            }
        }
        return (c % 2) ? a[int((c + 1) / 2)] : (a[c/2] + a[c/2 + 1]) / 2
    }
    # Relative spread across processes, as max-min over the median. Unlike the
    # in-process column this one wants the extremes: a layout that happens to be
    # slow is a real outcome you can land on, not a scheduling artefact to trim.
    function relrange(key, c, med,   i, lo, hi, x) {
        lo = hi = v[key, 1]
        for (i = 2; i <= c; i++) {
            x = v[key, i]
            if (x < lo) lo = x
            if (x > hi) hi = x
        }
        return (med > 0) ? (hi - lo) / med * 100 : 0
    }
    END {
        printf "\n  n = %s elements, %d processes\n", n_elems, cnt[order[1]]
        printf "  ns/op is the median across processes.\n"
        printf "  drift is the max-min across processes over that median -- the\n"
        printf "  floor below which a change between runs means nothing.\n\n"
        printf "  %-10s %-14s %-20s %12s %8s\n", "group", "impl", "operation", "ns/op", "drift"
        printf "  %-10s %-14s %-20s %12s %8s\n", "----------", "--------------", "--------------------", "------------", "--------"
        worst = 0
        for (i = 1; i <= nkeys; i++) {
            key = order[i]
            split(key, f, ",")
            c = cnt[key]
            med = median(key, c)
            d = relrange(key, c, med)
            if (d > worst) { worst = d; worstkey = key }
            printf "  %-10s %-14s %-20s %12.4g %7.1f%%\n", f[1], f[2], f[3], med, d
        }
        printf "\n  noise floor: %.1f%% (worst row, %s)\n\n", worst, worstkey
    }
'
