#!/usr/bin/env bash
# Task 08 release matrix: calibration pair, full snapshot matrix, full persistent matrix.
# usage: run.sh WORK OUT [STAGE...]   (default stages: calib-a calib-b snapshot persistent)
# WORK/build/bench/quadtree-bench must be the Release build of the recorded source.
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1") out=$(realpath -m "$2"); shift 2
stages=("$@"); ((${#stages[@]})) || stages=(calib-a calib-b snapshot persistent)
exe="$w/build/bench/quadtree-bench"
mkdir -p "$out"
stage_dir=$(mktemp -d)
objcopy --dump-section ".text=$stage_dir/bench.text" "$exe" "$stage_dir/copy"
(cd "$stage_dir" && sha256sum -c "$here/text.sha256") > "$out/text-check-$(date +%s).log"
rm -rf "$stage_dir"
[[ $(git -C "$w/box2d" rev-parse HEAD) == 178495ba35695aefe9c321e569d2277f06db3b90 ]]

snapshot_engines=index,index-ordered,brute,sweep,grid
persistent_engines=index,index-ordered,brute,sweep,grid,grid-rebuild,b2tree,b2tree-fat
args_for() {
    case $1 in
        calib-a|calib-b) echo --model=snapshot --types=int32 --engines=$snapshot_engines ;;
        snapshot) echo --model=snapshot --types=all --engines=$snapshot_engines ;;
        persistent) echo --model=persistent --types=all --engines=$persistent_engines ;;
        # Separate 45-round processes for rows left inconclusive by the 5-round matrix (MANIFEST: rerun with more rounds).
        resolve-int32) echo --model=snapshot --types=int32 --engines=$snapshot_engines \
            --scenes=uniform,split-line-translated --sizes=100,1000,100000 --small-sizes= --rounds=45 ;;
        resolve-int64) echo --model=snapshot --types=int64 --engines=$snapshot_engines \
            --scenes=uniform,clustered,mixed-size --sizes=100,1000,100000 --small-sizes= --rounds=45 ;;
        *) echo "unknown stage $1" >&2; return 1 ;;
    esac
}

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
    cmp "$out/power-before.txt" "$out/power-after.txt"
}
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
for f in "${files[@]}"; do printf 'performance\n' > "$f"; done
{
    git -C "$here" rev-parse HEAD; uname -a; lscpu | grep '^Model name:'
    c++ --version | head -1; cmake --version | head -1; git -C "$w/box2d" rev-parse HEAD
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
) >> "$out/thermal.csv" &
monitor=$!
[[ -s $out/run-order.csv ]] || printf 'stage,start,finish,start_tctl_millidegrees,finish_tctl_millidegrees,exit\n' > "$out/run-order.csv"
quiet="$here/../g2-two-scatter-repeatability-2026-11-21/quiet-start.py"
for stage in "${stages[@]}"; do
    dir="$out/$stage"
    mkdir "$dir"
    read -r -a args <<< "$(args_for "$stage")"
    taskset -c 0 python3 "$quiet" "$temp" "$dir/quiet-start.csv" > /dev/null
    start=$(date -Is); start_temp=$(<"$temp")
    rc=0
    taskset -c 6 /usr/bin/time -p "$exe" "${args[@]}" --csv="$dir/results.csv" --raw="$dir/raw.csv" \
        > "$dir/run.log" 2> "$dir/time.log" || rc=$?
    printf '%s,%s,%s,%s,%s,%s\n' "$stage" "$start" "$(date -Is)" "$start_temp" "$(<"$temp")" "$rc" >> "$out/run-order.csv"
    printf '%s\n' "${args[*]}" > "$dir/args.txt"
    gzip -n "$dir/raw.csv"
    grc=0
    "$exe" --gate-files="$dir/results.csv" > "$dir/gates.txt" || grc=$?
    printf '%s %s\n' "$rc" "$grc" > "$dir/exit.txt"
done
