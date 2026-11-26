#!/usr/bin/env bash
# Run on an idle core: taskset -c 6 bash screen.sh EXE OUT [full|target|per-type|large|resolve]
# TYPE=int32 (or int64, float, double) selects a single type for target/per-type.
set -eu
exe=$1 out=$2
mkdir -p "$out"
args=(--model=snapshot --min-time=0.03 --min-reps=3 --max-reps=12)
case ${3:-full} in
    full|target)
        sizes=0,1,2,4,8,16,32,64; rounds=81
        if [[ ${3:-full} == target ]]; then sizes=0,1,2; rounds=117; fi
        args+=(--types="${TYPE:-int32,int64,float,double}" --scenes=uniform,clustered
            --engines=index,index-ordered,qt-loose-sort,sweep,grid,brute --sizes=
            --small-sizes="$sizes" --batch=256 --rounds="$rounds" --raw="$out/raw.csv") ;;
    per-type)
        args=(--model=snapshot --types="${TYPE:?set TYPE}" --scenes=uniform,clustered
            --engines=index,index-ordered,brute,sweep,grid --sizes= --small-sizes=0,1,2 --rounds=45 --raw="$out/raw.csv") ;;
    resolve)
        args=(--model=snapshot --types=int32 --scenes=split-line,split-line-translated
            --engines=index,index-ordered,brute,sweep,grid --sizes=100,100000 --small-sizes= --rounds=45) ;;
    large)
        args+=(--types=int32,int64 --scenes=uniform,clustered,split-line,split-line-translated,mixed-size
            --engines=index,index-ordered,brute,sweep,grid --sizes=100,1000,10000,100000 --small-sizes= --rounds=5) ;;
    *) exit 2 ;;
esac
"$exe" "${args[@]}" --csv="$out/snapshot.csv" > "$out/run.log"
"$exe" --gate-files="$out/snapshot.csv" > "$out/gates.txt"
