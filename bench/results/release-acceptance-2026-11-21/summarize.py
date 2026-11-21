#!/usr/bin/env python3
"""Summarize the Task 08 release matrix.

usage: summarize.py DATA_DIR
Writes into DATA_DIR (resolve-int32/resolve-int64 gate verdicts replace matrix inconclusives):
  gate-rows.csv      every gate line of the snapshot and persistent stages, with release status
  gate-summary.csv   counts per stage/gate/kind/status
  reconciliation.csv expected vs observed (type, scene, n[, workload, mode]) rows per stage and engine
  g5-pathology.csv   index / index-ordered vs best baseline on the G5 scenes (published, no gate)
  library-rows.csv   index / index-ordered timing, work, allocations and memory for every row
Prints a short report. Release status per hard row: pass, accepted-exception (G1, 0-4 entries,
upper bound <= 3.5), blocking (FAIL or above the exception cap), inconclusive.
"""
import csv
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

data = Path(sys.argv[1])
TYPES = ('int32', 'int64', 'float', 'double')
SNAP_SCENES = ('uniform', 'clustered', 'split-line', 'split-line-translated', 'mixed-size', 'long-thin', 'tiles',
               'tiles-touch', 'points', 'pile', 'disjoint', 'outliers')
SMALL = (0, 1, 2, 4, 8, 16, 32, 64)
LARGE = (100, 1000, 10000, 100000)
PERS_SCENES = ('uniform', 'clustered', 'split-line', 'mixed-size')
PERS_SIZES = (1000, 10000, 100000)
WORKLOADS = ('static', 'small-single', 'small-1%', 'small-10%', 'small-100%', 'teleport-1%', 'teleport-10%',
             'noop-100%', 'churn-1%', 'churn-10%')
G5 = ('long-thin', 'tiles', 'tiles-touch', 'points', 'pile', 'outliers', 'disjoint')
LINE = re.compile(r'^(G\d+[a-z]?)\s+(hard|soft)\s+(.*?)\s{2,}(\S+)(?: \[(\S+)-(\S+)\])?\s+(<=|x/)\s*(\S+) '
                  r'(pass|FAIL|inconclusive)$')
SMALL_ROW = re.compile(r'^snapshot \S+ (uniform|clustered) n=(\d+) vs brute force')


def read(path):
    with open(path) as f:
        return list(csv.DictReader(line for line in f if not line.startswith('#')))


def save(name, header, rows):
    with (data/name).open('w') as f:
        w = csv.writer(f, lineterminator='\n')
        w.writerow(header)
        w.writerows(rows)


gate_rows, summary = [], Counter()
for stage in ('snapshot', 'persistent'):
    path = data/stage/'gates.txt'
    if not path.exists():
        print(f'{stage}: missing gates.txt')
        continue
    for line in path.read_text().splitlines():
        if not line.startswith('G'):
            continue
        m = LINE.match(line)
        assert m, line
        gate, kind, subject, value, lo, hi, _, limit, verdict = m.groups()
        status = {'pass': 'pass', 'FAIL': 'blocking', 'inconclusive': 'inconclusive'}[verdict]
        if kind == 'soft':
            status = 'soft-' + verdict.lower()
        elif gate == 'G1' and verdict != 'pass' and (s := SMALL_ROW.match(subject)) and int(s[2]) <= 4:
            upper = float(hi if hi else value)
            status = 'accepted-exception' if upper <= 3.5 else 'blocking'
        gate_rows.append((stage, gate, kind, subject, value, lo or '', hi or '', limit, verdict, status))
        summary[stage, gate, kind, status] += 1
# Rows inconclusive in the matrix were rerun in separate 45-round processes; their verdict is final.
rerun = {}
for stage in ('resolve-int32', 'resolve-int64'):
    path = data/stage/'gates.txt'
    if path.exists():
        for line in path.read_text().splitlines():
            if (m := LINE.match(line)) and m[2] == 'hard':
                rerun[m[1], m[3]] = (stage, m[4], m[5] or '', m[6] or '', m[9])
final = []
for row in gate_rows:
    stage, gate, kind, subject, value, lo, hi, limit, verdict, status = row
    if status == 'inconclusive' and (gate, subject) in rerun:
        rs, rv, rlo, rhi, rverdict = rerun[gate, subject]
        status = {'pass': 'pass', 'FAIL': 'blocking', 'inconclusive': 'inconclusive'}[rverdict]
        summary[stage, gate, kind, 'inconclusive'] -= 1
        summary[stage, gate, kind, status] += 1
        final.append((*row[:-1], status, rs, rv, rlo, rhi, rverdict))
    else:
        final.append((*row, '', '', '', '', ''))
gate_rows = final
save('gate-rows.csv', ('stage', 'gate', 'kind', 'subject', 'value', 'lower', 'upper', 'limit', 'verdict', 'status',
                       'rerun_stage', 'rerun_value', 'rerun_lower', 'rerun_upper', 'rerun_verdict'), gate_rows)
save('gate-summary.csv', ('stage', 'gate', 'kind', 'status', 'rows'),
     sorted((*k, v) for k, v in summary.items() if v))

