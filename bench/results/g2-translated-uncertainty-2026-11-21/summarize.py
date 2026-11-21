#!/usr/bin/env python3
"""Reconstruct published frozen ratios; project dispersion, never change the verdict."""
import csv
import gzip
import math
import re
import statistics as st
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
frozen = HERE.parent / 'g2-packed-cross-item-2026-11-21/frozen'
out = Path(sys.argv[1])
scene, n = 'split-line-translated', '100000'
engines = ('grid', 'index', 'index-ordered')
process = 0.02  # Frozen default; checked against the published err_ns below.
t95 = {9: 2.306, 15: 2.145, 30: 2.045, 45: 1.96, 60: 1.96}


def rows(path, opener=open):
    with opener(path, 'rt') as f:
        return list(csv.DictReader(l for l in f if not l.startswith('#')))


def emit(name, header, rows):
    with (out/name).open('w') as f:
        w = csv.writer(f, lineterminator='\n'); w.writerow(header.split()); w.writerows(rows)


def keep(r):
    return r['scene'] == scene and r['n'] == n and r['engine'] in engines


raw_rounds, fitted, projections = [], [], []
for rounds in (9,15):
    for kind in ('control','candidate'):
        run = f'{rounds}-{kind}'
        p = frozen/run
        ref = {r['engine']: r for r in rows(p/'snapshot.csv') if keep(r)}
        assert set(ref) == set(engines)
        samples = defaultdict(list)
        for r in rows(p/'snapshot-raw.csv.gz', gzip.open):
            if keep(r):
                assert r['type'] == 'int32' and r['batch'] == '1'
                samples[r['engine'],int(r['round'])].append(tuple(int(r[f]) for f in ('step_ns','prepare_ns','pairs_ns')))
        assert set(samples) == {(e,i) for e in engines for i in range(rounds)}
        fit = {}
        for engine in engines:
            r = ref[engine]
            assert r['status'] == 'ok' and int(r['rounds']) == rounds
            data = []
            for i in range(rounds):
                vals = samples[engine,i]
                step, build, pairs = (st.median(v) for v in zip(*vals))
                data.append(step)
                raw_rounds.append((run,engine,i,len(vals),step,build,pairs))
            estimate, sd = st.mean(data), st.stdev(data)
            round_err = t95[rounds]*sd/math.sqrt(rounds)
            error = math.hypot(round_err, process*estimate)
            assert all(abs(a-float(r[k])) <= 0.051 for a,k in ((estimate,'estimate_ns'),(round_err,'round_err_ns'),(error,'err_ns'))), (run,engine)
            assert sum(len(samples[engine,i]) for i in range(rounds)) == int(r['reps'])
            fit[engine] = estimate,sd
            fitted.append((run,engine,estimate,f'{sd:.3f}',f'{round_err:.3f}',f'{error:.3f}',
                           f'{100*round_err/estimate:.4f}',f'{100*error/estimate:.4f}'))
        for gate,a,b,limit in (('G2','index','grid',2),('G9','index-ordered','index',4)):
            point=fit[a][0]/fit[b][0]
            for projected in (rounds,30,45,60,0):
                h=math.hypot(*(math.hypot(t95[projected]*fit[e][1]/math.sqrt(projected)/fit[e][0] if projected else 0, process) for e in (a,b)))
                lo,hi=point*math.exp(-h),point*math.exp(h)
                if projected == rounds:
                    line,= [line for line in (p/'gates.txt').read_text().splitlines()
                             if line.startswith(gate+' ') and f'{scene} n={n} ' in line]
                    matched=re.search(r'([\d.]+) \[([\d.]+)-([\d.]+)\]\s+<= ',line)
                    assert (f'{point:.3f}',f'{lo:.3f}',f'{hi:.3f}') == matched.groups(), line
                projections.append((run,gate,projected or 'infinite',f'{point:.6f}',f'{lo:.6f}',f'{hi:.6f}',limit))
emit('round-medians.csv','run engine round reps step_median_ns build_median_ns pair_median_ns',raw_rounds)
emit('error-decomposition.csv','run engine estimate_ns sd_ns round_err_ns total_err_ns relative_round_pct relative_total_pct',fitted)
emit('bounds-projection.csv','run gate assumed_rounds ratio lower upper limit',projections)
print('All 12 engine estimates/errors and eight published gate intervals reconstructed; no rounds excluded.')
