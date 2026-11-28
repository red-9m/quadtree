#!/usr/bin/env bash
# Post-hoc diagnostic, NOT part of resolution-rule.md and not used for its decision.
# Runs the int32-presolve arguments (G8b mixed-size n=1000/100000 small-single/moved, 45 rounds)
# on the c13e498, ce92ef5 and db0c820 release binaries, rotating order, same protocol as run.sh.
# usage: run.sh BIN_DIR OUT   (BIN_DIR holds c13e, ce92, db0c: equal-length paths)
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")")
bin=$(realpath "$1") out=$(realpath -m "$2")
mkdir -p "$out"
quiet="$here/../../g2-two-scatter-repeatability-2026-11-21/quiet-start.py"
temp=$(grep -l '^k10temp$' /sys/class/hwmon/hwmon*/name); temp=${temp%/name}/temp1_input
files=(/sys/firmware/acpi/platform_profile /sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference)
original=(); for f in "${files[@]}"; do original+=("$(<"$f")"); done
restore() { for i in "${!files[@]}"; do printf '%s\n' "${original[i]}" > "${files[i]}"; done; }
trap restore EXIT; trap 'exit 130' INT; trap 'exit 143' TERM
for f in "${files[@]}"; do printf 'performance\n' > "$f"; done
args=($(<"$here/../data/int32-presolve/args.txt"))
printf 'run,start,start_tctl_millidegrees,exit\n' > "$out/run-order.csv"
for round in 1 2 3; do
    case $round in 1) order=(c13e ce92 db0c);; 2) order=(db0c c13e ce92);; 3) order=(ce92 db0c c13e);; esac
    for b in "${order[@]}"; do
        dir=$out/$b-$round; mkdir "$dir"; rc=0
        taskset -c 0 python3 "$quiet" "$temp" "$dir/quiet-start.csv" > /dev/null
        printf '%s,%s,%s,' "$b-$round" "$(date -Is)" "$(<"$temp")" >> "$out/run-order.csv"
        taskset -c 6 "$bin/$b" "${args[@]}" --csv="$dir/results.csv" > "$dir/run.log" 2>&1 || rc=$?
        "$bin/$b" --gate-files="$dir/results.csv" > "$dir/gates.txt" || true
        echo "$rc" >> "$out/run-order.csv"
    done
done
