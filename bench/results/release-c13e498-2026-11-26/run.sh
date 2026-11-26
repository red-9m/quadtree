#!/usr/bin/env bash
# c13e498 release validation: same staged power, timing and quiet-start protocol as packed-staged-2026-11-23.
# usage: run.sh WORK OUT [STAGE...]
#   stage = calib-a | calib-b | <int32|int64|float|double>-<snapshot|persistent|resolve|resolve-2|presolve|presolve-2>
# WORK/build/bench/quadtree-bench must be the Release build of source-head.txt (archive build, fetched Box2D).
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1") out=$(realpath -m "$2"); shift 2
candidate=$(<"$here/source-head.txt")
stages=("$@"); ((${#stages[@]})) || { echo "no stages" >&2; exit 2; }
exe="$w/build/bench/quadtree-bench"
mkdir -p "$out"
stage_dir=$(mktemp -d)
objcopy --dump-section ".text=$stage_dir/bench.text" "$exe" "$stage_dir/copy"
(cd "$stage_dir" && sha256sum -c "$here/text.sha256") > "$out/text-check-$(date +%s).log"
rm -rf "$stage_dir"
# Box2D: the v3.2.0 archive pinned by URL_HASH in bench/CMakeLists.txt, not a local source override.
box2d_tar="$w/build/_deps/box2d-subbuild/box2d-populate-prefix/src/v3.2.0.tar.gz"
[[ $(sha256sum < "$box2d_tar") == "ffec747b27a2267016f8d0240d658c82d187fb2271fa0f69fcbe53fbc067180d  -" ]]
grep -qx 'FETCHCONTENT_SOURCE_DIR_BOX2D:PATH=' "$w/build/CMakeCache.txt"

snapshot_engines=index,index-ordered,brute,sweep,grid
persistent_engines=index,index-ordered,brute,sweep,grid,grid-rebuild,b2tree,b2tree-fat
args_for() {
    case $1 in
        calib-a|calib-b) echo --model=snapshot --types=int32 --engines=$snapshot_engines; return ;;
    esac
    local type=${1%%-*}
    case ${1#*-} in
        snapshot) echo --model=snapshot --types=$type --engines=$snapshot_engines ;;
        persistent) echo --model=persistent --types=$type --engines=$persistent_engines ;;
        # Separate 45-round processes for hard rows left inconclusive by the matrix (resolution-rule.md).
        resolve|resolve-2) echo --model=snapshot --types=$type --engines=$snapshot_engines ${QT_RESOLVE_ARGS:?} --rounds=45 ;;
        presolve|presolve-2) echo --model=persistent --types=$type --engines=$persistent_engines ${QT_PRESOLVE_ARGS:?} --rounds=45 ;;
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
    c++ --version | head -1; cmake --version | head -1
    printf 'box2d archive: '; sha256sum < "$box2d_tar"
    printf 'CPU affinity: 6; monitor CPU: 0\n'
    grep -H . "$clock" "$siblings" "${files[@]}" /sys/devices/system/cpu/cpufreq/policy{6,7}/scaling_{driver,governor} \
        /sys/devices/system/cpu/cpufreq/boost /sys/devices/system/cpu/amd_pstate/status /sys/class/power_supply/*/online
} > "$out/environment.txt"

temp=
for f in /sys/class/hwmon/hwmon*/name; do
    if [[ $(<"$f") == k10temp ]]; then temp="${f%/name}/temp1_input"; break; fi
done
freq=/sys/devices/system/cpu/cpufreq/policy6/scaling_cur_freq
[[ -n $temp && -r $temp && -r $freq ]]
(
    taskset -pc 0 "$BASHPID" >/dev/null
    [[ -s $out/thermal.csv ]] || printf 'unix_seconds,tctl_millidegrees,cpu6_scaling_khz\n'
    while :; do printf '%s,%d,%d\n' "$EPOCHREALTIME" "$(<"$temp")" "$(<"$freq")"; sleep 5; done
) >> "$out/thermal.csv" &
monitor=$!
[[ -s $out/run-order.csv ]] || printf 'stage,start,finish,start_tctl_millidegrees,finish_tctl_millidegrees,exit\n' > "$out/run-order.csv"
quiet="$here/../g2-two-scatter-repeatability-2026-11-21/quiet-start.py"
for stage in "${stages[@]}"; do
    dir="$out/$stage"
    mkdir "$dir"
    read -r -a args <<< "$(args_for "$stage")"
    printf '%s\n' "${args[*]}" > "$dir/args.txt"
    printf '%s\n' "$dir/results.csv" "$dir/raw.csv" | awk '{ print length($0), $0 }' > "$dir/path-lengths.txt"
    taskset -c 0 python3 "$quiet" "$temp" "$dir/quiet-start.csv" > /dev/null
    start=$(date -Is); start_temp=$(<"$temp")
    rc=0
    taskset -c 6 /usr/bin/time -p "$exe" "${args[@]}" --csv="$dir/results.csv" --raw="$dir/raw.csv" \
        > "$dir/run.log" 2> "$dir/time.log" || rc=$?
    printf '%s,%s,%s,%s,%s,%s\n' "$stage" "$start" "$(date -Is)" "$start_temp" "$(<"$temp")" "$rc" >> "$out/run-order.csv"
    gzip -n "$dir/raw.csv"
    grc=0
    "$exe" --gate-files="$dir/results.csv" > "$dir/gates.txt" || grc=$?
    printf '%s %s\n' "$rc" "$grc" > "$dir/exit.txt"
    [[ $rc == 0 && ( $grc == 0 || $grc == 3 || $grc == 4 ) ]]
done
