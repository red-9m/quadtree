#!/usr/bin/env python3
import gzip
import math
import runpy
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
base = runpy.run_path(str(HERE.parent / 'g2-integer-key-2026-11-19/summarize.py'))
out = Path(sys.argv[1])
read, key, save = (base[k] for k in ('read_rows', 'key', 'save'))
ref_path = HERE.parent / 'g2-compact-classifier-2026-11-20/15-control/snapshot.csv'
ref = {key(r): r for r in read(ref_path)}
runs = {}
for name in ('radix', 'rank'):
    rows = read(out / f'{name}.csv')
    data = runs[name] = {key(r): r for r in rows}
    counts = Counter(map(key, read(out / f'{name}-raw.csv.gz', gzip.open)))
    assert len(rows) == len(data) == 8 and counts.keys() == data.keys(), name
    for k, r in data.items():
        assert r['n'] == '100000' and r['rounds'] == '9' and r['type'] == 'int32' and r['status'] == 'ok', (name, k)
        assert r['engine'] in ('index', 'index-ordered') and float(r['allocs_per_step']) == 0, (name, k)
        assert counts[k] == int(r['reps']), (name, k)
        assert all(r[f] == ref[k][f] for f in base['COUNTS']), (name, k)
assert runs['radix'].keys() == runs['rank'].keys()
changes = []
for k in sorted(runs['radix']):
    a, b = (runs[name][k] for name in ('radix', 'rank'))
    x, y = (float(r['estimate_ns']) for r in (a, b))
    half = math.hypot(float(a['err_ns']) / x, float(b['err_ns']) / y)
    ratio = y / x
    changes.append((*k, a['estimate_ns'], b['estimate_ns'], f'{ratio:.4f}', f'{ratio*math.exp(-half):.4f}',
                    f'{ratio*math.exp(half):.4f}', a['prepare_median_ns'], b['prepare_median_ns'],
                    a['pairs_median_ns'], b['pairs_median_ns']))
save(out / 'step-change.csv', 'scene n engine control_ns candidate_ns candidate_over_control ci_low ci_high '
     'control_build_ns candidate_build_ns control_pairs_ns candidate_pairs_ns', changes)
print('All 16 isolated pilot rows match the radix gate fixtures, results, work, allocations and footprint.')
