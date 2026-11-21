#!/usr/bin/env python3
"""Separate source and partner-row rank sorting by the orderedSelf callsite."""
import csv
import gzip
import io
import runpy
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
BASE = runpy.run_path(str(HERE.parent / 'g2-phase-profile-2026-11-19/summarize.py'))
SORTS = ('source_sort', 'row_sort', 'unresolved_sort')
PHASES = (*SORTS, 'other_pairs', 'build', 'other_process')


def classify(stack, frames):
    chain = [f for pc in stack for f in frames.get(pc, [])]
    # Line ranges are for the screened .text-matched debug twin. Demangled
    # names can mention one function in the template arguments of another.
    lib = {line for _, file, line in chain if file == 'quadtree-index-impl.h'}

    def loc(lo, hi):
        return any(lo <= line <= hi for line in lib)

    ordered = loc(2792, 2829)
    sort = loc(203, 285) or loc(2551, 2556)
    if loc(2095, 2289) or loc(1522, 1608) or loc(1784, 1878) or loc(2414, 2468) or any(
            file == 'bench-index.h' and 40 <= line <= 65 for _, file, line in chain):
        return 'build'
    if sort and ordered:
        assert not {2797, 2825} <= lib
        if 2797 in lib:
            return 'source_sort'
        if 2825 in lib:
            return 'row_sort'
        return 'unresolved_sort'
    if ordered or loc(1120, 1150) or loc(2580, 2621) or any(
            file == 'bench-index.h' and 98 <= line <= 104 for _, file, line in chain):
        return 'unresolved_sort' if sort else 'other_pairs'
    return 'other_process'


def verify_census(path, ref):
    rows = BASE['rows'](path)
    for scene in ('mixed-size', 'split-line-translated'):
        buckets = {r['bucket']: r for r in rows if r['scene'] == scene}
        assert len(buckets) == 6 and len(rows) == 12
        for row in buckets.values():
            assert all(row[k] == ref[scene][k] for k in ('pairs', 'checksum', 'candidates', 'node_visits')), scene
        source = buckets.pop('source')
        assert source['calls'] == '1' and source['elements'] == '100000'
        assert sum(int(r['calls']) for r in buckets.values()) == 99999
        assert sum(int(r['elements']) for r in buckets.values()) == int(source['pairs'])
        assert buckets['row_0_1']['comparisons'] == '0'
    print('Both comparison censuses match the screened benchmark pair counts, checksums and work.')


def main(captures, release, debug, out):
    read, samples, symbolize = (BASE[k] for k in ('rows', 'chain_samples', 'symbolize'))
    reference = HERE.parent / 'g2-grouped-child-screen-2026-11-20/15-candidate/snapshot.csv'
    ref = {r['scene']: r for r in read(reference) if r['n'] == '100000' and r['engine'] == 'index-ordered'}
    runs = {}
    with (captures / 'run-order.csv').open() as f:
        for spec in csv.DictReader(f):
            scene = spec['scene']
            run = f"{spec['block']}-{scene}"
            row, = read(captures / f'{run}.csv')
            assert (row['scene'], row['n'], row['engine'], row['batch']) == (scene, '100000', 'index-ordered', '1'), run
            assert row['rounds'] == '1' and row['status'] == 'ok' and float(row['allocs_per_step']) == 0, run
            assert all(row[k] == ref[scene][k] for k in BASE['EXACT']), run
            runs[run] = row, list(samples(captures / f'{run}.samples.gz', release))
    assert len(runs) == 6
    pcs = {pc for _, stacks in runs.values() for _, stack in stacks for pc in stack if isinstance(pc, int)}
    frames = symbolize(debug, pcs)
    with (out / 'attribution.csv').open('w') as summary, (out / 'phase-periods.csv').open('w') as phases, \
            (out / 'sort-callchains.csv.gz').open('wb') as packed, \
            gzip.GzipFile(filename='', mode='wb', mtime=0, fileobj=packed) as compressed, \
            io.TextIOWrapper(compressed, encoding='utf-8', newline='') as detail:
        table, totals, hist = (csv.writer(f, lineterminator='\n') for f in (summary, phases, detail))
        table.writerow(('run', 'scene', 'warm_reps', 'pair_samples', 'pair_pct_process',
                        *(p + '_pct_pairs' for p in SORTS), 'row_pct_resolved_sort'))
        totals.writerow(('run', 'phase', 'samples', 'period'))
        hist.writerow(('run', 'phase', 'samples', 'period', 'callsite_chain'))
        for run, (row, stacks) in runs.items():
            periods, counts, sort_periods, sort_counts = Counter(), Counter(), Counter(), Counter()
            for period, stack in stacks:
                phase = classify(stack, frames)
                periods[phase] += period
                counts[phase] += 1
                if phase in SORTS:
                    # The leaf PC alone cannot disambiguate the shared introsort body.
                    key = phase, stack
                    sort_periods[key] += period
                    sort_counts[key] += 1
            pair_period = sum(periods[p] for p in (*SORTS, 'other_pairs'))
            pair_count = sum(counts[p] for p in (*SORTS, 'other_pairs'))
            resolved = periods['source_sort'] + periods['row_sort']
            assert pair_count > 500 and periods['source_sort'] and periods['row_sort'] and periods['build'], run
            table.writerow((run, row['scene'], row['reps'], pair_count, f'{100 * pair_period / periods.total():.2f}',
                            *(f'{100 * periods[p] / pair_period:.2f}' for p in SORTS),
                            f"{100 * periods['row_sort'] / resolved:.2f}"))
            for phase in PHASES:
                totals.writerow((run, phase, counts[phase], periods[phase]))
            for (phase, stack), period in sort_periods.items():
                chain = [f'{hex(pc)} {name} @ {file}:{line}' for pc in stack if isinstance(pc, int)
                         for name, file, line in frames[pc]]
                hist.writerow((run, phase, sort_counts[phase, stack], period, ' <- '.join(chain)))
    print('Six Release profiles match the screened gate run: identical fixtures, results and work; zero warm allocations.')
    verify_census(out / 'census.csv', ref)


if __name__ == '__main__':
    captures, release, debug, out = sys.argv[1:]
    main(Path(captures), release, debug, Path(out))
