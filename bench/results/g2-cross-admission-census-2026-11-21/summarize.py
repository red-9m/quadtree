#!/usr/bin/env python3
import csv
import sys
from pathlib import Path

here = Path(__file__).resolve().parent
root = Path(sys.argv[1])

def rows(p):
    with p.open() as f:
        return list(csv.DictReader(line for line in f if not line.startswith('#')))

ref = {(r['scene'], r['n']): r for r in rows(here.parent / 'g2-radix-outline-2026-11-21/frozen/9-control/snapshot.csv')
       if r['engine'] == 'index'}
work = rows(root / 'work.csv')
shapes = rows(root / 'cross-shapes.csv')
assert len(work) == 4 and len(shapes) == 24
with (root / 'summary.csv').open('w') as f:
    w = csv.writer(f, lineterminator='\n')
    w.writerow(('scene','n','calls','recursive_pct','charged','disjoint_pct_of_charged','empty_pct_of_calls',
                'x_only_pct','y_only_pct','both_axes_pct','leaf_leaf_pct_of_disjoint','cross_exact_pct'))
    for row in work:
        k = row['scene'], row['n']
        for col in ('scene_hash', 'checksum', 'node_visits', 'candidates', 'pairs'):
            assert row[col] == ref[k][col], (k, col)
        s = [r for r in shapes if (r['scene'], r['n']) == k]
        assert {(r['origin'], r['shape']) for r in s} == {(str(x), str(y)) for x in (0, 1) for y in (0, 1, 2)}
        def count(col, candidates=s):
            return sum(int(r[col]) for r in candidates)
        for r in s:
            assert int(r['calls']) == sum(int(r[x]) for x in ('empty','x_only','y_only','both','filtered','admitted'))
        calls, empty, x, y, both = (count(c) for c in ('calls','empty','x_only','y_only','both'))
        charged, disjoint = calls-empty, x+y+both
        leaf = sum(count(c, [r for r in s if r['shape'] == '0']) for c in ('x_only','y_only','both'))
        assert charged + int(row['self_visits']) + int(row['holder_visits']) == int(row['node_visits'])
        for measure, suffix in (('candidates', 'tests'), ('pairs', 'reports')):
            assert sum(int(row[f'{owner}_{suffix}']) for owner in ('self','cross','holder')) == int(row[measure])
        assert count('candidates') == int(row['cross_tests']) and count('reports') == int(row['cross_reports'])
        def pct(a, b): return f'{a*100/b:.2f}'
        w.writerow((*k,calls,pct(count('calls',[r for r in s if r['origin']=='1']),calls),charged,pct(disjoint,charged),pct(empty,calls),
                    pct(x,disjoint),pct(y,disjoint),pct(both,disjoint),pct(leaf,disjoint),pct(int(row['cross_tests']),int(row['candidates']))))
print('Four shadow traversals match the actual pair stream and counters exactly; frozen fixture hashes, checksums and work reconcile.')
