#!/usr/bin/env bash
# bash reproduce.sh CONTROL_EXE CANDIDATE_EXE OUT [STAGE...]
# Release builds (-O3 -DNDEBUG, QUADTREE_BENCH_NATIVE=OFF, pinned Box2D v3.2.0) of the control and candidate.
# Stages (default all, in this order):
#   matrix   default 5-round snapshot matrix per type, binaries alternating first
#   targets  two 45-round processes per binary on the float/double rows open at c13e498
#   persist  64-step persistent slice per type (split-line, mixed-size, n=10000)
# CPU 6 (sibling 7 idle), EPP and platform profile `performance` (restored), quiet starts.
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")")
control=$(realpath "$1") candidate=$(realpath "$2") out=$(realpath -m "$3"); shift 3
stages=("$@"); ((${#stages[@]})) || stages=(matrix targets persist)
mkdir -p "$out"
quiet="$here/../g2-two-scatter-repeatability-2026-11-21/quiet-start.py"
temp=$(grep -l '^k10temp$' /sys/class/hwmon/hwmon*/name); temp=${temp%/name}/temp1_input
files=(/sys/firmware/acpi/platform_profile /sys/devices/system/cpu/cpufreq/policy*/energy_performance_preference)
original=()
for f in "${files[@]}"; do [[ -w $f ]]; original+=("$(<"$f")"); done
restore() { for i in "${!files[@]}"; do printf '%s\n' "${original[i]}" > "${files[i]}"; done; }
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
for f in "${files[@]}"; do printf 'performance\n' > "$f"; done
[[ -s $out/run-order.csv ]] || printf 'run,start,finish,start_tctl_millidegrees,finish_tctl_millidegrees,exit,gate_exit\n' > "$out/run-order.csv"

run() { # run BINARY NAME ARGS...; equal-length tags keep output paths (and process layout) alike
    local b=$1 name=$2 exe dir tag rc=0 grc=0 start start_temp
    shift 2
    tag=base; [[ $b == control ]] || tag=cand
    exe=${!b}; dir=$out/$name-$tag
    mkdir "$dir"
    printf '%s\n' "$*" > "$dir/args.txt"
    taskset -c 0 python3 "$quiet" "$temp" "$dir/quiet-start.csv" > /dev/null
    start=$(date -Is); start_temp=$(<"$temp")
    taskset -c 6 /usr/bin/time -p "$exe" "$@" --csv="$dir/results.csv" --raw="$dir/raw.csv" \
        > "$dir/run.log" 2> "$dir/time.log" || rc=$?
    gzip -n "$dir/raw.csv"
    "$exe" --gate-files="$dir/results.csv" > "$dir/gates.txt" || grc=$?
    printf '%s,%s,%s,%s,%s,%s,%s\n' "$name-$tag" "$start" "$(date -Is)" "$start_temp" "$(<"$temp")" "$rc" "$grc" \
        >> "$out/run-order.csv"
    [[ $rc == 0 && ( $grc == 0 || $grc == 3 || $grc == 4 ) ]]
}
pair() { # pair FIRST NAME ARGS...: both binaries, FIRST (control|candidate) first
    local first=$1 second=candidate
    shift
    [[ $first == control ]] || second=control
    run "$first" "$@"
    run "$second" "$@"
}
snapshot=(--model=snapshot --engines=index,index-ordered,brute,sweep,grid)
for stage in "${stages[@]}"; do
    case $stage in
        matrix)
            pair control matrix-float "${snapshot[@]}" --types=float
            pair candidate matrix-double "${snapshot[@]}" --types=double
            pair control matrix-int32 "${snapshot[@]}" --types=int32
            pair candidate matrix-int64 "${snapshot[@]}" --types=int64 ;;
        targets)
            target=("${snapshot[@]}" --types=float,double --scenes=uniform,split-line-translated
                --sizes=1000,100000 --small-sizes= --rounds=45)
            pair candidate targets-1 "${target[@]}"
            pair control targets-2 "${target[@]}" ;;
        persist)
            for t in float double int32 int64; do
                pair control "persist-$t" --model=persistent --types=$t --scenes=split-line,mixed-size \
                    --persistent-sizes=10000 --steps=64 --workloads=static,small-1%,teleport-1% --modes=full,moved \
                    --engines=index,index-ordered,brute,sweep,grid,grid-rebuild,b2tree,b2tree-fat
            done ;;
        *) echo "unknown stage $stage" >&2; exit 2 ;;
    esac
done
