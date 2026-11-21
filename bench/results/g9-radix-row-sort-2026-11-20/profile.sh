#!/usr/bin/env bash
# Sample the actual Release runners; use byte-matched debug twins only for PCs.
set -euo pipefail
export LC_ALL=C
work=$(realpath "$1") out=$2
mkdir "$out"
out=$(realpath "$out")
profile=/sys/firmware/acpi/platform_profile
prefs=(/sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference)
box2d=$(realpath "$work/box2d")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
check_mode() {
    for file in "$profile" "${prefs[@]}"; do
        if [[ $(< "$file") != performance ]]; then
            printf '%s is not performance\n' "$file" >&2
            return 1
        fi
    done
}
check_mode
for kind in control candidate; do
    src=$(realpath "$work/$kind")
    build=$(realpath "$work/$kind-build")
    release="$build/bench/quadtree-bench"
    debug="$out/$kind.debug"
    "${CXX:-c++}" -DQT_BENCH_HAVE_BOX2D -I"$src" -I"$build/bench/Release" \
        -I"$box2d/include" -I"$build/_deps/box2d-build/src" -O3 -DNDEBUG -g \
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow \
        "$src/bench/quadtree-bench.cpp" "$src/bench/bench-memory.cpp" \
        "$build/_deps/box2d-build/src/libbox2d.a" -lm -o "$debug"
    for side in release debug; do
        objcopy --dump-section ".text=$stage/$kind-$side.text" "${!side}" "$stage/$side.copy"
        objdump -h "${!side}" | awk '$2 == ".text" {print $3, $4, $6}' > "$stage/$side.layout"
    done
    cmp "$stage/$kind-release.text" "$stage/$kind-debug.text"
    cmp "$stage/release.layout" "$stage/debug.layout"
    cp "$stage/release.layout" "$out/$kind.text.layout"
    printf '%s\n' "$release" > "$out/$kind.elf-path.txt"
done
(cd "$stage" && sha256sum -- *.text) > "$out/profile-text.sha256"
perf --version > "$out/perf-version.txt"
printf 'round,source,hz,start\n' > "$out/profile-order.csv"
for round in 1 2 3; do
    case $round in
        1) hz=2999; order=(control candidate) ;;
        2) hz=3469; order=(candidate control) ;;
        3) hz=4001; order=(control candidate) ;;
    esac
    for kind in "${order[@]}"; do
        check_mode
        release=$(< "$out/$kind.elf-path.txt")
        run="$out/$round-$kind"
        printf '%s,%s,%s,%s\n' "$round" "$kind" "$hz" "$(date -Is)" >> "$out/profile-order.csv"
        taskset -c 6 perf record -e cycles:u -F "$hz" --user-callchains --call-graph dwarf,4096 \
            -o "$stage/perf.data" -- "$release" --model=snapshot --types=int32 --scenes=mixed-size \
            --engines=index-ordered --sizes=100000 --small-sizes= --rounds=1 --min-time=2.4 --max-time=4.8 \
            --max-reps=10000 --csv="$run.csv" > "$run.log" 2> "$run.perf.log"
        perf script -i "$stage/perf.data" -F period,ip,dso,dsoff | gzip -n > "$run.samples.gz"
        grep -Fxq '# energy preference: performance' "$run.csv"
    done
done
check_mode
