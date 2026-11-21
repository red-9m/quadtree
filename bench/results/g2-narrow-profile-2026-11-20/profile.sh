#!/usr/bin/env bash
# Sample the Release binary from the narrow-stream gate, not the debug twin.
set -euo pipefail
export LC_ALL=C
work=$(realpath "$1") out=$(realpath -m "$2")
here=$(dirname "$(realpath "$0")")
ref="$here/../g2-bulk-streams-2026-11-20"
src="$work/narrow" build="$work/narrow-build" box2d=$(realpath "$work/box2d")
release=$(realpath "$build/bench/quadtree-bench")
mkdir "$out"
debug="$out/narrow.debug"
(cd "$src" && sha256sum --check --quiet "$ref/narrow-inputs.sha256")
"${CXX:-c++}" -DQT_BENCH_HAVE_BOX2D -I"$src" -I"$build/bench/Release" -I"$box2d/include" \
    -I"$build/_deps/box2d-build/src" -O3 -DNDEBUG -g -Wall -Wextra -Wpedantic -Wconversion \
    -Wsign-conversion -Wshadow "$src/bench/quadtree-bench.cpp" "$src/bench/bench-memory.cpp" \
    "$build/_deps/box2d-build/src/libbox2d.a" -lm -o "$debug"
stage=$(mktemp -d)
mode=/sys/firmware/acpi/platform_profile
files=("$mode" /sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference)
original=()
for f in "${files[@]}"; do original+=("$(<"$f")"); done
grep -H . "${files[@]}" > "$out/power-before.txt"
restore() {
    for i in "${!files[@]}"; do printf '%s\n' "${original[i]}" > "${files[i]}"; done
    grep -H . "${files[@]}" > "$out/power-after.txt"
    rm -rf "$stage"
    cmp "$out/power-before.txt" "$out/power-after.txt"
}
trap restore EXIT
for kind in release debug; do
    objcopy --dump-section ".text=$stage/$kind.text" "${!kind}" "$stage/$kind.elf"
    objdump -h "${!kind}" | awk '$2 == ".text" {print $3, $4, $6}' > "$stage/$kind.layout"
done
cmp "$stage/release.text" "$stage/debug.text"
cmp "$stage/release.layout" "$stage/debug.layout"
(cd "$stage" && sha256sum release.text debug.text) > "$out/text.sha256"
cp "$stage/release.layout" "$out/text.layout"
grep -Fqx "$(awk '$2 == "narrow.text" {print $1}' "$ref/text.sha256")  release.text" "$out/text.sha256"
printf '%s\n' "$release" > "$out/elf-path.txt"
for f in "${files[@]}"; do printf 'performance\n' > "$f"; done
check_mode() {
    for f in "${files[@]}"; do [[ $(<"$f") == performance ]] || return 1; done
}
{
    uname -a; lscpu | grep '^Model name:'; "${CXX:-c++}" --version | head -1; perf --version
    git -C "$box2d" rev-parse HEAD
    printf 'CPU affinity: 6\n'
    grep -H . "${files[@]}" /sys/devices/system/cpu/cpu6/topology/thread_siblings_list \
         /sys/devices/system/cpu/cpufreq/policy6/{scaling_driver,scaling_governor} \
         /sys/devices/system/clocksource/clocksource0/current_clocksource /proc/sys/kernel/perf_event_paranoid
} > "$out/environment.txt"
fixtures=(index:uniform index:split-line-translated index-ordered:split-line-translated index-ordered:mixed-size)
printf 'block,scene,n,engine,event,setting,start\n' > "$out/run-order.csv"
for block in 1 2; do
    hz=2999 period=830000
    if [[ $block == 2 ]]; then hz=3469 period=910000; fi
    for i in "${!fixtures[@]}"; do
        j=$i; [[ $block == 1 ]] || j=$((${#fixtures[@]} - 1 - i))
        fixture=${fixtures[j]}
        engine=${fixture%:*} scene=${fixture#*:}
        events=(cycles)
        if [[ $engine == index ]]; then
            if [[ $block == 1 ]]; then events=(cycles ibs); else events=(ibs cycles); fi
        fi
        for event in "${events[@]}"; do
            check_mode
            if [[ $event == cycles ]]; then
                setting=$hz; options=(-e cycles:u -F "$hz"); fields=period,ip,dso,dsoff
            else
                setting=$period; options=(-e 'ibs_op/cnt_ctl=1,swfilt=1/u' -c "$period" -d --sample-mem-info)
                fields=period,data_src,ip,dso,dsoff
            fi
            run="$out/$block-$scene-$engine-$event"
            printf '%s,%s,100000,%s,%s,%s,%s\n' "$block" "$scene" "$engine" "$event" "$setting" "$(date -Is)" >> "$out/run-order.csv"
            taskset -c 6 perf record -N -m 256 "${options[@]}" --user-callchains --call-graph dwarf,4096 \
                -o "$stage/perf.data" -- "$release" --model=snapshot --types=int32 --scenes="$scene" \
                --engines="$engine" --sizes=100000 --small-sizes= --rounds=1 --min-time=2.4 --max-time=4.8 \
                --max-reps=10000 --csv="$run.csv" > "$run.log" 2> "$run.perf.log"
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
check_mode
