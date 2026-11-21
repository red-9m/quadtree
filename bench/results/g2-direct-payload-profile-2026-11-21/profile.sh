#!/usr/bin/env bash
# Attribute the control and direct-payload Release ELFs.
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1") out=$(realpath -m "$2")
ref="$here/../g2-direct-payload-2026-11-21"
box2d=$(realpath "$w/box2d")
mkdir "$out"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
[[ $(git -C "$box2d" rev-parse HEAD) == 178495ba35695aefe9c321e569d2277f06db3b90 ]]
for kind in control candidate; do
    src=$(realpath "$w/$kind"); build=$(realpath "$w/$kind-build")
    release="$build/bench/quadtree-bench" debug="$out/$kind.debug"
    manifest=$ref; [[ $kind != control ]] || manifest="$here/../g9-two-scatter-source-radix-2026-11-21"
    (cd "$src" && sha256sum -c --quiet "$manifest/candidate-inputs.sha256")
    if [[ $kind == control ]]; then
        sha256sum < "$release" | awk '{print $1 "  quadtree-bench"}' | cmp "$ref/control-executable.sha256" -
    fi
    "${CXX:-c++}" -DQT_BENCH_HAVE_BOX2D -I"$src" -I"$build/bench/Release" -I"$box2d/include" \
        -I"$build/_deps/box2d-build/src" -O3 -DNDEBUG -g -Wall -Wextra -Wpedantic -Wconversion \
        -Wsign-conversion -Wshadow "$src/bench/quadtree-bench.cpp" "$src/bench/bench-memory.cpp" \
        "$build/_deps/box2d-build/src/libbox2d.a" -lm -o "$debug"
    for mode in release debug; do
        objcopy --dump-section ".text=$stage/$kind-$mode.text" "${!mode}" "$stage/$kind-$mode.elf"
        objdump -h "${!mode}" | awk '$2 == ".text" {print $3, $4, $6}' > "$stage/$kind-$mode.layout"
    done
    cmp "$stage/$kind-release.text" "$stage/$kind-debug.text"
    cmp "$stage/$kind-release.layout" "$stage/$kind-debug.layout"
    mv "$stage/$kind-release.text" "$stage/$kind.text"
    cp "$stage/$kind-release.layout" "$out/$kind.text.layout"
    printf '%s\n' "$release" > "$out/$kind.elf-path.txt"
done
(cd "$stage" && sha256sum -c "$ref/text.sha256" && sha256sum control.text control-debug.text candidate.text candidate-debug.text) > "$out/text-check.log"
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
fixtures=(control:index:uniform candidate:index:uniform control:index:split-line-translated candidate:index:split-line-translated control:index-ordered:split-line-translated candidate:index-ordered:split-line-translated)
printf 'block,kind,engine,scene,event,start,start_tctl_millidegrees,finish\n' > "$out/run-order.csv"
for block in 1 2; do
    accepted=$(taskset -c 0 python3 "$quiet" "$temp" "$out/quiet-start-$block.csv" "${target[@]}")
    if [[ $block == 1 ]]; then hz=2999; period=830000; target=("$accepted"); else hz=3469; period=910000; fi
    for i in "${!fixtures[@]}"; do
        j=$i; [[ $block == 1 ]] || j=$((${#fixtures[@]} - 1 - i))
        IFS=: read -r kind engine scene <<< "${fixtures[j]}"
        release=$(<"$out/$kind.elf-path.txt")
        events=(cycles)
        if [[ $scene == split-line-translated && $engine == index ]]; then events+=(ibs); fi
        for event in "${events[@]}"; do
            if [[ $event == cycles ]]; then options=(-e cycles:u -F "$hz"); fields=period,ip,dso,dsoff
            else options=(-e 'ibs_op/cnt_ctl=1,swfilt=1/u' -c "$period" -d -W --sample-mem-info); fields=period,data_src,addr,weight,ip,dso,dsoff; fi
            run="$out/$block-$kind-$scene-$engine-$event"
            start=$(date -Is); start_temp=$(<"$temp")
            taskset -c 6 perf record -N -m 256 "${options[@]}" --user-callchains --call-graph dwarf,4096 \
                -o "$stage/perf.data" -- "$release" --model=snapshot --types=int32 --scenes="$scene" \
                --engines="$engine" --sizes=100000 --small-sizes= --rounds=1 --min-time=2.4 --max-time=4.8 \
                --max-reps=10000 --csv="$run.csv" > "$run.log" 2> "$run.perf.log"
            printf '%s,%s,%s,%s,%s,%s,%s,%s\n' "$block" "$kind" "$engine" "$scene" "$event" "$start" "$start_temp" "$(date -Is)" >> "$out/run-order.csv"
            perf script --show-lost-events -i "$stage/perf.data" -F "$fields" > "$stage/samples" 2> "$run.script.log"
            if grep -Eiq 'PERF_RECORD_LOST|lost [1-9][0-9]* (chunks|samples)|lost [0-9.]+%' \
                "$stage/samples" "$run.perf.log" "$run.script.log"; then
                printf 'Lost samples in %s\n' "$run" >&2; exit 1
            fi
            gzip -n < "$stage/samples" > "$run.samples.gz"
            grep -Fxq '# energy preference: performance' "$run.csv"
        done
    done
done
