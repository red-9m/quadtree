#!/usr/bin/env python3
"""Summarize the c13e498 release validation (resolution-rule.md).

usage: summarize.py DATA_DIR
Writes into DATA_DIR:
  gate-rows.csv      every gate line of the 5-round/64-step matrices: matrix verdict, pooled repeat (if any),
                     final verdict and release status
  gate-summary.csv   counts per type/model/gate/kind/final verdict and release status
  reconciliation.csv expected vs observed (type, scene, n[, workload, mode]) engine rows per matrix stage
  g5-pathology.csv   index / index-ordered vs best baseline on the G5 scenes (published, no gate)
  library-rows.csv   index / index-ordered timing, work, allocations and memory for every matrix row
Release status: int32 hard rows are pass / accepted-exception / blocking / open. For int64/float/double,
G6 (allocations) blocks; other hard gates are published (`published-pass`, `published-fail`, ...).
"""
import csv
import math
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

data = Path(sys.argv[1])
TYPES = ('int32', 'float', 'double', 'int64')
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
G2_EXC = re.compile(r'^snapshot int32 (split-line-translated n=100|uniform n=1000) vs best baseline')
G1_SMALL = re.compile(r'^snapshot \S+ (uniform|clustered) n=([0-4]) vs brute force')
REPEATS = {'snapshot': ('resolve', 'resolve-2'), 'persistent': ('presolve', 'presolve-2')}


def read(path):
    if not path.exists():
        return []
    with open(path) as f:
        return list(csv.DictReader(line for line in f if not line.startswith('#')))


def save(name, header, rows):
    with (data/name).open('w') as f:
        w = csv.writer(f, lineterminator='\n')
        w.writerow(header)
        w.writerows(rows)


def key_of(subject):
    return re.sub(r'(vs best baseline|vs dynamic tree) \([^)]+\)$', r'\1', subject)


def gate_lines(path):
    rows = []
    if path.exists():
        for line in path.read_text().splitlines():
            if line.startswith('G'):
                m = LINE.match(line)
                assert m, line
                rows.append(m.groups())
    return rows


gate_rows, summary, exits = [], Counter(), {}
for stage_dir in sorted(p for p in data.iterdir() if (p/'exit.txt').exists()):
    exits[stage_dir.name] = stage_dir.joinpath('exit.txt').read_text().split()
for t in TYPES:
    for model in ('snapshot', 'persistent'):
        reruns = []
        for s in REPEATS[model]:
            reruns.append({(g[0], key_of(g[2])): g for g in gate_lines(data/f'{t}-{s}'/'gates.txt') if g[1] == 'hard'})
        for gate, kind, subject, value, lo, hi, op, limit, verdict in gate_lines(data/f'{t}-{model}'/'gates.txt'):
            final = {'pass': 'pass', 'FAIL': 'fail', 'inconclusive': 'inconclusive'}[verdict]
            upper, pooled = (float(hi) if hi else float(value)), ('', '', '', '')
            if kind == 'hard' and verdict == 'inconclusive':
                got = [r[gate, key_of(subject)] for r in reruns if (gate, key_of(subject)) in r]
                if len(got) == 2:
                    logs = [math.log(float(g[3])) for g in got]
                    hs = [(math.log(float(g[5])) - math.log(float(g[4]))) / 2 for g in got]
                    m, h = sum(logs) / 2, math.sqrt(sum(x * x for x in hs)) / 2
                    plo, phi, lim = math.exp(m - h), math.exp(m + h), float(limit)
                    low = 1 / lim if op == 'x/' else 0
                    final = 'pass' if low <= plo and phi <= lim else 'fail' if plo > lim or phi < low else 'inconclusive'
                    upper = phi
                    pooled = (f'{math.exp(m):.3f}', f'{plo:.3f}', f'{phi:.3f}',
                              '; '.join(f'{g[3]} [{g[4]}-{g[5]}]' for g in got))
                else:
                    pooled = ('', '', '', 'repeat missing')
            if kind == 'soft':
                status = 'soft-' + final
            elif t == 'int32':
                status = {'pass': 'pass', 'fail': 'blocking', 'inconclusive': 'open'}[final]
                if final != 'pass' and ((gate == 'G1' and G1_SMALL.match(subject) and upper <= 3.5) or
                                        (gate == 'G2' and G2_EXC.match(subject) and upper <= 4.5)):
                    status = 'accepted-exception'
            elif gate == 'G6':
                status = 'pass' if final == 'pass' else 'blocking'
            else:
                status = 'published-' + final
            gate_rows.append((t, model, gate, kind, subject, value, lo or '', hi or '', op, limit, verdict, *pooled,
                              final, status))
            summary[t, model, gate, kind, final, status] += 1
