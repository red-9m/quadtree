#!/usr/bin/env bash
# bash regress.sh CONTROL_EXE CANDIDATE_EXE OUT LONG_OUT
# G1 at two heap placements; one 64-step int32 persistent regression slice.
set -euo pipefail
control=$(realpath "$1") candidate=$(realpath "$2") out=$3 long=$4
screen=$(dirname "$(realpath "$0")")/../g1-tiny-snapshots-2026-11-26/screen.sh
export LC_ALL=C
cpu=${CPU:-6}
for root in "$out" "$long"; do
    for b in control candidate; do
        tag=base; [[ $b == control ]] || tag=cand
        dir=$root/g1-$tag; rc=0
        taskset -c "$cpu" bash "$screen" "${!b}" "$dir" full || rc=$?
        [[ $rc == 0 || $rc == 3 || $rc == 4 ]]
        gzip -nf "$dir/raw.csv"
    done
done
for b in control candidate; do
    dir=$out/persistent-$b; mkdir -p "$dir"
    taskset -c "$cpu" "${!b}" --model=persistent --types=int32 --scenes=split-line,mixed-size \
        --persistent-sizes=10000 --steps=64 --workloads=static,small-1%,teleport-1% --modes=full,moved \
        --engines=index,index-ordered,brute,sweep,grid,grid-rebuild,b2tree,b2tree-fat \
        --raw="$dir/raw.csv" --csv="$dir/persistent.csv" > "$dir/run.log"
    rc=0; "${!b}" --gate-files="$dir/persistent.csv" > "$dir/gates.txt" || rc=$?
    [[ $rc == 0 || $rc == 3 || $rc == 4 ]]
    gzip -nf "$dir/raw.csv"
done
