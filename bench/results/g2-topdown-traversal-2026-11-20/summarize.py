#!/usr/bin/env python3
"""Attribute sampled user cycles in the staged top-down Release executable."""
import csv
import gzip
import io
import runpy
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
BASE = runpy.run_path(str(HERE.parent / 'g2-phase-profile-2026-11-19/summarize.py'))
PHASES = ('walk_gate', 'walk_iteration', 'rank_check_and_collect', 'pair_eligibility',
          'report', 'rank_sort', 'cross_nodes', 'self_nodes', 'items_vs_subtree', 'other_pairs')


def classify(stack, frames):
    # Prefer source ranges in the byte-matched candidate over demangled names,
    # which can contain the name of a different operation in template arguments.
    chain = [f for pc in stack for f in frames.get(pc, [])]
    lib = [line for _, file, line in chain if file == 'quadtree-index-impl.h']

    def loc(lo, hi):
        return any(lo <= line <= hi for line in lib)

    if loc(2094, 2278) or loc(1521, 1607) or loc(1783, 1877) or loc(2413, 2464) or any(
            file == 'bench-index.h' and 40 <= line <= 65 for _, file, line in chain):
        return 'build'
    if not (loc(1119, 1149) or loc(2535, 2626) or loc(2725, 2751) or loc(2820, 2893) or any(
            file == 'bench-index.h' and 98 <= line <= 104 for _, file, line in chain)):
        return 'other_process'
    if loc(202, 284) or loc(2528, 2532):
        return 'rank_sort'
    if loc(1355, 1364) or loc(2583, 2587) or loc(2610, 2616) or loc(2502, 2510) or any(
            (file == 'bench-index.h' and line == 101) or (file == 'quadtree-bench.cpp' and 436 <= line <= 442)
            for _, file, line in chain):
        return 'report'
    if loc(2590, 2607):
        return 'pair_eligibility'
    if loc(2736, 2741):
        return 'rank_check_and_collect'
    for line in lib:
        if 2535 <= line <= 2547:
            return 'walk_gate'
        if 2548 <= line <= 2553:
            return 'walk_iteration'
        if 2839 <= line <= 2866:
            return 'cross_nodes'
        if 2869 <= line <= 2892:
            return 'self_nodes'
        if 2820 <= line <= 2836:
            return 'items_vs_subtree'
    return 'other_pairs'


def verify_census(path, ref):
    rows = BASE['rows'](path)
    assert len(rows) == 5
    for row in rows:
        scene, engine = row['scene'], row['engine']
        assert all(row[k] == ref[scene, engine][k] for k in ('pairs', 'checksum', 'candidates', 'node_visits'))
        c = {k: int(v) for k, v in row.items() if k not in ('scene', 'engine', 'checksum')}
        walk = sum(c['walk_' + kind] for kind in ('disjoint', 'filter', 'admitted'))
        cross = sum(c['cross_' + kind] for kind in ('disjoint', 'filter', 'admitted'))
        if engine == 'index-ordered':
            assert walk == c['node_visits'] and cross == 0
            assert c['rank_checks'] - c['rank_rejects'] == c['candidates']  # all filters pass on these fixtures
            assert sum(v for k, v in c.items() if k.startswith('rows_')) == 99999
            assert sum(v for k, v in c.items() if k.startswith('partners_')) == c['pairs']
        else:
            assert 0 < walk + cross < c['node_visits']  # remaining charges are selfPairs
    print('Five census rows match benchmark pair counts, checksums, candidates and node visits.')


def main(captures, release, debug, out):
    read, samples, symbolize = (BASE[name] for name in ('rows', 'chain_samples', 'symbolize'))
    ref = {(r['scene'], r['engine']): r for r in read(HERE.parent / 'g2-topdown-build-2026-11-20/15-candidate/snapshot.csv')
           if r['n'] == '100000'}
    runs = {}
    with (captures / 'run-order.csv').open() as order:
        for spec in csv.DictReader(order):
            scene, engine = spec['scene'], spec['engine']
            run = f"{spec['block']}-{scene}-{engine}"
            row, = read(captures / f'{run}.csv')
            assert (row['scene'], row['n'], row['engine'], row['batch']) == (scene, '100000', engine, '1'), run
            assert row['rounds'] == '1' and row['status'] == 'ok' and float(row['allocs_per_step']) == 0, run
            assert all(row[f] == ref[scene, engine][f] for f in BASE['EXACT']), run
            runs[run] = row, list(samples(captures / f'{run}.samples.gz', release))
    assert len(runs) == 10
    pcs = {pc for _, stacks in runs.values() for _, stack in stacks for pc in stack if isinstance(pc, int)}
    frames = symbolize(debug, pcs)
    with (out / 'attribution.csv').open('w') as summary, (out / 'profile-rows.csv').open('w') as results, \
         (out / 'phase-periods.csv').open('w') as totals, (out / 'pc-histogram.csv.gz').open('wb') as packed, \
         gzip.GzipFile(filename='', mode='wb', mtime=0, fileobj=packed) as compressed:
        include = ('run', 'scene', 'n', 'engine', 'warm_reps', 'pair_samples', 'pairs_pct_process')
        table, raw, count = (csv.writer(f, lineterminator='\n') for f in (summary, results, totals))
        table.writerow((*include, *(name + '_pct_pairs' for name in PHASES)))
        raw.writerow(('run', *next(iter(runs.values()))[0]))
        count.writerow(('run', 'phase', 'samples', 'period'))
        with io.TextIOWrapper(compressed, encoding='utf-8', newline='') as detail:
            hist = csv.writer(detail, lineterminator='\n')
            hist.writerow(('run', 'phase', 'leaf_pc', 'samples', 'period', 'inline_frames'))
            for run, (row, stacks) in runs.items():
                periods, counts, per_pc, pc_counts = Counter(), Counter(), Counter(), Counter()
                for period, stack in stacks:
                    phase = classify(stack, frames)
                    leaf = stack[0] if stack else 'unknown'
                    periods[phase] += period
                    counts[phase] += 1
                    per_pc[phase, leaf] += period
                    pc_counts[phase, leaf] += 1
                pair_period = sum(periods[p] for p in PHASES)
                pair_samples = sum(counts[p] for p in PHASES)
                assert pair_period and pair_samples > 500 and periods['build'], run
                table.writerow((run, row['scene'], row['n'], row['engine'], row['reps'], pair_samples,
                                f'{100 * pair_period / periods.total():.2f}',
                                *(f'{100 * periods[p] / pair_period:.2f}' for p in PHASES)))
                raw.writerow((run, *row.values()))
                for phase in (*PHASES, 'build', 'other_process'):
                    count.writerow((run, phase, counts[phase], periods[phase]))
                for (phase, pc), period in sorted(per_pc.items(), key=lambda k: (k[0][0], str(k[0][1]))):
                    symbol = ' <- '.join(f'{name} @ {file}:{line}' for name, file, line in frames.get(pc, ()))
                    hist.writerow((run, phase, hex(pc) if isinstance(pc, int) else pc, pc_counts[phase, pc], period, symbol))
    print('Ten Release profiles match the staged 15-round rows: same fixtures, results and work; zero warm allocations.')
    verify_census(out / 'census.csv', ref)


if __name__ == '__main__':
    captures, release, debug, out = sys.argv[1:]
    main(Path(captures), str(Path(release).resolve()), debug, Path(out))
