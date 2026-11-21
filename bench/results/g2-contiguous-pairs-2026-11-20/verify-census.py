#!/usr/bin/env python3
import csv
from pathlib import Path

here = Path(__file__).resolve().parent
reference = here.parent / 'g2-inline-count-2026-11-20/15-candidate/snapshot.csv'
with reference.open() as f:
    ref = {(r['scene'], r['n']): r for r in csv.DictReader(l for l in f if not l.startswith('#')) if r['engine'] == 'index'}
with (here / 'census.csv').open() as f:
    rows = list(csv.DictReader(f))
assert len(rows) == 8
for r in rows:
    key = r['scene'], r['n']
    assert all(r[f] == ref[key][f] for f in ('pairs', 'checksum', 'candidates', 'node_visits')), key
    n = int(r['candidates'])
    assert sum(int(v) for k, v in r.items() if k.startswith(('self_', 'cross_', 'walk_')) and not k.endswith('_grouped')) == n
    eligible = sum(int(r[f'{scope}_{length}']) for scope in ('self', 'cross') for length in ('4_7', '8_plus'))
    grouped = int(r['self_grouped']) + int(r['cross_grouped'])
    assert grouped <= eligible
    print(*key, f'self/cross length 4+: {100 * eligible / n:.2f}%; complete groups: {100 * grouped / n:.2f}%')
print('All eight count-only rows reconcile with the staged gate work and results.')
