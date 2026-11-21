#!/usr/bin/env python3
import gzip
import math
import runpy
import sys
from collections import Counter
from pathlib import Path

here = Path(__file__).resolve().parent
base = runpy.run_path(str(here.parent / 'g2-integer-key-2026-11-19/summarize.py'))
out = Path(sys.argv[1])
read, key, save = (base[k] for k in ('read_rows', 'key', 'save'))
ref = {key(r): r for r in read(here.parent / 'g2-radix-outline-2026-11-21/frozen/9-control/snapshot.csv')}
keys = {(s, n, e) for s in ('uniform', 'split-line', 'split-line-translated', 'mixed-size')
        for n in (10000, 100000) for e in ('index', 'index-ordered')}
changes = []
for block in (1, 2):
    both = []
    for kind in ('control', 'candidate'):
        path = out / f'{block}-{kind}'
        rows = read(path / 'snapshot.csv')
        counts = Counter(map(key, read(path / 'snapshot-raw.csv.gz', gzip.open)))
        assert len(rows) == len(keys) and counts.keys() == keys
        data = {key(r): r for r in rows}
        assert data.keys() == keys
        for k, row in data.items():
            assert (row['type'], row['rounds'], row['status'], row['batch']) == ('int32', '9', 'ok', '1'), (path, k)
            assert counts[k] == int(row['reps']) and float(row['allocs_per_step']) == 0, (path, k)
            assert all(row[f] == ref[k][f] for f in base['COUNTS']), (path, k)
        both.append(data)
    for k in sorted(keys):
        a, b = (r[k] for r in both)
        x, y = (float(r['estimate_ns']) for r in (a, b))
        ratio = y/x
        half = math.hypot(float(a['err_ns'])/x, float(b['err_ns'])/y)
        changes.append((block, *k, a['estimate_ns'], b['estimate_ns'], f'{ratio:.5f}',
                        f'{ratio*math.exp(-half):.5f}', f'{ratio*math.exp(half):.5f}',
                        a['prepare_median_ns'], b['prepare_median_ns'], a['pairs_median_ns'], b['pairs_median_ns']))
save(out / 'step-change.csv', 'block scene n engine control_ns candidate_ns ratio ci_low ci_high '
     'control_build_ns candidate_build_ns control_pairs_ns candidate_pairs_ns', changes)
print('64 pilot rows match frozen fixtures, pair checksums, work, allocations and footprint. Zero warm allocations.')
