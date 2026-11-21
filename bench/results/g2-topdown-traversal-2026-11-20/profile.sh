#!/usr/bin/env bash
# Sample the staged Release executable; use a byte-matched debug twin to symbolize.
set -euo pipefail
export LC_ALL=C
source=$(realpath "$1") build=$(realpath "$2") box2d=$(realpath "$3") out=$4
here=$(dirname "$(realpath "$0")")
release="$build/bench/quadtree-bench"
profile=/sys/firmware/acpi/platform_profile
prefs=(/sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference)
mkdir "$out"
out=$(realpath "$out")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
(cd "$source" && sha256sum --check --quiet "$here/../g2-topdown-build-2026-11-20/candidate-inputs.sha256")
debug="$out/quadtree-bench.debug"
"${CXX:-c++}" -DQT_BENCH_HAVE_BOX2D -I"$source" -I"$build/bench/Release" \
    -I"$box2d/include" -I"$build/_deps/box2d-build/src" -O3 -DNDEBUG -g \
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow \
    "$source/bench/quadtree-bench.cpp" "$source/bench/bench-memory.cpp" \
    "$build/_deps/box2d-build/src/libbox2d.a" -lm -o "$debug"
for kind in release debug; do
    objcopy --dump-section ".text=$stage/$kind.text" "${!kind}" "$stage/$kind.copy"
    objdump -h "${!kind}" | awk '$2 == ".text" {print $3, $4, $6}' > "$stage/$kind.layout"
done
cmp "$stage/release.text" "$stage/debug.text"
cmp "$stage/release.layout" "$stage/debug.layout"
(cd "$stage" && sha256sum release.text debug.text) > "$out/text.sha256"
cp "$stage/release.layout" "$out/text.layout"
expected=$(awk '$2 == "candidate.text" {print $1}' "$here/../g2-topdown-build-2026-11-20/text.sha256")
grep -Fqx "$expected  release.text" "$out/text.sha256"
check_mode() {
    for file in "$profile" "${prefs[@]}"; do
        if [[ $(< "$file") != performance ]]; then
            printf '%s is not performance\n' "$file" >&2
            return 1
        fi
    done
}
check_mode
{
    uname -a
    lscpu | grep '^Model name:'
    "${CXX:-c++}" --version | head -1
    perf --version
    git -C "$box2d" rev-parse HEAD
    printf 'CPU affinity: 6\nRelease executable: %s\n' "$release"
    grep -H . /sys/devices/system/cpu/cpu6/topology/thread_siblings_list \
        /sys/devices/system/cpu/cpufreq/policy6/{scaling_driver,scaling_governor} \
        /sys/devices/system/clocksource/clocksource0/current_clocksource "$profile" "${prefs[@]}"
} > "$out/environment.txt"
fixtures=(index-ordered:split-line index-ordered:split-line-translated index-ordered:mixed-size
          index:uniform index:split-line-translated)
printf 'block,scene,n,engine,hz,start\n' > "$out/run-order.csv"
for block in 1 2; do
    hz=2999; [[ $block == 1 ]] || hz=3469
    for i in "${!fixtures[@]}"; do
        j=$i; [[ $block == 1 ]] || j=$((${#fixtures[@]} - 1 - i))
        fixture=${fixtures[j]}
        engine=${fixture%:*} scene=${fixture#*:}
        check_mode
        run="$out/$block-$scene-$engine"
        printf '%s,%s,100000,%s,%s,%s\n' "$block" "$scene" "$engine" "$hz" "$(date -Is)" >> "$out/run-order.csv"
        taskset -c 6 perf record -q -e cycles:u -F "$hz" --user-callchains --call-graph dwarf,4096 \
            -o "$stage/perf.data" -- "$release" --model=snapshot --types=int32 --scenes="$scene" \
            --engines="$engine" --sizes=100000 --small-sizes= --rounds=1 --min-time=1.2 --max-time=2.4 \
            --max-reps=10000 --csv="$run.csv" > "$run.log"
        perf script -i "$stage/perf.data" -F period,ip,dso,dsoff | gzip -n > "$run.samples.gz"
        grep -Fxq '# energy preference: performance' "$run.csv"
    done
done
check_mode
