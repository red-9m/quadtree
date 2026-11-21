#!/usr/bin/env python3
"""Split archived unordered pair cycles by nearest traversal caller and leaf work."""
import runpy
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
P = runpy.run_path(str(HERE.parent / 'g2-bulk-classifier-2026-11-21/summarize.py'))
BASE, SAVE = P['BASE'], P['SAVE']
OWNERS = ('self', 'cross', 'holder', 'unresolved')
WORK = ('node_loop', 'exact', 'report')


def classify(stack, frames):
    chain = [f for pc in stack for f in frames.get(pc, ())]
    lib = [line for _, file, line in chain if file == 'quadtree-index-impl.h']
    owner = 'unresolved'
    for line in lib:
        if 3077 <= line <= 3093:
            owner = 'holder'
        elif 3096 <= line <= 3123:
            owner = 'cross'
        elif 3126 <= line <= 3148:
            owner = 'self'
        else:
            continue
        break
    report = any(2794 <= line <= 2800 for line in lib) or any(
        file == 'bench-index.h' and line == 101 or file == 'quadtree-bench.cpp' and 436 <= line <= 442
        for _, file, line in chain)
    return owner, 'report' if report else 'exact' if any(2775 <= line <= 2792 for line in lib) else 'node_loop'


def main(cap, out):
    read, key = BASE['rows'], BASE['key']
    parent = HERE.parent / 'g2-bulk-classifier-2026-11-21'
    assert (cap / 'text.sha256').read_text() == (parent / 'text.sha256').read_text()
    elf = (cap / 'elf-path.txt').read_text().strip()
    debug = cap / 'staged.debug'
    audit = P['sites'](debug)
    ref = {key(r): r for r in read(HERE.parent / 'g2-radix-outline-2026-11-21/frozen/9-control/snapshot.csv')}
    expected = {(r['block'], r['scene'], r['n']): int(r['period']) for r in read(parent / 'cycle-periods.csv')
                if r['engine'] == 'index' and r['phase'] == 'pairs'}
    runs, pcs = {}, set()
    for spec in read(cap / 'run-order.csv'):
        b, scene, n, engine, event = (spec[k] for k in ('block', 'scene', 'n', 'engine', 'event'))
        if engine != 'index' or event != 'cycles':
            continue
        stem = f'{b}-{scene}-{engine}-{event}'
        row, = read(cap / f'{stem}.csv')
        assert key(row) == (scene, n, engine) and row['type'] == 'int32' and row['status'] == 'ok'
        assert all(row[k] == ref[key(row)][k] for k in BASE['EXACT']) and float(row['allocs_per_step']) == 0
        events = list(BASE['chain_samples'](cap / f'{stem}.samples.gz', elf))
        pcs.update(pc for _, stack in events for pc in stack if isinstance(pc, int))
        runs[b, scene, n] = events
    assert len(runs) == 6 and expected.keys() == runs.keys()
    frames = BASE['symbolize'](debug, pcs)
    phases, sites = [], []
    for k, events in runs.items():
        counts, periods, leaves = Counter(), Counter(), Counter()
        for period, stack in events:
            if P['phase'](stack, frames, audit) != 'pairs':
                continue
            g = classify(stack, frames)
            periods[g] += period
            counts[g] += 1
            leaves[g, stack[0] if stack else 'unknown'] += period
        total = periods.total()
        assert total == expected[k] and counts.total() > 500, k
        for owner in OWNERS:
            for work in WORK:
                g = owner, work
                phases.append((*k, *g, counts[g], periods[g], f'{periods[g] * 100 / total:.3f}'))
        for (g, pc), p in leaves.most_common(30):
            chain = ' | '.join(f'{f}:{n}' for _, f, n in frames.get(pc, ()))
            sites.append((*k, *g, hex(pc) if isinstance(pc, int) else pc, p, f'{100 * p / total:.3f}', chain))
    SAVE(out / 'attribution.csv', 'block scene n owner work samples period pct_of_pairs', phases)
    SAVE(out / 'hot-pcs.csv', 'block scene n owner work pc period pct_of_pairs inline_locations', sites)
    print('Six staged Release captures match the frozen fixture/checksum/work rows; zero warm allocations.')
    print('Attribution reconciles with all six published unordered pair-cycle totals.')


if __name__ == '__main__':
    main(*(Path(arg) for arg in sys.argv[1:]))
