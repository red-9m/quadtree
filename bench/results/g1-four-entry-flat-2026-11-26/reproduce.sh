#!/usr/bin/env bash
# bash reproduce.sh CONTROL_EXE CANDIDATE_EXE OUT SHORT_OUT LONG_OUT
# OUT, SHORT_OUT and LONG_OUT should give distinct path lengths (see README).
# Use an idle core; CPU=6 by default. Binaries are Release quadtree-bench.
set -euo pipefail
base=$(realpath "$1") cand=$(realpath "$2") out=$3 short=$4 long=$5
screen=$(dirname "$(realpath "$0")")/../g1-tiny-snapshots-2026-11-26/screen.sh
run() {
    local b=$1 exe=$2 mode=$3 dir=$4 rc=0
    printf '%s %s %s %s\n' "$(date +%T)" "$b" "$mode" "$dir"
    taskset -c "${CPU:-6}" bash "$screen" "$exe" "$dir" "$mode" || rc=$?
    [[ $rc == 0 || $rc == 3 || $rc == 4 ]]
    test -s "$dir/gates.txt"
}
match() {
    run base "$base" "$1" "$2-base-$3"
    run cand "$cand" "$1" "$2-cand-$3"
}
gate() {
    local rc=0
    "$1" --gate-files="$2/snapshot.csv" > "$2/gates.txt" || rc=$?
    [[ $rc == 0 || $rc == 3 || $rc == 4 ]]
}
for i in 1 2 3; do match full "$out/full" "$i"; done
match full "$short/full" 1
match full "$long/full" 1
for i in 1 2; do match target "$out/target" "$i"; done
for t in int32 int64 float double; do TYPE=$t match per-type "$out/per-type" "$t"; done
for i in 1 2; do
    for b in base cand; do
        exe=${!b} dir=$out/iso-$b-$i; mkdir -p "$dir"
        printf '%s %s isolated int64 uniform n=4\n' "$(date +%T)" "$b"
        taskset -c "${CPU:-6}" "$exe" --model=snapshot --min-time=0.03 --min-reps=3 --max-reps=12 --types=int64 \
            --scenes=uniform --engines=index,index-ordered,qt-loose-sort,sweep,grid,brute --sizes= \
            --small-sizes=4 --batch=256 --rounds=81 --raw="$dir/raw.csv" --csv="$dir/snapshot.csv" > "$dir/run.log"
        gate "$exe" "$dir"
    done
done
match resolve "$out/resolve" 1
for i in 1 2; do match large "$out/large" "$i"; done
for i in 1 2; do
    for b in base cand; do
        exe=${!b} dir=$out/resolve64-$b-$i; mkdir -p "$dir"
        printf '%s %s resolve64\n' "$(date +%T)" "$b"
        taskset -c "${CPU:-6}" "$exe" --model=snapshot --types=int64 --scenes=split-line,split-line-translated \
            --engines=index,index-ordered,brute,sweep,grid --sizes=100,100000 --small-sizes= --rounds=45 \
            --csv="$dir/snapshot.csv" > "$dir/run.log"
        gate "$exe" "$dir"
    done
done
# Recheck the long-path singleton and the int32 split-line G9 rows separately.
match full "$long/full" 2
match resolve "$out/resolve" 2
