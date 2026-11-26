#!/usr/bin/env bash
# 679a2b2 validation; same staged power, timing and quiet-start protocol.
# usage: run.sh WORK OUT [STAGE...]   stage = <int32|int64|float|double>-<snapshot|persistent|resolve|resolve-2>
# WORK/build/bench/quadtree-bench must be the Release build of the recorded source.
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1") out=$(realpath -m "$2"); shift 2
record=${QT_VALIDATION_DIR:-$here}
candidate=679a2b2106815ac459f8d2d12a0a7ed9905eb033
if [[ $record != "$here" ]]; then candidate=$(<"$record/source-head.txt"); fi
stages=("$@"); ((${#stages[@]})) || stages=(int32-snapshot int64-snapshot int32-persistent int64-persistent)
exe="$w/build/bench/quadtree-bench"
mkdir -p "$out"
stage_dir=$(mktemp -d)
objcopy --dump-section ".text=$stage_dir/bench.text" "$exe" "$stage_dir/copy"
(cd "$stage_dir" && sha256sum -c "$record/text.sha256") > "$out/text-check-$(date +%s).log"
rm -rf "$stage_dir"
[[ $(git -C "$w/box2d" rev-parse HEAD) == 178495ba35695aefe9c321e569d2277f06db3b90 ]]

snapshot_engines=index,index-ordered,brute,sweep,grid
persistent_engines=index,index-ordered,brute,sweep,grid,grid-rebuild,b2tree,b2tree-fat
args_for() {
    local type=${1%%-*}
    case ${1#*-} in
        snapshot) echo --model=snapshot --types=$type --engines=$snapshot_engines ;;
        persistent) echo --model=persistent --types=$type --engines=$persistent_engines ;;
        # Separate 45-round processes for rows left inconclusive by the 5-round matrix (MANIFEST: rerun with more rounds).
        resolve|resolve-2) echo --model=snapshot --types=$type --engines=$snapshot_engines ${QT_RESOLVE_ARGS:?} --rounds=45 ;;
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
    printf 'candidate: %s\nharness: ' "$candidate"; git -C "$here" rev-parse HEAD; uname -a; lscpu | grep '^Model name:'
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
    [[ $rc == 0 && ( $grc == 0 || $grc == 3 || $grc == 4 ) ]]
done
