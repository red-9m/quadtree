#!/usr/bin/env python3
"""Re-bin the original Release DWARF callchains for the six failing G2 rows."""
import csv
import gzip
import runpy
import sys
from collections import Counter
from pathlib import Path


HERE = Path(__file__).resolve().parent
BASE = runpy.run_path(str(HERE.parent / 'g2-phase-profile-2026-11-19/summarize.py'))
SCENES = ('uniform', 'split-line', 'split-line-translated')
PHASES = ('cross_gate', 'cross_loop', 'items_gate', 'items_loop', 'walk_gate', 'walk_loop',
          'self_gate', 'self_loop', 'pair_filter', 'pair_geometry_budget', 'report', 'other_pairs')


def bucket(stack, frames):
    chain = [frame for pc in stack for frame in frames.get(pc, [])]
    # These names can appear inside a template argument. Use the lexical
    # source block for the operation and the enclosing node traversal site.
    traversal = [(file, line) for _, file, line in chain if file == 'quadtree-index-impl.h']
    sites = ((2533, 2561, 'cross'), (2514, 2531, 'items'), (2229, 2248, 'walk'),
             (2563, 2586, 'self'))
    context = next(((kind, line) for _, line in traversal for lo, hi, kind in sites if lo <= line <= hi), None)
    phase = BASE['classify'](stack, frames)
    if phase != 'pairs' and context is None:
        return None
    if any(file == 'bench-index.h' and line == 101 or file == 'quadtree-bench.cpp' and 436 <= line <= 442
           for _, file, line in chain) or any(2304 <= line <= 2310 or 2197 <= line <= 2202
                                              for _, line in traversal):
        return 'report', context[0] if context else 'none'
    for _, line in traversal:
        if 2287 <= line <= 2291:
            return 'pair_filter', context[0] if context else 'none'
        if 2284 <= line <= 2302 or 2312 <= line <= 2318:
            return 'pair_geometry_budget', context[0] if context else 'none'
    if not context:
        return 'other_pairs', 'none'
    kind, line = context
    gate = {'items': 2525, 'cross': 2547, 'walk': 2242, 'self': 2573}[kind]
    return kind + ('_gate' if line <= gate else '_loop'), kind


def main(captures, release, debug, out):
    read, samples, symbolize = (BASE[name] for name in ('rows', 'chain_samples', 'symbolize'))
    baseline = {(r['scene'], r['n']): r for r in read(HERE.parent / 'g2-phase-profile-2026-11-19/baseline/snapshot.csv')
                if r['engine'] == 'index'}
    trace = {}
    for block in (1, 2):
        for scene in SCENES:
            for n in (10000, 100000):
                run = f'{block}-{scene}-{n}'
                row, = read(captures / f'{run}.csv')
                assert row['status'] == 'ok' and row['allocs_per_step'] == '0.000', run
                assert (row['scene'], row['n'], row['engine'], row['batch']) == (scene, str(n), 'index', '1')
                assert all(row[f] == baseline[scene, str(n)][f] for f in BASE['EXACT']), run
                trace[run] = scene, n, list(samples(captures / f'{run}.samples.gz', release))
    pcs = {pc for _, _, samples in trace.values() for _, stack in samples for pc in stack if isinstance(pc, int)}
    frames = symbolize(debug, pcs)
    with (out / 'pair-summary.csv').open('w') as summary, (out / 'pair-sites.csv').open('w') as sites, \
         gzip.open(out / 'pair-pcs.csv.gz', 'wt') as detail:
        table, scope, hist = (csv.writer(f, lineterminator='\n') for f in (summary, sites, detail))
        table.writerow(('run', 'scene', 'n', 'pair_samples', 'pair_period', 'pairs_pct_process',
                        *(phase + '_pct_pairs' for phase in PHASES)))
        scope.writerow(('run', 'phase', 'site', 'samples', 'period', 'pct_pairs'))
        hist.writerow(('run', 'phase', 'site', 'pc', 'samples', 'period', 'inline_frames'))
        for run, (scene, n, samples) in trace.items():
            periods, counts = Counter(), Counter()
            for period, stack in samples:
                rank = bucket(stack, frames)
                if rank:
                    k = (*rank, stack[0] if stack else 'unknown')
                    periods[k] += period
                    counts[k] += 1
            groups, group_counts, phases = Counter(), Counter(), Counter()
            pair_period = periods.total()
            assert pair_period > 0 and counts.total() > 500, run
            for (phase, site, pc), period in periods.most_common():
                groups[phase, site] += period
                group_counts[phase, site] += counts[phase, site, pc]
                phases[phase] += period
                symbol = ' <- '.join(f'{name} @ {file}:{line}' for name, file, line in frames.get(pc, ()))
                hist.writerow((run, phase, site, hex(pc) if isinstance(pc, int) else pc,
                               counts[phase, site, pc], period, symbol))
            for (phase, site), period in groups.most_common():
                scope.writerow((run, phase, site, group_counts[phase, site], period, f'{100 * period / pair_period:.2f}'))
            table.writerow((run, scene, n, counts.total(), pair_period,
                            f'{100 * pair_period / sum(period for period, _ in samples):.2f}',
                            *(f'{100 * phases[p] / pair_period:.2f}' for p in PHASES)))
    print('12 profiles match retained fixture hashes, pair counts, checksums and traversal counts with zero warm allocations.')


if __name__ == '__main__':
    captures, release, debug, out = sys.argv[1:]
    main(Path(captures), release, debug, Path(out))
