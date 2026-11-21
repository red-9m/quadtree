#!/usr/bin/env bash
# Matched-start phase and gate screen on the exact frozen snapshot slice.
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1") out=$(realpath -m "$2")
rounds_to_run=(9 15)
scope=full
final=()
pilot=0
confirm=0
if [[ ${3:-} == --pilot && $# == 3 ]]; then
    rounds_to_run=(1 2)
    pilot=1
elif [[ ${3:-} == --confirm-45 && $# == 3 ]]; then
    rounds_to_run=(45)
    confirm=1
elif [[ ${3:-} == --g1 && $# == 3 ]]; then
    rounds_to_run=(81 117)
    scope=g1
elif [[ ${3:-} == --g1-117 && $# == 3 ]]; then
    rounds_to_run=(117)
    scope=g1
elif (($# > 2)); then
    [[ $# == 4 && $3 == --final-control && $4 =~ ^[0-9]+$ && $4 -le 40000 ]]
    rounds_to_run=(15)
    final=("$4")
fi
runner="$here/../g1-flat-input-2026-11-19/reproduce.sh"
thermal="$here/../g2-two-scatter-repeatability-2026-11-21"
control_ref=$(realpath "${QT_MATCH_CONTROL_RESULT:-$here/../g2-two-scatter-2026-11-21}")
candidate_ref=$(realpath "${QT_MATCH_CANDIDATE_RESULT:-$here}")
control_elf_hash=${QT_MATCH_CONTROL_ELF_HASH:-$here/../g2-two-scatter-thermal-2026-11-21/frozen/executable.sha256}
mkdir "$out"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for kind in control candidate; do
    if [[ $kind == control ]]; then ref=$control_ref; else ref=$candidate_ref; fi
    (cd "$w/$kind" && sha256sum -c --quiet "$ref/candidate-inputs.sha256")
    objcopy --dump-section ".text=$stage/$kind.text" "$w/$kind-build/bench/quadtree-bench" "$stage/$kind.copy"
done
(cd "$stage" && sha256sum -c "$candidate_ref/text.sha256") > "$out/text-check.log"
sha256sum < "$w/control-build/bench/quadtree-bench" | awk '{print $1 "  quadtree-bench"}' | cmp "$control_elf_hash" -
[[ $(git -C "$w/box2d" rev-parse HEAD) == 178495ba35695aefe9c321e569d2277f06db3b90 ]]

clock=/sys/devices/system/clocksource/clocksource0/current_clocksource
siblings=/sys/devices/system/cpu/cpu6/topology/thread_siblings_list
files=(/sys/firmware/acpi/platform_profile /sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference)
[[ $(<"$clock") == hpet && $(<"$siblings") == 6-7 ]]
original=()
for f in "${files[@]}"; do [[ -w $f ]]; original+=("$(<"$f")"); done
grep -H . "${files[@]}" > "$out/power-before.txt"
monitor=
restore() {
    if [[ -n $monitor ]]; then kill "$monitor" 2>/dev/null || true; wait "$monitor" 2>/dev/null || true; fi
    for i in "${!files[@]}"; do printf '%s\n' "${original[i]}" > "${files[i]}"; done
    grep -H . "${files[@]}" > "$out/power-after.txt"
    rm -rf "$stage"
    cmp "$out/power-before.txt" "$out/power-after.txt"
}
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
for f in "${files[@]}"; do printf 'performance\n' > "$f"; done
for f in "${files[@]}"; do [[ $(<"$f") == performance ]]; done
{
    git -C "$here" rev-parse HEAD; uname -a; lscpu | grep '^Model name:'
    c++ --version | head -1; git -C "$w/box2d" rev-parse HEAD
    printf 'CPU affinity: 6; monitor CPU: 0\n'
    grep -H . "$clock" "$siblings" "${files[@]}" /sys/class/power_supply/*/online
} > "$out/environment.txt"

temp=
for f in /sys/class/hwmon/hwmon*/name; do
    if [[ $(<"$f") == k10temp ]]; then temp="${f%/name}/temp1_input"; break; fi
done
freq=/sys/devices/system/cpu/cpufreq/policy6/scaling_cur_freq
[[ -n $temp && -r $temp && -r $freq ]]
(
    taskset -pc 0 "$BASHPID" >/dev/null
    printf 'unix_seconds,tctl_millidegrees,cpu6_scaling_khz\n'
    while :; do printf '%s,%d,%d\n' "$EPOCHREALTIME" "$(<"$temp")" "$(<"$freq")"; sleep 5; done
) > "$out/thermal.csv" &
monitor=$!
printf 'run,start,finish,start_tctl_millidegrees,finish_tctl_millidegrees\n' > "$out/run-order.csv"
for rounds in "${rounds_to_run[@]}"; do
    kinds=(control candidate); [[ $rounds == 9 || $rounds == 1 || $rounds == 81 ]] || kinds=(candidate control)
    if ((confirm)); then kinds=(candidate); fi
    target=()
    if ((${#final[@]} != 0)); then kinds=(control); target=("${final[0]}"); fi
    for kind in "${kinds[@]}"; do
        run="$rounds-$kind"
        ready=0
        for attempt in 1 2 3; do
            suffix=; [[ $attempt == 1 ]] || suffix="-retry-$attempt"
            taskset -c 0 python3 "$thermal/quiet-start.py" "$temp" "$out/quiet-start-$run$suffix.csv" "${target[@]}" >/dev/null
            start=$(date -Is); start_temp=$(<"$temp")
            diff=$((start_temp - ${target[0]:-$start_temp}))
            if ((start_temp <= 40000 && diff >= -1000 && diff <= 1000)); then ready=1; break; fi
            printf '%s attempt %s: launch Tctl %s m°C outside band; benchmark not run\n' \
                "$run" "$attempt" "$start_temp" >> "$out/start-rejections.log"
        done
        if ((ready == 0)); then printf 'Launch temperature not matched for %s\n' "$run" >&2; exit 1; fi
        if ((${#target[@]} == 0)); then target=("$start_temp"); fi
        exe="$w/$kind-build/bench/quadtree-bench"
        if ((pilot)); then
            mkdir "$out/$run"
            taskset -c 6 "$exe" --model=snapshot --types=int32 --scenes=uniform,split-line,split-line-translated,mixed-size \
                --engines=index,index-ordered --sizes=10000,100000 --small-sizes= --rounds=9 --min-time=0.03 \
                --min-reps=3 --max-reps=12 --csv="$out/$run/snapshot.csv" --raw="$out/$run/snapshot-raw.csv" > "$out/$run/run.log"
        else
            taskset -c 6 bash "$runner" "$exe" "$out/$run" "$rounds" "$scope"
        fi
        printf '%s,%s,%s,%s,%s\n' "$run" "$start" "$(date -Is)" "$start_temp" "$(<"$temp")" >> "$out/run-order.csv"
        gzip -n "$out/$run/snapshot-raw.csv"
        grep -Fxq '# energy preference: performance' "$out/$run/snapshot.csv"
    done
done
