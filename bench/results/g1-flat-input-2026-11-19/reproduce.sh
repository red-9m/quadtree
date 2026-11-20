#!/usr/bin/env bash
set -eu
exe=$1 out=$2 rounds=$3 scope=${4:-g1}
mkdir -p "$out"
scenes=uniform,clustered; sizes=
if [[ $scope == full ]]; then scenes=uniform,clustered,split-line,split-line-translated,mixed-size,outliers; sizes=1000,10000,100000; fi
/usr/bin/time -p "$exe" --model=snapshot --types=int32 --scenes="$scenes" --engines=index,index-ordered,qt-loose-sort,sweep,grid,brute --sizes="$sizes" --small-sizes=0,1,2,4,8,16,32,64 --batch=256 --rounds="$rounds" --min-time=0.03 --min-reps=3 --max-reps=12 --csv="$out/snapshot.csv" --raw="$out/snapshot-raw.csv" > "$out/run.log" 2> "$out/time.log"
printf '0\n' > "$out/run.exit"
rc=0
"$exe" --gate-files="$out/snapshot.csv" > "$out/gates.txt" || rc=$?
echo "$rc" > "$out/gates.exit"
