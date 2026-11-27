#!/usr/bin/env bash
# Owner-requested rerun (rerun-rule.md): calibration pair + int32 snapshot matrix + the rule's 45-round repeats.
# usage: bash rerun.sh WORK [OUT]   (OUT default: ./data-rerun)
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")"); w=$(realpath "$1"); out=$(realpath -m "${2:-$here/data-rerun}")
exe="$w/build/bench/quadtree-bench"
run() { bash "$here/run.sh" "$w" "$out" "$@"; }

run calib-a calib-b
rc=0; "$exe" --diff="$out/calib-a/results.csv,$out/calib-b/results.csv" > "$out/calib-diff.txt" || rc=$?
echo "$rc" > "$out/calib-diff.exit"

run int32-snapshot
QT_RESOLVE_ARGS=$(python3 - "$out/int32-snapshot/gates.txt" <<'PY'
import re, sys
scenes, sizes, small = set(), set(), set()
for line in open(sys.argv[1]):
    if ' hard ' not in line or not line.rstrip().endswith(' inconclusive'): continue
    gate, scene, n = re.match(r'(G\d+)\s+hard snapshot \S+ (\S+) n=(\d+)', line).groups()
    scenes.add(scene); (small if gate == 'G1' else sizes).add(int(n))
    if gate == 'G4': scenes.add('split-line')
if scenes:
    print('--scenes='+','.join(sorted(scenes)), '--sizes='+','.join(map(str, sorted(sizes))),
          '--small-sizes='+','.join(map(str, sorted(small))))
PY
)
export QT_RESOLVE_ARGS
if [[ $QT_RESOLVE_ARGS ]]; then run int32-resolve int32-resolve-2; fi
python3 "$here/resolve.py" "$out" snapshot int32 > "$out/final-snapshot-status.txt"
