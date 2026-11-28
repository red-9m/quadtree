#!/usr/bin/env python3
"""python3 summarize.py DATA: gate summaries, hard rows by binary, work equality and phase medians."""
import csv
import re
import sys
from collections import defaultdict
from pathlib import Path

data = Path(sys.argv[1])
ROW = re.compile(r'^(G\d+[a-c]?)\s+(hard|soft)\s+(.*?)\s{2,}(\d+\.\d+) \[(\d+\.\d+)-(\d+\.\d+)\]\s+(<=|>=) (\S+) (\S+)$')


def gates(run):
    rows, summary = {}, []
    for line in (data / run / 'gates.txt').read_text().splitlines():
        m = ROW.match(line)
        if m:
            rows[(m[1], m[3])] = (float(m[4]), float(m[5]), float(m[6]), m[9])
        elif re.match(r'\s+G\S+ (hard|soft)\s+\d', line):
            summary.append(line.strip())
    return rows, summary


def results(run):
    with open(data / run / 'results.csv') as f:
        return list(csv.DictReader(line for line in f if not line.startswith('#')))


def key(r):
    return tuple(r[k] for k in ('model', 'scene', 'type', 'n', 'workload', 'mode', 'engine'))


runs = sorted(p.name for p in data.iterdir() if (p / 'gates.txt').exists())
groups = defaultdict(dict)
for run in runs:
    name, binary = run.rsplit('-', 1)
    binary = {'control': 'base', 'candidate': 'cand'}.get(binary, binary)  # Pilot naming.
    groups[name][binary] = run

WORK = ('candidates', 'node_visits', 'pairs', 'checksum', 'scene_hash', 'allocs_per_step')
for name, pair in groups.items():
    print(f'== {name}')
    parsed = {b: gates(r) for b, r in pair.items()}
    for b in ('base', 'cand'):
        if b in parsed:
            print(f'  {b:9}: ' + '; '.join(parsed[b][1]))
    if len(pair) != 2:
        continue
    (cr, _), (xr, _) = parsed['base'], parsed['cand']
    changed = []
    for k in sorted(set(cr) | set(xr)):
        if k[0] in ('G1', 'G2', 'G9', 'G7', 'G8a', 'G8b', 'G8c') and (k in cr) and (k in xr):
            c, x = cr[k], xr[k]
            if c[3] != x[3] or x[3] != 'pass' or c[3] != 'pass':
                changed.append(f'    {k[0]:4} {k[1][:70]:70} {c[0]:.3f} [{c[1]:.3f}-{c[2]:.3f}] {c[3]:12} -> '
                               f'{x[0]:.3f} [{x[1]:.3f}-{x[2]:.3f}] {x[3]}')
    if changed:
        print('  hard/soft rows not passing on both:')
        print('\n'.join(changed))
    a = {key(r): r for r in results(pair['base'])}
    b = {key(r): r for r in results(pair['cand'])}
    diff = [k for k in a if k in b and a[k]['status'] == b[k]['status'] == 'ok' and
            k[6] in ('index', 'index-ordered') and any(a[k][w] != b[k][w] for w in WORK)]
    capped = sum(1 for k in a if a[k]['status'] != 'ok')
    print(f'  library rows: {sum(1 for k in a if k[6].startswith("index"))}, work/checksum differences: {len(diff)}, '
          f'non-ok rows (base): {capped}')
    for k in diff[:5]:
        print('    DIFF', k, {w: (a[k][w], b[k][w]) for w in WORK if a[k][w] != b[k][w]})
    if 'matrix' in name or 'targets' in name:
        print('  index / index-ordered estimate (us) and phase medians, base -> cand:')
        for k in sorted(a, key=lambda k: (k[1], int(k[3]))):
            if k[6] not in ('index', 'index-ordered') or k not in b or a[k]['status'] != 'ok' or int(k[3]) < 1000:
                continue
            ca, cb = a[k], b[k]
            est = lambda r, c: float(r[c]) / 1e3
            print(f'    {k[1]:22} {k[3]:>6} {k[6]:13} {est(ca, "estimate_ns"):10.1f} -> {est(cb, "estimate_ns"):10.1f} '
                  f'({100 * (est(cb, "estimate_ns") / est(ca, "estimate_ns") - 1):+5.1f}%)  prepare '
                  f'{est(ca, "prepare_median_ns"):9.1f} -> {est(cb, "prepare_median_ns"):9.1f}  pairs '
                  f'{est(ca, "pairs_median_ns"):9.1f} -> {est(cb, "pairs_median_ns"):9.1f}')
