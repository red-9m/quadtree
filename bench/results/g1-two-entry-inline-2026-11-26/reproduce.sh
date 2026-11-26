#!/usr/bin/env bash
# Reproduce the screens in this directory on an idle core (CPU 6 here).
#   bash reproduce.sh CONTROL_EXE CANDIDATE_EXE OUT
# Executables: Release quadtree-bench (-DQUADTREE_BUILD_BENCHMARKS=ON) at dc26b4f and at this commit.
# OUT path length matters: it shifts the harness heap (see README, "Limits").
set -eu
base=$1 cand=$2 out=$3
screen=$(dirname "$(realpath "$0")")/../g1-tiny-snapshots-2026-11-26/screen.sh
cd "$(dirname "$(realpath "$0")")/../../.."
# screen.sh exits 3 (fail) or 4 (inconclusive) from the gate evaluator; keep going.
run() { local exe=$1 mode=$2 dir=$3; mkdir -p "$dir"; taskset -c 6 bash "$screen" "$exe" "$dir" "$mode" || true; }
for i in 1 2 3; do run "$cand" full "$out/full-cand-$i"; run "$base" full "$out/full-base-$i"; done
for i in 1 2; do run "$base" target "$out/target-base-$i"; run "$cand" target "$out/target-cand-$i"; done
for t in int32 int64 float double; do
    TYPE=$t run "$cand" per-type "$out/per-type-cand-$t"; TYPE=$t run "$base" per-type "$out/per-type-base-$t"
done
# Isolated target row, 81 rounds, full engine set.
for i in 1 2; do
    for b in cand base; do
        exe=$cand; [[ $b == base ]] && exe=$base
        o=$out/iso-$b-$i; mkdir -p "$o"
        taskset -c 6 "$exe" --model=snapshot --min-time=0.03 --min-reps=3 --max-reps=12 --types=int32 --scenes=uniform \
            --engines=index,index-ordered,qt-loose-sort,sweep,grid,brute --sizes= --small-sizes=2 --batch=256 --rounds=81 \
            --raw="$o/raw.csv" --csv="$o/snapshot.csv" > "$o/run.log"
        "$exe" --gate-files="$o/snapshot.csv" > "$o/gates.txt" || true
    done
done
run "$cand" resolve "$out/resolve-cand-1"; run "$base" resolve "$out/resolve-base-1"
for i in 1 2; do run "$cand" large "$out/large-cand-$i"; run "$base" large "$out/large-base-$i"; done
# 45-round int64 split-line follow-up for the 5-round G2 inconclusives.
for i in 1 2; do
    for b in cand base; do
        exe=$cand; [[ $b == base ]] && exe=$base
        o=$out/resolve64-$b-$i; mkdir -p "$o"
        taskset -c 6 "$exe" --model=snapshot --types=int64 --scenes=split-line,split-line-translated \
            --engines=index,index-ordered,brute,sweep,grid --sizes=100,100000 --small-sizes= --rounds=45 \
            --csv="$o/snapshot.csv" > "$o/run.log"
        "$exe" --gate-files="$o/snapshot.csv" > "$o/gates.txt" || true
    done
done
