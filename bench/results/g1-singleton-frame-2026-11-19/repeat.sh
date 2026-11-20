#!/usr/bin/env bash
# Run with the platform profile and CPU energy preferences already set to performance.
set -euo pipefail
export LC_ALL=C
control=$(realpath "$1") outlined=$(realpath "$2") inline=$(realpath "$3") out=$(realpath -m "$4")
reproduce=$(dirname "$(realpath "$0")")/../g1-flat-input-2026-11-19/reproduce.sh
profile=/sys/firmware/acpi/platform_profile
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
mkdir "$out"
printf 'run,start,finish\n' > "$out/run-order.csv"
for run in 81-control 81-outlined 81-inline 117-outlined 117-inline 117-control 153-inline 153-control 153-outlined; do
    check_mode
    kind=${run#*-}
    start=$(date -Is)
    taskset -c 6 bash "$reproduce" "${!kind}" "$out/$run" "${run%-*}"
    check_mode
    grep -Fxq '# energy preference: performance' "$out/$run/snapshot.csv"
    printf '%s,%s,%s\n' "$run" "$start" "$(date -Is)" >> "$out/run-order.csv"
done
