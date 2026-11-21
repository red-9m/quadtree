#!/usr/bin/env bash
# Isolate rank validation on the interleaved-descriptor radix control.
set -euo pipefail
export LC_ALL=C
w=$(realpath "$1")
mkdir "$2"
out=$(realpath "$2")
mode=/sys/firmware/acpi/platform_profile
previous=$(<"$mode")
restore() {
    printf '%s\n' "$previous" > "$mode"
    printf '%s:%s\n' "$mode" "$(<"$mode")" > "$out/restored-mode.txt"
}
trap restore EXIT
printf 'performance\n' > "$mode"
grep -H . "$mode" /sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference > "$out/environment.txt"
printf 'run,start,finish\n' > "$out/run-order.csv"
for kind in radix rank; do
    start=$(date -Is)
    taskset -c 6 "$w/$kind-build/bench/quadtree-bench" --model=snapshot --types=int32 \
        --scenes=uniform,split-line,split-line-translated,mixed-size --engines=index,index-ordered \
        --sizes=100000 --small-sizes= --rounds=9 --min-time=0.03 --min-reps=3 --max-reps=12 \
        --csv="$out/$kind.csv" --raw="$out/$kind-raw.csv" > "$out/$kind.log"
    printf '%s,%s,%s\n' "$kind" "$start" "$(date -Is)" >> "$out/run-order.csv"
    gzip -n "$out/$kind-raw.csv"
done
