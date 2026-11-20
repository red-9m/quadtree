#!/usr/bin/env bash
# Diagnostic only. Run from the repository root on Zen 3 with IBS enabled.
set -euo pipefail
export LC_ALL=C
release=$(realpath "$1") out=$2
sha256sum --quiet -c bench/results/g1-tiny-store-fusion-2026-11-19/control-inputs.sha256
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
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
objcopy --dump-section ".text=$work/release.text" "$release" "$work/release.copy"
(cd "$work" && sha256sum release.text) > "$out/text.sha256"
grep -Fxq '34423ec81fa44ba7b4a0b139799c19be26e916cfd268c072b5e22e4ac19c9807  release.text' "$out/text.sha256"
{
    uname -a
    lscpu | grep '^Model name:'
    c++ --version | head -1
    perf --version
    printf 'CPU affinity: 6\nDSO: %s\n' "$release"
    grep -H . /sys/devices/system/cpu/cpu6/topology/thread_siblings_list \
        /sys/devices/system/cpu/cpufreq/policy6/{scaling_driver,scaling_governor} \
        /sys/devices/system/clocksource/clocksource0/current_clocksource "$profile" "${prefs[@]}"
    uptime
} > "$out/environment.txt"
printf 'run,period,start,finish\n' > "$out/run-order.csv"
scenes=(uniform split-line split-line-translated clustered)
for block in 1 2; do
    period=16384; [[ $block == 1 ]] || period=32768
    for i in "${!scenes[@]}"; do
        j=$i; [[ $block == 1 ]] || j=$((${#scenes[@]} - i - 1))
        scene=${scenes[j]} name=$block-${scenes[j]}-100000
        start=$(date -Is)
        check_mode
        taskset -c 6 perf record -e 'ibs_op/cnt_ctl=1,swfilt=1/u' -c "$period" -d -W --sample-mem-info \
            -o "$work/ibs.data" -- "$release" --model=snapshot --types=int32 --scenes="$scene" \
            --engines=index --sizes=100000 --small-sizes= --rounds=1 --min-time=1.5 --max-time=2 \
            --max-reps=10000 --csv="$out/$name.csv" > "$out/$name.log" 2> "$out/$name.perf.log"
        perf script -i "$work/ibs.data" -F ip,dso,dsoff,data_src,weight | gzip -n > "$out/$name.ibs.gz"
        printf '%s,%s,%s,%s\n' "$name" "$period" "$start" "$(date -Is)" >> "$out/run-order.csv"
    done
done
check_mode
