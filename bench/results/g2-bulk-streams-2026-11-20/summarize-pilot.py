#!/usr/bin/env python3
import csv
import gzip
import math
import runpy
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
BASE = runpy.run_path(str(HERE.parent / 'g2-integer-key-2026-11-19/summarize.py'))
out = Path(sys.argv[1])
read, key, save = (BASE[name] for name in ('read_rows', 'key', 'save'))
scenes = ('uniform', 'split-line', 'split-line-translated', 'mixed-size')
keys = {(scene, 100000, engine) for scene in scenes for engine in ('index', 'index-ordered')}
ref = {key(row): row for row in read(HERE.parent / 'g9-build-rank-2026-11-20/15-candidate/snapshot.csv')}
steps = []
for block in (1, 2):
    runs = {}
    for kind in ('control', 'centers', 'narrow'):
        path = out / f'{block}-{kind}'
        rows = read(path.with_suffix('.csv'))
        counts = Counter(map(key, read(Path(f'{path}-raw.csv.gz'), gzip.open)))
        assert len(rows) == 8 and counts.keys() == keys
        runs[kind] = {key(row): row for row in rows}
        assert runs[kind].keys() == keys
        for k, row in runs[kind].items():
            assert (row['type'], row['rounds'], row['status'], row['batch']) == ('int32', '9', 'ok', '1'), (path, k)
            assert counts[k] == int(row['reps']) and float(row['allocs_per_step']) == 0, (path, k)
            assert all(row[f] == ref[k][f] for f in BASE['COUNTS']), (path, k)
    for kind in ('centers', 'narrow'):
        for k in sorted(keys):
            a, b = (runs[name][k] for name in ('control', kind))
            x, y = (float(row['estimate_ns']) for row in (a, b))
            ratio = y / x
            half = math.hypot(float(a['err_ns']) / x, float(b['err_ns']) / y)
            steps.append((block, kind, *k, a['estimate_ns'], b['estimate_ns'], f'{ratio:.4f}',
                          f'{ratio * math.exp(-half):.4f}', f'{ratio * math.exp(half):.4f}',
                          a['prepare_median_ns'], b['prepare_median_ns'], a['pairs_median_ns'], b['pairs_median_ns']))
save(out / 'step-change.csv', 'block kind scene n engine control_ns candidate_ns candidate_over_control ci_low ci_high '
     'control_build_ns candidate_build_ns control_pairs_ns candidate_pairs_ns', steps)
print('48 pilot rows agree with the staged gate reference: fixtures, pairs, checksums, work, allocations and footprint.')