# Reconciliation: every expected row must appear for the library engines; baselines are listed as observed.
recon = []
snap = read(data/'snapshot'/'results.csv') if (data/'snapshot'/'results.csv').exists() else []
seen = defaultdict(dict)
for r in snap:
    seen[r['type'], r['scene'], int(r['n'])][r['engine']] = r['status']
for t in TYPES:
    for scene in SNAP_SCENES:
        sizes = (SMALL if scene in ('uniform', 'clustered') else ()) + LARGE
        for n in sizes:
            got = seen.get((t, scene, n), {})
            for engine in ('index', 'index-ordered', 'brute', 'sweep', 'grid'):
                status = got.get(engine) or ('row-capped' if '*' in got else 'MISSING')
                recon.append(('snapshot', t, scene, n, '', '', engine, status))
pers = read(data/'persistent'/'results.csv') if (data/'persistent'/'results.csv').exists() else []
pseen = defaultdict(dict)
for r in pers:
    pseen[r['type'], r['scene'], int(r['n']), r['workload'], r['mode']][r['engine']] = r['status']
if pers:
    for t in TYPES:
        engines = ('index', 'index-ordered', 'brute', 'sweep', 'grid', 'grid-rebuild') + \
            (('b2tree', 'b2tree-fat') if t in ('int32', 'float') else ())
        for scene in PERS_SCENES:
            for n in PERS_SIZES:
                for wl in WORKLOADS:
                    for mode in ('full', 'moved') if wl != 'small-100%' and wl != 'noop-100%' else ('full',):
                        got = pseen.get((t, scene, n, wl, mode), {})
                        for engine in engines:
                            if mode == 'moved' and engine == 'grid-rebuild':
                                continue
                            recon.append(('persistent', t, scene, n, wl, mode, engine, got.get(engine, 'MISSING')))
save('reconciliation.csv', ('stage', 'type', 'scene', 'n', 'workload', 'mode', 'engine', 'status'), recon)

# G5 pathology: published comparison of the library to the best timed baseline.
g5 = []
by = defaultdict(dict)
for r in snap:
    by[r['type'], r['scene'], int(r['n'])][r['engine']] = r
for (t, scene, n), engines in sorted(by.items()):
    if scene not in G5:
        continue
    base = [e for k, e in engines.items() if k in ('brute', 'sweep', 'grid') and e['status'] == 'ok']
    best = min(base, key=lambda e: float(e['estimate_ns'])) if base else None
    for lib in ('index', 'index-ordered'):
        e = engines.get(lib)
        if not e or e['status'] != 'ok':
            g5.append((t, scene, n, lib, e['status'] if e else 'row-capped', '', '', '', '', ''))
            continue
        ratio = f"{float(e['estimate_ns'])/float(best['estimate_ns']):.3f}" if best else ''
        g5.append((t, scene, n, lib, 'ok', e['estimate_ns'], best['engine'] if best else '',
                   best['estimate_ns'] if best else '', ratio, e['candidates']))
save('g5-pathology.csv', ('type', 'scene', 'n', 'engine', 'status', 'estimate_ns', 'best_baseline',
                          'baseline_ns', 'ratio', 'candidates'), g5)

# Library rows: timing distribution, work, allocations and memory (cold build peak and retained bytes).
lib = []
for stage, rows in (('snapshot', snap), ('persistent', pers)):
    for r in rows:
        if r['engine'] in ('index', 'index-ordered'):
            lib.append((stage, r['type'], r['scene'], r['n'], r['workload'], r['mode'], r['engine'], r['status'],
                        r['estimate_ns'], r['err_ns'], r['median_ns'], r['p95_ns'], r['prepare_median_ns'],
                        r['pairs_median_ns'], r['candidates'], r['node_visits'], r['pairs'], r['allocs_per_step'],
                        r['cold_allocs'], r['peak_bytes'], r['retained_bytes']))
save('library-rows.csv', ('stage', 'type', 'scene', 'n', 'workload', 'mode', 'engine', 'status', 'estimate_ns',
                          'err_ns', 'median_ns', 'p95_ns', 'prepare_median_ns', 'pairs_median_ns', 'candidates',
                          'node_visits', 'pairs', 'allocs_per_step', 'cold_allocs', 'peak_bytes', 'retained_bytes'), lib)

print('Hard gate rows by release status:')
for stage in ('snapshot', 'persistent'):
    counts = Counter()
    for (s, g, k, st), v in summary.items():
        if s == stage and k == 'hard':
            counts[g, st] += v
    for g in sorted({g for g, _ in counts}):
        print(f'  {stage:10} {g:4}', ', '.join(f'{st} {counts[g, st]}' for st in
              ('pass', 'accepted-exception', 'blocking', 'inconclusive') if counts[g, st]))
missing = [r for r in recon if r[-1] == 'MISSING']
print(f'Reconciliation: {len(recon)} expected engine rows, {len(missing)} missing')
for r in missing[:20]:
    print('  MISSING', *r[:-1])
soft = Counter()
for (s_, g, k, st), v in summary.items():
    if k == 'soft':
        soft[s_, g, st] += v
print('Soft gates:', ', '.join(f'{s_} {g} {st} {v}' for (s_, g, st), v in sorted(soft.items())))
