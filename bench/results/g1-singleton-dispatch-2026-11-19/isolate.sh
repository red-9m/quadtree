#!/usr/bin/env bash
# Run at the repository root. Diagnostic only; do not use the patched executables elsewhere.
set -euo pipefail
export LC_ALL=C
exe=$(realpath "$1") out=$2
here=$(dirname "$(realpath "$0")")
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
out=$(realpath "$out")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
python3 "$here/ablate.py" "$exe" "$stage" > "$out/binaries.sha256"
{
    uname -a
    lscpu | grep '^Model name:'
    printf 'CPU affinity: 6\n'
    grep -H . /sys/devices/system/cpu/cpu6/topology/thread_siblings_list \
        /sys/devices/system/cpu/cpufreq/policy6/{scaling_driver,scaling_governor} \
        /sys/devices/system/clocksource/clocksource0/current_clocksource "$profile" "${prefs[@]}"
    uptime
} > "$out/environment.txt"

# Each four-block cycle balances run position and immediate predecessors.
# All timings are serial, in fresh processes.
orders=('control workspace combined dispatch' 'workspace dispatch control combined'
        'dispatch combined workspace control' 'combined control dispatch workspace')
printf 'block,variant,start\n' > "$out/run-order.csv"
for ((block=1; block<=12; ++block)); do
    read -ra order <<< "${orders[(block-1)%4]}"
    for variant in "${order[@]}"; do
        check_mode
        run="$out/$block-$variant"
        mkdir "$run"
        printf '%s,%s,%s\n' "$block" "$variant" "$(date -Is)" >> "$out/run-order.csv"
        taskset -c 6 "$stage/$variant" --model=snapshot --types=int32 \
            --scenes=uniform,clustered --engines=index,brute --sizes= --small-sizes=1 --batch=256 \
            --rounds=67 --min-time=0.08 --max-reps=2144 --csv="$run/snapshot.csv" \
            --raw="$run/snapshot-raw.csv" > "$run/run.log"
        grep -Fxq '# energy preference: performance' "$run/snapshot.csv"
        gzip -n "$run/snapshot-raw.csv"
    done
done
check_mode
python3 "$here/summarize.py" "$out"
