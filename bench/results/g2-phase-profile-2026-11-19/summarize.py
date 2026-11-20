#!/usr/bin/env python3
import csv
import gzip
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path


BUILD = ('adapter', 'capture', 'make_key', 'radix_sort', 'count_nodes', 'cut_unresolved',
         'entry_commit', 'topology_commit', 'build_other')
PHASES = (*BUILD, 'pairs', 'other')
EXACT = 'scene_hash pairs checksum candidates node_visits references max_refs cell_tests'.split()


def rows(path):
    with path.open() as f:
        return list(csv.DictReader(line for line in f if not line.startswith('#')))


def chain_samples(path, release):
    # Callchain mode prints a period on one line, then the leaf IP and callers.
    with gzip.open(path, 'rt') as f:
        for record in f.read().strip().split('\n\n'):
            header, *stack = record.strip().splitlines()
            frames = []
            for i, frame in enumerate(stack):
                _, dso = frame.strip().split(maxsplit=1)
                name, sep, off = dso[1:-1].rpartition('+0x')
                if sep and name == release:
                    # addr2line expects the call site, not the caller's return PC.
                    frames.append(int(off, 16) - bool(i))
                else:
                    frames.append(Path(name or dso[1:-1]).name)
            yield int(header.strip()), tuple(frames)


def symbolize(debug, pcs):
    result = subprocess.run(['addr2line', '-aifC', '-e', debug], check=True, text=True,
                            input=''.join(f'{pc:#x}\n' for pc in sorted(pcs)),
                            stdout=subprocess.PIPE)
    lines = iter(result.stdout.splitlines())
    frames = defaultdict(list)
    for line in lines:
        if re.fullmatch(r'0x[\da-f]+', line):
            pc = int(line, 16)
        else:
            file, _, source_line = next(lines).split(' (discriminator', 1)[0].rpartition(':')
            frames[pc].append((line, Path(file).name, int(source_line) if source_line.isdigit() else 0))
    assert frames.keys() == pcs
    return frames


def classify(stack, frames):
    chain = [frame for pc in stack for frame in frames.get(pc, [])]
    names = [name for name, _, _ in chain]

    def member(name):
        return any(f'::{name}(' in f or f'::{name}<' in f for f in names)

    if member('makeKey'):
        return 'make_key'
    if member('radixSort'):
        return 'radix_sort'
    if member('countNodes'):
        return 'count_nodes'
    if member('materialize') or member('rebuild'):
        return 'topology_commit'
    for name, file, line in chain:
        if '::buildPrepared<' in name and file == 'quadtree-index-impl.h':
            if line == 1941:
                return 'make_key'
            if line == 1942:
                return 'radix_sort'
            if line == 1943:
                return 'count_nodes'
            if line == 1995:
                return 'topology_commit'
            if member('cut'):
                return 'cut_unresolved'
            if member('constructBuildEntry') or 1954 <= line <= 1994:
                return 'entry_commit'
            if 1911 <= line <= 1937:
                return 'capture'
            return 'build_other'
    if member('cut'):
        return 'cut_unresolved'
    if member('constructBuildEntry'):
        return 'entry_commit'
    if member('prepareReplacement') or member('growReplacement') or member('buildRange'):
        return 'build_other'
    if any(file == 'bench-index.h' and 40 <= line <= 65 for _, file, line in chain):
        return 'adapter'
    if any(member(name) for name in ('pairs', 'forEachPair', 'selfPairs', 'crossPairs', 'itemsVsSubtree',
                                     'pairQualifies', 'testPair', 'reportPair')):
        return 'pairs'
    return 'other'


def key(row):
    return row['scene'], row['n'], row['engine']


