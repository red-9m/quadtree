#!/usr/bin/env bash
# Run with the platform profile and CPU energy preferences set to performance.
set -euo pipefail
export LC_ALL=C
control=$(realpath "$1") wrapper=$(realpath "$2") out=$(realpath -m "$3")
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
{
    uname -a
    lscpu | grep '^Model name:'
    printf 'CPU affinity: 6\n'
    grep -H . /sys/devices/system/cpu/cpu6/topology/thread_siblings_list \
        /sys/devices/system/cpu/cpufreq/policy6/{scaling_driver,scaling_governor} \
        /sys/devices/system/clocksource/clocksource0/current_clocksource "$profile" "${prefs[@]}"
    uptime
} > "$out/environment.txt"
printf 'run,start,finish\n' > "$out/run-order.csv"
for run in 81-control 81-wrapper 117-wrapper 117-control 153-control 153-wrapper; do
    check_mode
    kind=${run#*-}
    start=$(date -Is)
    taskset -c 6 bash "$reproduce" "${!kind}" "$out/$run" "${run%-*}"
    check_mode
    grep -Fxq '# energy preference: performance' "$out/$run/snapshot.csv"
    printf '%s,%s,%s\n' "$run" "$start" "$(date -Is)" >> "$out/run-order.csv"
    gzip -n "$out/$run/snapshot-raw.csv"
done
