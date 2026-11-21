#!/usr/bin/env python3
"""Attribute sampled cycle periods on the hash-checked two-stream control."""
import csv
import gzip
import io
import runpy
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
BASE = runpy.run_path(str(HERE.parent / 'g2-phase-profile-2026-11-19/summarize.py'))
read, stacks, symbolize, key = (BASE[k] for k in ('rows', 'chain_samples', 'symbolize', 'key'))
PAIR = ('source_sort', 'row_sort', 'ordered_walk', 'ordered_other', 'unordered_pairs', 'unresolved_pair_sort')
PHASES = (*PAIR, 'build', 'sort_unknown_phase', 'other_process')


def classify(stack, frames, engine):
    chain = [frame for pc in stack for frame in frames.get(pc, [])]
    lib = {line for _, file, line in chain if file == 'quadtree-index-impl.h'}
    adapter = {line for _, file, line in chain if file == 'bench-index.h'}
    step = {line for _, file, line in chain if file == 'quadtree-bench.cpp'}

    def loc(lo, hi):
        return any(lo <= line <= hi for line in lib)

    # These are line ranges in the staged .text-matched source, not the tree at HEAD.
    # Build has its own rank validation using the same generic sorting helpers.
    if 684 in step or any(40 <= line <= 65 for line in adapter) or loc(2198, 2544):
        return 'build'
    pair = (686 in step or any(98 <= line <= 104 for line in adapter) or loc(1192, 1218)
            or loc(2698, 2741) or loc(2775, 2806) or loc(2909, 2997) or loc(3067, 3165))
    sort = loc(203, 285) or loc(2662, 2666)
    if engine == 'index-ordered' and pair:
        source, row = 2957 in lib, loc(2987, 2993) or loc(2909, 2949)
        assert not (source and row), lib
        if source:
            return 'source_sort'
        if row:
            return 'row_sort'
        if sort:
            return 'unresolved_pair_sort'
        if loc(2698, 2740) or loc(2962, 2968) or loc(2973, 2981):
            return 'ordered_walk'
        return 'ordered_other'
    if pair:
        assert not sort, lib
        return 'unordered_pairs'
    return 'sort_unknown_phase' if sort else 'other_process'


def main(captures, out):
    release = (captures / 'elf-path.txt').read_text().strip()
    refpath = HERE.parent / 'g2-two-scatter-thermal-2026-11-21/frozen/1/snapshot.csv'
    ref = {key(row): row for row in read(refpath)}
    runs = {}
    for spec in read(captures / 'run-order.csv'):
        scene, engine = spec['scene'], spec['engine']
        run = f"{spec['block']}-{scene}-{engine}"
        row, = read(captures / f'{run}.csv')
        assert key(row) == (scene, '100000', engine), run
        assert row['batch'] == '1' and row['rounds'] == '1' and row['status'] == 'ok', run
        assert float(row['allocs_per_step']) == 0 and int(row['reps']) >= 20, run
        assert all(row[k] == ref[key(row)][k] for k in BASE['EXACT']), run
        assert run not in runs
        runs[run] = row, list(stacks(captures / f'{run}.samples.gz', release))
    assert len(runs) == 8
    pcs = {pc for _, samples in runs.values() for _, stack in samples for pc in stack if isinstance(pc, int)}
    frames = symbolize(captures / 'staged.debug', pcs)
    with (out / 'attribution.csv').open('w') as f, (out / 'phase-periods.csv').open('w') as p, \
            (out / 'sort-callchains.csv.gz').open('wb') as packed, \
            gzip.GzipFile(filename='', mode='wb', mtime=0, fileobj=packed) as compressed, \
            io.TextIOWrapper(compressed, encoding='utf-8', newline='') as detail:
        results, phases, chains = (csv.writer(x, lineterminator='\n') for x in (f, p, detail))
        results.writerow(('run', 'scene', 'engine', 'warm_reps', 'pair_samples', 'pair_pct_process',
                          *(phase + '_pct_pairs' for phase in PAIR)))
        phases.writerow(('run', 'phase', 'samples', 'cycle_period'))
        chains.writerow(('run', 'phase', 'samples', 'cycle_period', 'symbolized_chain'))
        for run, (row, samples) in runs.items():
            periods, counts, sort_periods, sort_counts = Counter(), Counter(), Counter(), Counter()
            for period, stack in samples:
                phase = classify(stack, frames, row['engine'])
                periods[phase] += period
                counts[phase] += 1
                if phase in ('source_sort', 'row_sort', 'unresolved_pair_sort', 'sort_unknown_phase'):
                    sort_periods[phase, stack] += period
                    sort_counts[phase, stack] += 1
            pair = sum(periods[p] for p in PAIR)
            pair_count = sum(counts[p] for p in PAIR)
            assert pair_count > 500 and periods['build'], run
            if row['engine'] == 'index-ordered':
                assert periods['source_sort'] and periods['row_sort'] and periods['ordered_walk'], run
            else:
                assert pair == periods['unordered_pairs'] and not periods['sort_unknown_phase'], run
            assert sum(periods[p] for p in PHASES) == periods.total(), run
            results.writerow((run, row['scene'], row['engine'], row['reps'], pair_count,
                              f'{100 * pair / periods.total():.2f}',
                              *(f'{100 * periods[p] / pair:.2f}' for p in PAIR)))
            for phase in PHASES:
                phases.writerow((run, phase, counts[phase], periods[phase]))
            for (phase, stack), period in sort_periods.items():
                chain = [f'{pc:#x} {name} @ {file}:{line}' for pc in stack if isinstance(pc, int)
                         for name, file, line in frames[pc]]
                chains.writerow((run, phase, sort_counts[phase, stack], period, ' <- '.join(chain)))
    print('Eight Release profiles match the frozen control fixtures, pair checksums and work; zero warm allocations.')
    print('Four unordered negative controls have no query sort samples.')


if __name__ == '__main__':
    main(*(Path(arg) for arg in sys.argv[1:]))
