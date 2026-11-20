#!/usr/bin/env bash
set -euo pipefail
control=$(realpath "$1")
direct=$(realpath "$2")
out=$(realpath -m "$3")
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
for run in 81-control 81-direct 117-direct 117-control 153-control 153-direct; do
    check_mode
    case $run in
        *-control) exe=$control ;;
        *-direct) exe=$direct ;;
    esac
    start=$(date -Is)
    taskset -c 6 bash "$reproduce" "$exe" "$out/$run" "${run%-*}"
    check_mode
    grep -Fxq '# energy preference: performance' "$out/$run/snapshot.csv"
    printf '%s,%s,%s\n' "$run" "$start" "$(date -Is)" >> "$out/run-order.csv"
done
