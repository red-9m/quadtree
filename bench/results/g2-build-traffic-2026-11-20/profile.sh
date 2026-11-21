#!/usr/bin/env bash
# WORK is the staged radix work directory. DEBUG must match its Release .text.
set -euo pipefail
export LC_ALL=C
work=$(realpath "$1") debug=$(realpath "$2") out=$3
here=$(dirname "$(realpath "$0")")
ref="$here/../g9-radix-row-sort-2026-11-20"
release=$(realpath "$work/candidate-build/bench/quadtree-bench")
mkdir "$out"
out=$(realpath "$out")
(cd "$work/candidate" && sha256sum --check --quiet "$ref/candidate-inputs.sha256")
stage=$(mktemp -d)
profile=/sys/firmware/acpi/platform_profile
original=$(< "$profile")
restore() {
    printf '%s\n' "$original" > "$profile"
    printf '%s:%s\n' "$profile" "$(< "$profile")" > "$out/restored-mode.txt"
    rm -rf "$stage"
}
trap restore EXIT
for side in release debug; do
    objcopy --dump-section ".text=$stage/$side.text" "${!side}" "$stage/$side.elf"
    objdump -h "${!side}" | awk '$2 == ".text" {print $3, $4, $6}' > "$stage/$side.layout"
done
cmp "$stage/release.text" "$stage/debug.text"
cmp "$stage/release.layout" "$stage/debug.layout"
(cd "$stage" && sha256sum release.text debug.text) > "$out/text.sha256"
cp "$stage/release.layout" "$out/text.layout"
expected=$(awk '$2 == "candidate.text" {print $1}' "$ref/text.sha256")
grep -Fxq "$expected  release.text" "$out/text.sha256"
printf '%s\n' "$release" > "$out/elf-path.txt"
printf 'performance\n' > "$profile"
prefs=(/sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference)
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
    c++ --version | head -1
    perf --version
    git -C "$work/box2d" rev-parse HEAD
    printf 'CPU affinity: 6\n'
    grep -H . "$profile" "${prefs[@]}" /sys/devices/system/cpu/cpu6/topology/thread_siblings_list \
         /sys/devices/system/cpu/cpufreq/policy6/{scaling_driver,scaling_governor} \
         /sys/devices/system/clocksource/clocksource0/current_clocksource /proc/sys/kernel/perf_event_paranoid
} > "$out/environment.txt"
fixtures=(index:uniform index:split-line-translated index-ordered:uniform index-ordered:split-line-translated)
printf 'block,scene,n,engine,event,setting,start\n' > "$out/run-order.csv"
for block in 1 2; do
    hz=2999 period=640016
    events=(cycles ibs)
    if [[ $block == 2 ]]; then
        hz=3469 period=710000
        events=(ibs cycles)
    fi
    for i in "${!fixtures[@]}"; do
        j=$i; [[ $block == 1 ]] || j=$((${#fixtures[@]} - 1 - i))
        fixture=${fixtures[j]}
        engine=${fixture%:*} scene=${fixture#*:}
        for event in "${events[@]}"; do
            check_mode
            if [[ $event == cycles ]]; then
                setting=$hz
                options=(-e cycles:u -F "$hz")
                fields=period,ip,dso,dsoff
            else
                setting=$period
                options=(-e 'ibs_op/cnt_ctl=1,swfilt=1/u' -c "$period" -d --sample-mem-info)
                fields=period,data_src,ip,dso,dsoff
            fi
            run="$out/$block-$scene-$engine-$event"
            printf '%s,%s,100000,%s,%s,%s,%s\n' "$block" "$scene" "$engine" "$event" "$setting" "$(date -Is)" >> "$out/run-order.csv"
            taskset -c 6 perf record -N -m 256 "${options[@]}" --user-callchains --call-graph dwarf,4096 \
                -o "$stage/perf.data" -- "$release" --model=snapshot --types=int32 --scenes="$scene" \
                --engines="$engine" --sizes=100000 --small-sizes= --rounds=1 --min-time=1.6 --max-time=3.2 \
                --max-reps=10000 --csv="$run.csv" > "$run.log" 2> "$run.perf.log"
            perf script --show-lost-events -i "$stage/perf.data" -F "$fields" > "$stage/samples" 2> "$run.script.log"
            if grep -Eiq 'PERF_RECORD_LOST|lost [1-9][0-9]* (chunks|samples)|lost [0-9.]+%' \
                "$stage/samples" "$run.perf.log" "$run.script.log"; then
                printf 'Lost samples in %s\n' "$run" >&2
                exit 1
            fi
            gzip -n < "$stage/samples" > "$run.samples.gz"
            grep -Fxq '# energy preference: performance' "$run.csv"
        done
    done
done
check_mode
