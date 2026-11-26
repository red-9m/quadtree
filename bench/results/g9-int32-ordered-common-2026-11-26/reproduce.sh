#!/usr/bin/env bash
# bash reproduce.sh CONTROL_EXE CANDIDATE_EXE OUT
# Release, default benchmark timing/caps, pinned Box2D. Use an idle CPU (default 6).
set -euo pipefail
control=$(realpath "$1") candidate=$(realpath "$2") out=$3
export LC_ALL=C
cpu=${CPU:-6}
run() {
    local b=$1 stage=$2 dir=$out/$1-$2 exe=${!1} rc=0
    shift 2
    mkdir -p "$dir"
    printf '%s %s %s\n' "$(date -Is)" "$b" "$stage"
    taskset -c "$cpu" /usr/bin/time -p "$exe" "$@" --raw="$dir/raw.csv" --csv="$dir/snapshot.csv" > "$dir/run.log" 2> "$dir/time.log"
    "$exe" --gate-files="$dir/snapshot.csv" > "$dir/gates.txt" || rc=$?
    [[ $rc == 0 || $rc == 3 || $rc == 4 ]]
    gzip -nf "$dir/raw.csv"
}
snapshot=(--model=snapshot --types=int32 --engines=index,index-ordered,brute,sweep,grid)
target=("${snapshot[@]}" --scenes=split-line,split-line-translated --sizes=100000 --small-sizes= --rounds=45)
for i in 1 2 3; do
    if ((i % 2)); then order=(control candidate); else order=(candidate control); fi
    for b in "${order[@]}"; do run "$b" "target-$i" "${target[@]}"; done
done
for b in control candidate; do run "$b" matrix "${snapshot[@]}"; done
