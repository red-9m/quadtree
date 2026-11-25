#!/usr/bin/env bash
# Run once, from a clean archive build. Each run.sh invocation restores power settings.
set -euo pipefail
here=$(dirname "$(realpath "$0")"); w=$(realpath "$1"); out="$here/data"
run() { bash "$here/run.sh" "$w" "$out" "$@"; }
run int64-snapshot int32-snapshot
for t in int64 int32; do
    QT_RESOLVE_ARGS=$(python3 - "$out/$t-snapshot/gates.txt" <<'PY'
import re, sys
scenes, sizes, small = set(), set(), set()
for line in open(sys.argv[1]):
    if ' hard ' not in line or not line.rstrip().endswith(' inconclusive'): continue
    gate, scene, n = re.match(r'(G\d+)\s+hard snapshot \S+ (\S+) n=(\d+)', line).groups()
    scenes.add(scene); (small if gate == 'G1' else sizes).add(int(n))
    if gate == 'G4': scenes.add('split-line')
if scenes:
    print('--scenes='+','.join(sorted(scenes)), '--sizes='+','.join(map(str,sorted(sizes))), '--small-sizes='+','.join(map(str,sorted(small))))
PY
)
    export QT_RESOLVE_ARGS
    if [[ $QT_RESOLVE_ARGS ]]; then run "$t-resolve" "$t-resolve-2"; fi
done
python3 "$here/resolve.py" "$out" > "$out/final-snapshot-status.txt"
run int64-persistent int32-persistent