def main(out, release, debug):
    reference = Path(__file__).resolve().parent.parent / 'current-snapshot-2026-11-18/repeat/snapshot.csv'
    history = {key(row): row for row in rows(reference)}
    timings = {}
    for name in ('baseline', 'repeat'):
        directory = out / name
        if name == 'repeat' and not directory.exists():
            continue
        timing = timings[name] = {key(row): row for row in rows(directory / 'snapshot.csv')}
        assert len(timing) == len(history) == 204 and timing.keys() == history.keys()
        assert (directory / 'run.exit').read_text().strip() == '0'
        assert (directory / 'gates.exit').read_text().strip() == '3'
        for k, row in timing.items():
            assert row['status'] == history[k]['status'] and row['scene_hash'] == history[k]['scene_hash'], k
            if int(row['n']) >= 1000 and row['status'] == 'ok':
                assert all(row[f] == history[k][f] for f in EXACT), k
            if row['engine'] in ('index', 'index-ordered'):
                assert row['status'] == 'ok' and float(row['allocs_per_step']) == 0, k
        print(f'{name}: 204 rows match historical status and fixture hashes; all uncapped G2-size rows '
              'match work and checksums; 68 library rows have zero warm allocations.')
    baseline = timings['baseline']

    profiles = {}
    for path in sorted(out.glob('*.samples.gz')):
        stem = path.name.removesuffix('.samples.gz')
        row, = rows(out / f'{stem}.csv')
        assert row['status'] == 'ok' and row['batch'] == '1' and row['engine'] == 'index', stem
        assert row['rounds'] == '1' and int(row['reps']) >= 20 and float(row['allocs_per_step']) == 0, stem
        assert all(row[f] == baseline[key(row)][f] for f in EXACT), stem
        profiles[stem] = row, list(chain_samples(path, release))
    assert len(profiles) == 14
    cases = sorted({key(row)[:2] for row, _ in profiles.values()})
    with (out / 'g2-budget.csv').open('w') as f:
        writer = csv.writer(f, lineterminator='\n')
        writer.writerow(('run', 'scene', 'n', 'index_step_ns', 'index_error_ns', 'index_build_median_ns',
                         'index_pairs_median_ns', 'grid_step_ns', 'grid_error_ns', 'grid_build_median_ns',
                         'grid_pairs_median_ns', 'index_ns_over_2x_grid', 'pairs', 'candidates',
                         'node_visits', 'allocs_per_step'))
        for run, timing in timings.items():
            for scene, n in cases:
                index, grid = (timing[scene, n, engine] for engine in ('index', 'grid'))
                fields = ('estimate_ns', 'err_ns', 'prepare_median_ns', 'pairs_median_ns')
                gap = float(index['estimate_ns']) - 2 * float(grid['estimate_ns'])
                writer.writerow((run, scene, n, *(index[f] for f in fields), *(grid[f] for f in fields),
                                 f'{gap:.1f}', *(index[f] for f in ('pairs', 'candidates', 'node_visits', 'allocs_per_step'))))
    pcs = {pc for _, samples in profiles.values() for _, stack in samples for pc in stack if isinstance(pc, int)}
    frames = symbolize(debug, pcs)
    with (out / 'phase-summary.csv').open('w') as f, (out / 'profile-run-results.csv').open('w') as results:
        writer = csv.writer(f, lineterminator='\n')
        writer.writerow(('run', 'scene', 'n', 'warm_reps', 'samples', 'total_period', 'build_pct_process',
                         'pairs_pct_process', 'other_pct_process', *(p + '_pct_build' for p in BUILD)))
        runs = csv.writer(results, lineterminator='\n')
        runs.writerow(['run', *next(iter(profiles.values()))[0]])
        for name, (row, samples) in profiles.items():
            phases = Counter()
            for period, stack in samples:
                phases[classify(stack, frames)] += period
            build = sum(phases[p] for p in BUILD)
            total = phases.total()
            assert build and phases['pairs'], name
            writer.writerow((name, row['scene'], row['n'], row['reps'], len(samples), total,
                             *(f'{100 * x / total:.2f}' for x in (build, phases['pairs'], phases['other'])),
                             *(f'{100 * phases[p] / build:.2f}' for p in BUILD)))
            runs.writerow([name, *row.values()])
    print('All 14 profile rows match baseline fixture hashes, pair results and traversal counts with zero warm allocations.')


if __name__ == '__main__':
    main(Path(sys.argv[1]), str(Path(sys.argv[2]).resolve()), sys.argv[3])
