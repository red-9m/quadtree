#!/usr/bin/env bash
# Usage: bash screen.sh /path/to/Release/quadtree-bench output-dir
set -eu
exe=$(realpath "$1"); dir=$(realpath -m "$2"); mkdir -p "$dir"
run() {
    local f="$dir/$1"; shift
    taskset -c 6 "$exe" --model=snapshot --engines=index,index-ordered,brute,sweep,grid "$@" \
        --csv="$f.csv" --raw="$f.raw.csv" > "$f.log"
    local rc=0; "$exe" --gate-files="$f.csv" > "$f.gates" || rc=$?
    [[ $rc == 0 || $rc == 3 || $rc == 4 ]]
}
for i in 1 2; do run "$i" --types=int64 --scenes=uniform --sizes=100000 --small-sizes= --rounds=45; done
for t in int64 int32; do run "$t-matrix" --types="$t" --scenes=uniform,clustered,mixed-size,split-line,split-line-translated; done
