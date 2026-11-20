#!/usr/bin/env bash
# Run at the repo root. The debug twin is used only for symbolization.
set -euo pipefail
export LC_ALL=C
release=$(realpath "$1") debug=$(realpath "$2") out=$3
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
for kind in release debug; do
    objcopy --dump-section ".text=$stage/$kind.text" "${!kind}" "$stage/$kind.copy"
    objdump -h "${!kind}" | awk '$2 == ".text" {print $3, $4, $6}' > "$stage/$kind.layout"
done
cmp "$stage/release.text" "$stage/debug.text"
cmp "$stage/release.layout" "$stage/debug.layout"
(cd "$stage" && sha256sum release.text debug.text) > "$out/text.sha256"
cp "$stage/release.layout" "$out/text.layout"
grep -q '^34423ec81fa44ba7b4a0b139799c19be26e916cfd268c072b5e22e4ac19c9807 ' "$out/text.sha256"
{
    uname -a
    lscpu | grep '^Model name:'
    c++ --version | head -1
    perf --version
    printf 'CPU affinity: 6\n'
    grep -H . /sys/devices/system/cpu/cpu6/topology/thread_siblings_list \
        /sys/devices/system/cpu/cpufreq/policy6/{scaling_driver,scaling_governor} \
        /sys/devices/system/clocksource/clocksource0/current_clocksource "$profile" "${prefs[@]}"
    uptime
} > "$out/environment.txt"
if [[ ${4:-} ]]; then
    cp -r "$4" "$out/baseline"
else
    taskset -c 6 bash "$here/../g1-flat-input-2026-11-19/reproduce.sh" "$release" "$out/baseline" 9 full
fi
grep -Fxq '# energy preference: performance' "$out/baseline/snapshot.csv"
gzip -n "$out/baseline/snapshot-raw.csv"

# Two process blocks, with reversed row order and different sample frequencies.
printf 'block,scene,n,hz,start\n' > "$out/run-order.csv"
fixtures=(uniform:10000 uniform:100000 split-line:10000 split-line:100000
          split-line-translated:10000 split-line-translated:100000 clustered:100000)
for block in 1 2; do
    hz=2999; [[ $block == 1 ]] || hz=3469
    for i in "${!fixtures[@]}"; do
        j=$i; [[ $block == 1 ]] || j=$((${#fixtures[@]} - i - 1))
        fixture=${fixtures[j]}
        scene=${fixture%:*} n=${fixture#*:}
        check_mode
        run="$out/$block-$scene-$n"
        printf '%s,%s,%s,%s,%s\n' "$block" "$scene" "$n" "$hz" "$(date -Is)" >> "$out/run-order.csv"
        taskset -c 6 perf record -q -e cycles:u -F "$hz" --user-callchains --call-graph dwarf,4096 \
            -o "$stage/perf.data" -- "$release" --model=snapshot --types=int32 --scenes="$scene" \
            --engines=index --sizes="$n" --small-sizes= --rounds=1 --min-time=0.8 --max-time=1.2 \
            --max-reps=10000 --csv="$run.csv" > "$run.log"
        perf script -i "$stage/perf.data" -F period,ip,dso,dsoff | gzip -n > "$run.samples.gz"
        grep -Fxq '# energy preference: performance' "$run.csv"
    done
done
check_mode
python3 "$here/summarize.py" "$out" "$release" "$debug"
