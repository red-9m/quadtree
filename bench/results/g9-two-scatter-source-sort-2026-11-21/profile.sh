#!/usr/bin/env bash
# Capture user cycles from the unchanged two-stream Release ELF.
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1") out=$(realpath -m "$2")
src=$(realpath "$w/control") build=$(realpath "$w/control-build") box2d=$(realpath "$w/box2d")
release="$build/bench/quadtree-bench" debug="$out/staged.debug"
ref="$here/../g2-two-scatter-2026-11-21"
mkdir "$out"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
(cd "$src" && sha256sum -c --quiet "$ref/candidate-inputs.sha256")
[[ $(git -C "$box2d" rev-parse HEAD) == 178495ba35695aefe9c321e569d2277f06db3b90 ]]
sha256sum < "$release" | awk '{print $1 "  quadtree-bench"}' > "$out/executable.sha256"
cmp "$out/executable.sha256" "$here/../g2-two-scatter-thermal-2026-11-21/frozen/executable.sha256"
"${CXX:-c++}" -DQT_BENCH_HAVE_BOX2D -I"$src" -I"$build/bench/Release" -I"$box2d/include" \
    -I"$build/_deps/box2d-build/src" -O3 -DNDEBUG -g -Wall -Wextra -Wpedantic -Wconversion \
    -Wsign-conversion -Wshadow "$src/bench/quadtree-bench.cpp" "$src/bench/bench-memory.cpp" \
    "$build/_deps/box2d-build/src/libbox2d.a" -lm -o "$debug"
for kind in release debug; do
    objcopy --dump-section ".text=$stage/$kind.text" "${!kind}" "$stage/$kind.elf"
    objdump -h "${!kind}" | awk '$2 == ".text" {print $3, $4, $6}' > "$stage/$kind.layout"
done
cmp "$stage/release.text" "$stage/debug.text"
cmp "$stage/release.layout" "$stage/debug.layout"
(cd "$stage" && sha256sum release.text debug.text) > "$out/text.sha256"
grep -Fqx "$(awk '$2 == "candidate.text" {print $1}' "$ref/text.sha256")  release.text" "$out/text.sha256"
cp "$stage/release.layout" "$out/text.layout"
printf '%s\n' "$release" > "$out/elf-path.txt"

files=(/sys/firmware/acpi/platform_profile /sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference)
clock=/sys/devices/system/clocksource/clocksource0/current_clocksource
siblings=/sys/devices/system/cpu/cpu6/topology/thread_siblings_list
[[ $(<"$clock") == hpet && $(<"$siblings") == 6-7 ]]
original=()
for f in "${files[@]}"; do [[ -w $f ]]; original+=("$(<"$f")"); done
grep -H . "${files[@]}" > "$out/power-before.txt"
restore() {
    for i in "${!files[@]}"; do printf '%s\n' "${original[i]}" > "${files[i]}"; done
    grep -H . "${files[@]}" > "$out/power-after.txt"
    rm -rf "$stage"
    cmp "$out/power-before.txt" "$out/power-after.txt"
}
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
for f in "${files[@]}"; do printf 'performance\n' > "$f"; done
for f in "${files[@]}"; do [[ $(<"$f") == performance ]]; done
{
    date -Is; uname -a; lscpu | grep '^Model name:'; "${CXX:-c++}" --version | head -1; perf --version
    git -C "$here" rev-parse HEAD; git -C "$box2d" rev-parse HEAD
    printf 'CPU affinity: 6\n'
    grep -H . "$siblings" "$clock" "${files[@]}" /proc/sys/kernel/perf_event_paranoid
} > "$out/environment.txt"
quiet="$here/../g2-two-scatter-repeatability-2026-11-21/quiet-start.py"
temp=
for f in /sys/class/hwmon/hwmon*/name; do
    if [[ $(<"$f") == k10temp ]]; then temp="${f%/name}/temp1_input"; break; fi
done
[[ -n $temp && -r $temp ]]
target=()
fixtures=(index:uniform index:split-line-translated index-ordered:uniform index-ordered:split-line-translated)
printf 'block,scene,engine,hz,start,start_tctl_millidegrees,finish_tctl_millidegrees,finish\n' > "$out/run-order.csv"
for block in 1 2; do
    accepted=$(taskset -c 0 python3 "$quiet" "$temp" "$out/quiet-start-$block.csv" "${target[@]}")
    if [[ $block == 1 ]]; then hz=2999; target=("$accepted"); else hz=3469; fi
    for i in "${!fixtures[@]}"; do
        j=$i; [[ $block == 1 ]] || j=$((${#fixtures[@]} - 1 - i))
        engine=${fixtures[j]%:*} scene=${fixtures[j]#*:}
        run="$out/$block-$scene-$engine"
        start=$(date -Is); start_temp=$(<"$temp")
        taskset -c 6 perf record -N -m 256 -e cycles:u -F "$hz" --user-callchains --call-graph dwarf,4096 \
            -o "$stage/perf.data" -- "$release" --model=snapshot --types=int32 --scenes="$scene" \
            --engines="$engine" --sizes=100000 --small-sizes= --rounds=1 --min-time=4.8 --max-time=9.6 \
            --max-reps=10000 --csv="$run.csv" > "$run.log" 2> "$run.perf.log"
        printf '%s,%s,%s,%s,%s,%s,%s,%s\n' "$block" "$scene" "$engine" "$hz" "$start" "$start_temp" "$(<"$temp")" "$(date -Is)" >> "$out/run-order.csv"
        perf script --show-lost-events -i "$stage/perf.data" -F period,ip,dso,dsoff > "$stage/samples" 2> "$run.script.log"
        if grep -Eiq 'PERF_RECORD_LOST|lost [1-9][0-9]* (chunks|samples)|lost [0-9.]+%' \
            "$stage/samples" "$run.perf.log" "$run.script.log"; then
            printf 'Lost samples in %s\n' "$run" >&2; exit 1
        fi
        gzip -n < "$stage/samples" > "$run.samples.gz"
        grep -Fxq '# energy preference: performance' "$run.csv"
    done
done
