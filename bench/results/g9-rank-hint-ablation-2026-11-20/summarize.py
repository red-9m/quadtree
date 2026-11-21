#!/usr/bin/env python3
"""Report paired block comparisons without treating diagnostic runs as gates."""
import csv
import gzip
import itertools
import math
import runpy
import statistics
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
BASE = runpy.run_path(str(HERE.parent / 'g2-integer-key-2026-11-19/summarize.py'))
read, key, save = (BASE[name] for name in ('read_rows', 'key', 'save'))
VARIANTS = ('control', 'hintbin', 'nohintx')
COMPARE = (('hintbin', 'nohintx'), ('hintbin', 'control'), ('nohintx', 'control'))
SCENES = ('uniform', 'split-line-translated')
ENGINES = ('index', 'index-ordered')
KEYS = {(s, 100000, e) for s in SCENES for e in ENGINES}
PHASES = ('estimate_ns', 'prepare_median_ns', 'pairs_median_ns')
LABELS = ('step', 'build', 'pairs')


def main(out):
    frozen = HERE.parent / 'g9-home-lookahead-2026-11-20/frozen/15-control/snapshot.csv'
    ref = {key(r): r for r in read(frozen)}
    orders, runs = defaultdict(list), defaultdict(dict)
    raw_count = 0
    for spec in read(out / 'run-order.csv'):
        block, kind = int(spec['block']), spec['kind']
        path = out / f'{block}-{kind}'
        orders[block].append(kind)
        assert kind not in runs[block] and kind in VARIANTS
        rows = read(path.with_suffix('.csv'))
        assert len(rows) == len(KEYS) and {key(r) for r in rows} == KEYS
        runs[block][kind] = {key(r): r for r in rows}
        raw = read(Path(f'{path}-raw.csv.gz'), gzip.open)
        raw_count += len(raw)
        counts, rounds = Counter(), defaultdict(set)
        for sample in raw:
            k = key(sample)
            counts[k] += 1
            assert (sample['model'], sample['type'], sample['batch']) == ('snapshot', 'int32', '1')
            rounds[k].add(int(sample['round']))
            assert all(int(sample[f]) > 0 for f in ('step_ns', 'prepare_ns', 'pairs_ns'))
        assert counts.keys() == KEYS
        for r in rows:
            k = key(r)
            assert (r['model'], r['type'], r['batch'], r['rounds'], r['status']) == ('snapshot', 'int32', '1', '15', 'ok')
            assert counts[k] == int(r['reps']) and rounds[k] == set(range(15)), (path, k)
            assert float(r['allocs_per_step']) == 0 and all(r[f] == ref[k][f] for f in BASE['COUNTS']), (path, k)
    assert tuple(orders) == tuple(range(1, 7))
    assert {tuple(o) for o in orders.values()} == set(itertools.permutations(VARIANTS))

    recs, aggregate = [], defaultdict(list)
    for block, variants in runs.items():
        for scene, n, engine in sorted(KEYS):
            k = scene, n, engine
            for kind, ref_kind in COMPARE:
                x, y = variants[ref_kind][k], variants[kind][k]
                ratios = tuple(float(y[f]) / float(x[f]) for f in PHASES)
                half = math.hypot(*(float(r['err_ns']) / float(r['estimate_ns']) for r in (x, y)))
                step, build, pairs = ratios
                lower, upper = step * math.exp(-half), step * math.exp(half)
                ms = tuple((float(y[f]) - float(x[f])) / 1e6 for f in PHASES)
                recs.append((block, *k, kind, ref_kind, *(x[f] for f in PHASES), *(y[f] for f in PHASES),
                             f'{step:.5f}', f'{lower:.5f}', f'{upper:.5f}', f'{build:.5f}', f'{pairs:.5f}'))
                aggregate[k, kind, ref_kind].append((ratios, ms, lower, upper))
    summary = []
    for (k, kind, ref_kind), blocks in aggregate.items():
        assert len(blocks) == 6
        for i, name in enumerate(LABELS):
            rel = [100 * (row[0][i] - 1) for row in blocks]
            ms = [row[1][i] for row in blocks]
            faster, slower = (sum(r[3] < 1 for r in blocks), sum(r[2] > 1 for r in blocks)) if name == 'step' else ('', '')
            summary.append((*k, kind, ref_kind, name, *(f'{v:.3f}' for v in (statistics.median(rel), min(rel), max(rel),
                            statistics.median(ms), min(ms), max(ms))), sum(x < 0 for x in rel), faster, slower))
    save(out / 'block-comparisons.csv', 'block scene n engine kind reference reference_step_ns reference_build_ns '
         'reference_pairs_ns step_ns build_ns pairs_ns step_ratio step_ci_low step_ci_high build_ratio pairs_ratio', recs)
    save(out / 'summary.csv', 'scene n engine kind reference phase median_change_pct min_change_pct max_change_pct '
         'median_delta_ms min_delta_ms max_delta_ms lower_in_blocks resolved_faster resolved_slower', summary)
    print(f'{raw_count} warm samples, 18 processes, 72 rows: fixtures, pair checksums, work, allocations and footprint match.')
    print('Each treatment ran twice in each block position. All warmed library rows allocate zero.')


if __name__ == '__main__':
    main(Path(sys.argv[1]))
