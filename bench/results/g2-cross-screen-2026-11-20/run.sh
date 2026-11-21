#!/usr/bin/env bash
# W contains control-build, candidate-build and box2d. Use an idle host.
set -euo pipefail
export LC_ALL=C
w=$(realpath "$1")
out=$(realpath -m "$2")
here=$(dirname "$(realpath "$0")")
runner="$here/../g1-flat-input-2026-11-19/reproduce.sh"
mkdir -p "$out"
if [[ -e "$out/run-order.csv" ]]; then
    printf 'Output already contains a run: %s\n' "$out" >&2
    exit 1
fi
mode=/sys/firmware/acpi/platform_profile
previous=$(<"$mode")
restore() {
    printf '%s\n' "$previous" > "$mode"
    printf '%s:%s\n' "$mode" "$(<"$mode")" > "$out/restored-mode.txt"
}
trap restore EXIT
printf 'performance\n' > "$mode"
{
    git -C "$here" rev-parse HEAD
    uname -a
    lscpu | grep '^Model name:'
    c++ --version | head -1
    git -C "$w/box2d" rev-parse HEAD
    printf 'CPU affinity: 6\n'
    for f in "$mode" /sys/devices/system/cpu/cpu6/topology/thread_siblings_list \
        /sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference \
        /sys/devices/system/clocksource/clocksource0/current_clocksource; do
        printf '%s:%s\n' "$f" "$(<"$f")"
    done
} > "$out/environment.txt"
printf 'run,start,finish\n' > "$out/run-order.csv"
for run in 9-control 9-candidate 15-candidate 15-control; do
    start=$(date -Is)
    taskset -c 6 bash "$runner" "$w/${run#*-}-build/bench/quadtree-bench" "$out/$run" "${run%-*}" full
    printf '%s,%s,%s\n' "$run" "$start" "$(date -Is)" >> "$out/run-order.csv"
    gzip -n "$out/$run/snapshot-raw.csv"
done
