#!/usr/bin/env bash
# Diagnostic phase screen only; never use the modified ELF to evaluate gates.
set -euo pipefail
export LC_ALL=C
w=$(realpath "$1")
out=$(realpath -m "$2")
here=$(dirname "$(realpath "$0")")
ref="$here/../g9-home-lookahead-2026-11-20/text.sha256"
cpu=${CPU:-6}
mkdir "$out"
stage=$(mktemp -d)
mode=/sys/firmware/acpi/platform_profile
files=("$mode" /sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference)
original=()
for f in "${files[@]}"; do original+=("$(<"$f")"); done
grep -H . "${files[@]}" > "$out/power-before.txt"
restore() {
    for i in "${!files[@]}"; do printf '%s\n' "${original[i]}" > "${files[i]}"; done
    grep -H . "${files[@]}" > "$out/power-after.txt"
    rm -rf "$stage"
    cmp "$out/power-before.txt" "$out/power-after.txt"
}
trap restore EXIT
cp -p "$w/control-build/bench/quadtree-bench" "$stage/control"
cp -p "$w/candidate-build/bench/quadtree-bench" "$stage/hintbin"
python3 "$here/make-twin.py" "$stage/hintbin" "$stage/nohintx" > "$out/twin-verification.log"
objcopy --dump-section ".text=$stage/control.text" "$stage/control" "$stage/control.copy"
(cd "$stage" && sha256sum control.text) > "$out/control-text.sha256"
grep -Fxq "$(grep '  control.text$' "$ref")" "$out/control-text.sha256"
(cd "$stage" && sha256sum control hintbin nohintx) > "$out/elf.sha256"
for f in "${files[@]}"; do printf 'performance\n' > "$f"; done
for f in "${files[@]}"; do [[ $(<"$f") == performance ]]; done
{
    date -Is; uname -a; lscpu | grep '^Model name:'; c++ --version | head -1
    git -C "$w/box2d" rev-parse HEAD
    printf 'CPU affinity: %s\n' "$cpu"
    grep -H . "${files[@]}" /sys/devices/system/cpu/cpu"$cpu"/topology/thread_siblings_list \
         /sys/devices/system/cpu/cpufreq/policy"$cpu"/{scaling_driver,scaling_governor} \
         /sys/devices/system/clocksource/clocksource0/current_clocksource
} > "$out/environment.txt"
# All six permutations counterbalance the three treatments. The alternating
# filter spelling is recorded; the benchmark visits both scenes in kScenes order.
orders=('control hintbin nohintx' 'nohintx hintbin control' 'hintbin control nohintx' \
        'nohintx control hintbin' 'hintbin nohintx control' 'control nohintx hintbin')
printf 'block,kind,scene_filter,start,finish\n' > "$out/run-order.csv"
for b in "${!orders[@]}"; do
    scenes=uniform,split-line-translated
    if (( b % 2 )); then scenes=split-line-translated,uniform; fi
    for kind in ${orders[b]}; do
        start=$(date -Is)
        run="$out/$((b + 1))-$kind"
        taskset -c "$cpu" "$stage/$kind" --model=snapshot --types=int32 --scenes="$scenes" \
            --engines=index,index-ordered --sizes=100000 --small-sizes= --rounds=15 --min-time=0.03 \
            --min-reps=3 --max-reps=12 --csv="$run.csv" --raw="$run-raw.csv" > "$run.log"
        printf '%s,%s,%s,%s,%s\n' "$((b + 1))" "$kind" "${scenes/,/:}" "$start" "$(date -Is)" >> "$out/run-order.csv"
        gzip -n "$run-raw.csv"
    done
done
