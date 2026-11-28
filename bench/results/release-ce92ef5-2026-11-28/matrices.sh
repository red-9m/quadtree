#!/usr/bin/env bash
# Controller, run once from a clean archive build: bash matrices.sh WORK [OUT]   (OUT default: ./data)
# Follows resolution-rule.md. Each run.sh invocation sets and restores power settings.
# QT_FROM=persistent resumes after a completed snapshot half (aborted stages are renamed *-aborted first).
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")"); w=$(realpath "$1"); out=$(realpath -m "${2:-$here/data}")
exe="$w/build/bench/quadtree-bench"
run() { bash "$here/run.sh" "$w" "$out" "$@"; }
types=(int32 float double int64)

if [[ ${QT_FROM:-} != persistent ]]; then
run calib-a calib-b
rc=0; "$exe" --diff="$out/calib-a/results.csv,$out/calib-b/results.csv" > "$out/calib-diff.txt" || rc=$?
echo "$rc" > "$out/calib-diff.exit"

run "${types[@]/%/-snapshot}"
for t in "${types[@]}"; do
    QT_RESOLVE_ARGS=$(python3 - "$out/$t-snapshot/gates.txt" <<'PY'
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
    if [[ $QT_RESOLVE_ARGS ]]; then run "$t-resolve" "$t-resolve-2"; fi
done
python3 "$here/resolve.py" "$out" snapshot > "$out/final-snapshot-status.txt"
fi

run "${types[@]/%/-persistent}"
for t in "${types[@]}"; do
    QT_PRESOLVE_ARGS=$(python3 - "$out/$t-persistent/gates.txt" <<'PY'
import re, sys
scenes, sizes, workloads, modes = set(), set(), set(), set()
for line in open(sys.argv[1]):
    if ' hard ' not in line or not line.rstrip().endswith(' inconclusive'): continue
    gate, scene, n, wl, mode = re.match(r'(G\d+[a-z]?)\s+hard persistent \S+ (\S+) n=(\d+) (\S+)/(\S+) ', line).groups()
    scenes.add(scene); sizes.add(int(n)); workloads.add(wl); modes.add(mode)
    if gate == 'G8b': sizes.add(1000)
if scenes:
    print('--scenes='+','.join(sorted(scenes)), '--persistent-sizes='+','.join(map(str, sorted(sizes))),
          '--workloads='+','.join(sorted(workloads)), '--modes='+','.join(sorted(modes)))
PY
)
    export QT_PRESOLVE_ARGS
    if [[ $QT_PRESOLVE_ARGS ]]; then run "$t-presolve" "$t-presolve-2"; fi
done
python3 "$here/resolve.py" "$out" persistent > "$out/final-persistent-status.txt"