save('gate-rows.csv', ('type', 'model', 'gate', 'kind', 'subject', 'value', 'lower', 'upper', 'op', 'limit',
                       'matrix_verdict', 'pooled_value', 'pooled_lower', 'pooled_upper', 'repeats', 'final_verdict',
                       'release_status'), gate_rows)
save('gate-summary.csv', ('type', 'model', 'gate', 'kind', 'final_verdict', 'release_status', 'rows'),
     sorted((*k, v) for k, v in summary.items()))

# Reconciliation: every expected engine row must appear in the type's matrix.
recon, snap, pers = [], {}, {}
for t in TYPES:
    snap[t] = read(data/f'{t}-snapshot'/'results.csv')
    pers[t] = read(data/f'{t}-persistent'/'results.csv')
    seen = defaultdict(dict)
    for r in snap[t]:
        seen[r['scene'], int(r['n'])][r['engine']] = r['status']
    for scene in SNAP_SCENES:
        for n in (SMALL if scene in ('uniform', 'clustered') else ()) + LARGE:
            got = seen.get((scene, n), {})
            for engine in ('index', 'index-ordered', 'brute', 'sweep', 'grid'):
                status = got.get(engine) or ('row-capped' if '*' in got else 'MISSING')
                recon.append(('snapshot', t, scene, n, '', '', engine, status))
    pseen = defaultdict(dict)
    for r in pers[t]:
        pseen[r['scene'], int(r['n']), r['workload'], r['mode']][r['engine']] = r['status']
    engines = ('index', 'index-ordered', 'brute', 'sweep', 'grid', 'grid-rebuild') + \
        (('b2tree', 'b2tree-fat') if t in ('int32', 'float') else ())
    for scene in PERS_SCENES:
        for n in PERS_SIZES:
            for wl in WORKLOADS:
                for mode in ('full',) if wl in ('small-100%', 'noop-100%') else ('full', 'moved'):
                    got = pseen.get((scene, n, wl, mode), {})
                    for engine in engines:
                        if mode == 'moved' and engine == 'grid-rebuild':
                            continue
                        recon.append(('persistent', t, scene, n, wl, mode, engine, got.get(engine, 'MISSING')))
save('reconciliation.csv', ('stage', 'type', 'scene', 'n', 'workload', 'mode', 'engine', 'status'), recon)

g5 = []
for t in TYPES:
    by = defaultdict(dict)
    for r in snap[t]:
        by[r['scene'], int(r['n'])][r['engine']] = r
    for (scene, n), engines in sorted(by.items()):
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

FIELDS = ('status', 'estimate_ns', 'err_ns', 'median_ns', 'p95_ns', 'prepare_median_ns', 'pairs_median_ns',
          'candidates', 'node_visits', 'pairs', 'checksum', 'allocs_per_step', 'cold_allocs', 'peak_bytes',
          'retained_bytes')
lib = []
for t in TYPES:
    for stage, rows in (('snapshot', snap[t]), ('persistent', pers[t])):
        for r in rows:
            if r['engine'] in ('index', 'index-ordered'):
                lib.append((stage, t, r['scene'], r['n'], r['workload'], r['mode'], r['engine'],
                            *(r[f] for f in FIELDS)))
save('library-rows.csv', ('stage', 'type', 'scene', 'n', 'workload', 'mode', 'engine', *FIELDS), lib)

print('Process exits (benchmark, gate-files):', ', '.join(f'{s} {"/".join(e)}' for s, e in exits.items()))
print('Hard gate rows by release status:')
for t in TYPES:
    for model in ('snapshot', 'persistent'):
        counts = Counter()
        for (tt, mm, g, k, _, st), v in summary.items():
            if tt == t and mm == model and k == 'hard':
                counts[g, st] += v
        for g in sorted({g for g, _ in counts}):
            print(f'  {t:6} {model:10} {g:4}', ', '.join(f'{st} {n}' for (gg, st), n in sorted(counts.items()) if gg == g))
missing = [r for r in recon if r[-1] == 'MISSING']
print(f'Reconciliation: {len(recon)} expected engine rows, {len(missing)} missing')
for r in missing[:20]:
    print('  MISSING', *r[:-1])
print('Non-pass hard rows:')
for row in gate_rows:
    if row[3] == 'hard' and (row[-1] not in ('pass', 'published-pass') or row[11]):
        t, model, gate, _, subject, value, lo, hi, _, limit, verdict, pv, plo, phi, reps, final, status = row
        extra = f' -> pooled {pv} [{plo}-{phi}]' if pv else ''
        print(f'  {status:22} {gate:4} {subject[:70]:70} {value} [{lo}-{hi}] {verdict}{extra}')
