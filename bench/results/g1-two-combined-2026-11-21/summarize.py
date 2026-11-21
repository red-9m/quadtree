#!/usr/bin/env python3
"""Verify the matched G1 runs and summarize the direct two-entry result candidate.

Writes g1-comparison.csv (all 16 G1 rows per run, both binaries) and
library-change.csv (index and index-ordered A/B at n=0..8 with log-ratio intervals).
"""
import csv
import gzip
import math
import re
import runpy
import sys
from collections import Counter
from pathlib import Path

here = Path(__file__).resolve().parent
out = Path(sys.argv[1])
base = runpy.run_path(str(here.parent/'g2-integer-key-2026-11-19/summarize.py'))
read, key, save, COUNTS = (base[k] for k in ('read_rows', 'key', 'save', 'COUNTS'))
regex = re.compile(r'G1\s+hard snapshot int32 (\S+) n=(\d+) vs brute force \(brute\)\s+(\S+) \[(\S+)-(\S+)\]\s+<= 2 '
                   r'(pass|FAIL|inconclusive)')
assert (out/'power-before.txt').read_bytes() == (out/'power-after.txt').read_bytes()
with (out/'run-order.csv').open() as f:
    temps = {r['run']: int(r['start_tctl_millidegrees']) for r in csv.DictReader(f)}
assert len(temps) == 4 and max(temps.values()) <= 40000
g1, change = [], []
for rounds in (81, 117):
    assert abs(temps[f'{rounds}-control']-temps[f'{rounds}-candidate']) <= 1000
    runs = {}
    for kind in ('control', 'candidate'):
        d = out/f'{rounds}-{kind}'
        assert (d/'run.exit').read_text().strip() == '0'
        rows = read(d/'snapshot.csv')
        raw = read(d/'snapshot-raw.csv.gz', gzip.open)
        data = {key(r): r for r in rows}
        reps = Counter((key(r), int(r['round'])) for r in raw)
        assert len(rows) == len(data) == 96 and len(reps) == len(raw) == 96*rounds
        for k, r in data.items():
            assert r['status'] == 'ok' and r['type'] == 'int32' and r['batch'] == '256'
            assert r['rounds'] == r['reps'] == str(rounds) and all(reps[k, i] == 1 for i in range(rounds))
            if k[-1] in ('index', 'index-ordered'):
                assert float(r['allocs_per_step']) == 0
        gates = {}
        for line in (d/'gates.txt').read_text().splitlines():
            if m := regex.fullmatch(line):
                scene, n, *g = m.groups()
                gates[scene, int(n)] = g
        assert len(gates) == 16
        runs[kind] = data, gates
    (a, ag), (b, bg) = runs['control'], runs['candidate']
    assert a.keys() == b.keys() and all(a[k][f] == b[k][f] for k in a for f in COUNTS)
    print(f'{rounds}: {rounds*96} raw samples per process; 96 matched rows agree on '
          f'{", ".join(COUNTS)}; 32 library rows per process have zero warm allocations.')
    for kind, (data, gates) in runs.items():
        verdicts = Counter(g[-1] for g in gates.values())
        print(f'  {kind}: G1 pass/FAIL/inconclusive = {verdicts["pass"]}/{verdicts["FAIL"]}/{verdicts["inconclusive"]}')
        for (scene, n), g in sorted(gates.items()):
            r = data[scene, n, 'index']
            g1.append((rounds, kind, scene, n, r['estimate_ns'], r['err_ns'], data[scene, n, 'brute']['estimate_ns'], *g))
    for scene in ('clustered', 'uniform'):
        for n in (0, 1, 2, 4, 8):
            for engine in ('index', 'index-ordered'):
                t, u = (float(x[scene, n, engine]['estimate_ns']) for x in (a, b))
                e0, e1 = (float(x[scene, n, engine]['err_ns']) for x in (a, b))
                h = math.hypot(e0/t, e1/u)
                lo, hi = (u/t*math.exp(s*h) for s in (-1, 1))
                change.append((rounds, scene, n, engine, f'{t:.2f}', f'{u:.2f}', f'{t-u:.2f}', f'{u/t:.4f}',
                               f'{lo:.4f}', f'{hi:.4f}', 'faster' if hi < 1 else 'slower' if lo > 1 else 'unresolved'))
save(out/'g1-comparison.csv', 'rounds binary scene n index_ns index_err_ns brute_ns ratio lower upper verdict', g1)
save(out/'library-change.csv', 'rounds scene n engine control_ns candidate_ns saved_ns ratio lower upper verdict', change)
for row in change:
    if row[2] in (2, 4, 8):
        print(' ', *row)
