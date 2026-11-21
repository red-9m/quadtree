#!/usr/bin/env python3
"""Attribute ordered-row work on the screened and radix Release ELFs."""
import csv
import runpy
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
SORT = runpy.run_path(str(HERE.parent / 'g9-order-sort-profile-2026-11-20/summarize.py'))
BASE = SORT['BASE']
PHASES = ('source_sort', 'row_sort', 'row_radix', 'unresolved_sort', 'other_pairs', 'build', 'other_process')


def classify(kind, stack, frames):
    if kind == 'control':
        return SORT['classify'](stack, frames)
    chain = [f for pc in stack for f in frames.get(pc, [])]
    lib = {line for _, file, line in chain if file == 'quadtree-index-impl.h'}
    if any(2791 <= line <= 2831 for line in lib):
        return 'row_radix'  # Includes caching, histograms, scatters, and write-back.
    if 2870 in lib:
        return 'row_radix'  # Inlined/outlined callsite, including any unattributed call overhead.
    # Retain the control classifier for unaffected scopes, shifting only the
    # orderedSelf lines and remapping its two fallback sort callsites.
    adjusted = {}
    for pc in set(stack) & frames.keys():
        adjusted[pc] = [(name, file, translate(file, line)) for name, file, line in frames[pc]]
    return SORT['classify'](stack, adjusted)


def translate(file, line):
    if file != 'quadtree-index-impl.h' or not 2834 <= line <= 2879:
        return line
    if line in (2872, 2875):
        return 2825
    if line <= 2866:
        return line - 42
    if line >= 2876:
        return line - 50
    return 2824


def main(captures, out):
    read, samples, symbolize = (BASE[k] for k in ('rows', 'chain_samples', 'symbolize'))
    reference = HERE.parent / 'g2-grouped-child-screen-2026-11-20/15-candidate/snapshot.csv'
    ref, = [r for r in read(reference) if (r['scene'], r['n'], r['engine']) == ('mixed-size', '100000', 'index-ordered')]
    sites = Counter()
    report = []
    with (captures / 'profile-order.csv').open() as f:
        for spec in csv.DictReader(f):
            kind = spec['source']
            run = f"{spec['round']}-{kind}"
            row, = read(captures / f'{run}.csv')
            assert (row['scene'], row['n'], row['engine'], row['batch']) == ('mixed-size', '100000', 'index-ordered', '1'), run
            assert row['status'] == 'ok' and float(row['allocs_per_step']) == 0, run
            assert all(row[k] == ref[k] for k in BASE['EXACT']), run
            elf = (captures / f'{kind}.elf-path.txt').read_text().strip()
            stacks = list(samples(captures / f'{run}.samples.gz', elf))
            pcs = {pc for _, chain in stacks for pc in chain if isinstance(pc, int)}
            frames = symbolize(str(captures / f'{kind}.debug'), pcs)
            periods, counts = Counter(), Counter()
            for period, chain in stacks:
                phase = classify(kind, chain, frames)
                periods[phase] += period
                counts[phase] += 1
                if phase in ('row_sort', 'row_radix'):
                    sites[run, phase, chain[0]] += period
            pair = sum(periods[p] for p in PHASES[:-2])
            assert pair and periods['build'] and periods['row_sort'] and periods['source_sort'], run
            assert bool(periods['row_radix']) == (kind == 'candidate'), run
            assert periods['unresolved_sort'] == 0, run
            report.append((run, row['reps'], row['pairs_median_ns'], pair, sum(counts[p] for p in PHASES[:-2]),
                           *(f'{100 * periods[p] / pair:.2f}' for p in PHASES[:4]),
                           f"{100 * (periods['row_sort'] + periods['row_radix']) / pair:.2f}",
                           f'{100 * pair / periods.total():.2f}'))
    assert len(report) == 6
    with (out / 'attribution.csv').open('w') as f:
        writer = csv.writer(f, lineterminator='\n')
        writer.writerow(('run', 'warm_reps', 'profile_pairs_median_ns', 'pair_period', 'pair_samples',
                         *(p + '_pct_pairs' for p in PHASES[:4]), 'all_row_sort_pct_pairs', 'pair_pct_process'))
        writer.writerows(report)
    with (out / 'row-pc-periods.csv').open('w') as f:
        writer = csv.writer(f, lineterminator='\n')
        writer.writerow(('run', 'phase', 'leaf_pc', 'period'))
        for (run, phase, pc), period in sorted(sites.items(), key=lambda item: str(item[0])):
            writer.writerow((run, phase, hex(pc) if isinstance(pc, int) else pc, period))
    print('All six Release profiles match fixed fixtures, checksum and work; zero warm allocations and no unresolved pair-sort sample.')


if __name__ == '__main__':
    captures, out = map(Path, sys.argv[1:])
    main(captures, out)
