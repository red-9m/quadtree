#!/usr/bin/env bash
# Profile the staged Release control; use the debug twin only for symbolization.
set -euo pipefail
export LC_ALL=C
work=$(realpath "$1") out=$(realpath -m "$2") cpu=${CPU:-6}
here=$(dirname "$(realpath "$0")")
ref="$here/../g2-child-bounds-only-2026-11-20"
src=$(realpath "$work/control") build=$(realpath "$work/control-build") box2d=$(realpath "$work/box2d")
release="$build/bench/quadtree-bench"
mkdir "$out"
debug="$out/control.debug"
(cd "$src" && sha256sum --check --quiet "$ref/control-inputs.sha256")
box2d_rev=$(git -C "$box2d" rev-parse HEAD)
[[ $box2d_rev == 178495ba35695aefe9c321e569d2277f06db3b90 ]]
if [[ -n ${DEBUG_TWIN:-} ]]; then
    cp "$DEBUG_TWIN" "$debug"
else
    "${CXX:-c++}" -DQT_BENCH_HAVE_BOX2D -I"$src" -I"$build/bench/Release" -I"$box2d/include" \
        -I"$build/_deps/box2d-build/src" -O3 -DNDEBUG -g -Wall -Wextra -Wpedantic -Wconversion \
        -Wsign-conversion -Wshadow "$src/bench/quadtree-bench.cpp" "$src/bench/bench-memory.cpp" \
        "$build/_deps/box2d-build/src/libbox2d.a" -lm -o "$debug"
fi
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for kind in release debug; do
    objcopy --dump-section ".text=$stage/$kind.text" "${!kind}" "$stage/$kind.elf"
    objdump -h "${!kind}" | awk '$2 == ".text" {print $3, $4, $6}' > "$stage/$kind.layout"
done
cmp "$stage/release.text" "$stage/debug.text"
cmp "$stage/release.layout" "$stage/debug.layout"
(cd "$stage" && sha256sum release.text debug.text) > "$out/text.sha256"
grep -Fqx "$(awk '$2 == "control.text" {print $1}' "$ref/text.sha256")  release.text" "$out/text.sha256"
cp "$stage/release.layout" "$out/text.layout"
printf '%s\n' "$release" > "$out/elf-path.txt"
{
    date -Is; uname -a; lscpu | grep '^Model name:'; "${CXX:-c++}" --version | head -1; perf --version
    printf 'Box2D: %s\nCPU affinity: %s\n' "$box2d_rev" "$cpu"
    grep -H . /sys/firmware/acpi/platform_profile /sys/devices/system/cpu/cpu"$cpu"/topology/thread_siblings_list \
         /sys/devices/system/cpu/cpu"$cpu"/cache/index0/{coherency_line_size,size} \
         /sys/devices/system/cpu/cpufreq/policy"$cpu"/{scaling_driver,scaling_governor,energy_performance_preference} \
         /sys/devices/system/clocksource/clocksource0/current_clocksource /proc/sys/kernel/perf_event_paranoid
} > "$out/environment.txt"
fixtures=(index:uniform index:split-line-translated index-ordered:uniform index-ordered:split-line-translated)
printf 'block,scene,n,engine,event,setting,start\n' > "$out/run-order.csv"
for block in 1 2; do
    hz=2999 period=830000 events=(cycles ibs)
    if [[ $block == 2 ]]; then hz=3469 period=910000 events=(ibs cycles); fi
    for i in "${!fixtures[@]}"; do
        j=$i; [[ $block == 1 ]] || j=$((${#fixtures[@]} - 1 - i))
        fixture=${fixtures[j]}
        engine=${fixture%:*} scene=${fixture#*:}
        for event in "${events[@]}"; do
            if [[ $event == cycles ]]; then
                setting=$hz; options=(-e cycles:u -F "$hz"); fields=period,ip,dso,dsoff
            else
                setting=$period; options=(-e 'ibs_op/cnt_ctl=1,swfilt=1/u' -c "$period" -d -W --sample-mem-info)
                fields=period,data_src,addr,weight,ip,dso,dsoff
            fi
            run="$out/$block-$scene-$engine-$event"
            printf '%s,%s,100000,%s,%s,%s,%s\n' "$block" "$scene" "$engine" "$event" "$setting" "$(date -Is)" >> "$out/run-order.csv"
            taskset -c "$cpu" perf record -N -m 256 "${options[@]}" --user-callchains --call-graph dwarf,4096 \
                -o "$stage/perf.data" -- "$release" --model=snapshot --types=int32 --scenes="$scene" \
                --engines="$engine" --sizes=100000 --small-sizes= --rounds=1 --min-time=2.4 --max-time=4.8 \
                --max-reps=10000 --csv="$run.csv" > "$run.log" 2> "$run.perf.log"
            perf script --show-lost-events -i "$stage/perf.data" -F "$fields" > "$stage/samples" 2> "$run.script.log"
            if grep -Eiq 'PERF_RECORD_LOST|lost [1-9][0-9]* (chunks|samples)|lost [0-9.]+%' \
                "$stage/samples" "$run.perf.log" "$run.script.log"; then
                printf 'Lost samples in %s\n' "$run" >&2; exit 1
            fi
            gzip -n < "$stage/samples" > "$run.samples.gz"
        done
    done
done
