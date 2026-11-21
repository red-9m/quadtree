#!/usr/bin/env bash
# Matched phase screen; use the frozen slice runner for gate verdicts.
set -euo pipefail
export LC_ALL=C
w=$(realpath "$1")
mkdir "$2"
out=$(realpath "$2")
mode=/sys/firmware/acpi/platform_profile
files=("$mode" /sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference)
original=()
for f in "${files[@]}"; do original+=("$(<"$f")"); done
grep -H . "${files[@]}" > "$out/power-before.txt"
restore() {
    for i in "${!files[@]}"; do printf '%s\n' "${original[i]}" > "${files[i]}"; done
    grep -H . "${files[@]}" > "$out/power-after.txt"
    cmp "$out/power-before.txt" "$out/power-after.txt"
}
trap restore EXIT
for f in "${files[@]}"; do printf 'performance\n' > "$f"; done
for f in "${files[@]}"; do [[ $(<"$f") == performance ]]; done
{
    uname -a
    lscpu | grep '^Model name:'
    c++ --version | head -1
    git -C "$w/box2d" rev-parse HEAD
    printf 'CPU affinity: 6\n'
    grep -H . "${files[@]}" /sys/devices/system/cpu/cpu6/topology/thread_siblings_list \
        /sys/devices/system/clocksource/clocksource0/current_clocksource
} > "$out/environment.txt"
printf 'block,kind,start,finish\n' > "$out/run-order.csv"
for block in 1 2; do
    order=(control candidate)
    if [[ $block == 2 ]]; then order=(candidate control); fi
    for kind in "${order[@]}"; do
        start=$(date -Is)
        run="$out/$block-$kind"
        taskset -c 6 "$w/$kind-build/bench/quadtree-bench" --model=snapshot --types=int32 \
            --scenes=uniform,split-line,split-line-translated,mixed-size --engines=index,index-ordered \
            --sizes=100000 --small-sizes= --rounds=9 --min-time=0.03 --min-reps=3 --max-reps=12 \
            --csv="$run.csv" --raw="$run-raw.csv" > "$run.log"
        printf '%s,%s,%s,%s\n' "$block" "$kind" "$start" "$(date -Is)" >> "$out/run-order.csv"
        gzip -n "$run-raw.csv"
    done
done
