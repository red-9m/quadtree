#!/usr/bin/env python3
import csv
import gzip
import math
from collections import Counter
from pathlib import Path
import re
import sys


def rows(path, opener=open):
    with opener(path, 'rt') as f:
        return list(csv.DictReader(line for line in f if not line.startswith('#')))


def key(row):
    return row['scene'], int(row['n']), row['engine']


def write_csv(path, fields, records):
    with path.open('w') as f:
        writer = csv.writer(f, lineterminator='\n')
        writer.writerow(fields.split())
        writer.writerows(records)


def main():
    out = Path(sys.argv[1])
    scenes = [(scene, n) for scene in ('clustered', 'uniform') for n in (0, 1, 2, 4, 8, 16, 32, 64)]
    engines = ('brute', 'grid', 'sweep', 'qt-loose-sort', 'index', 'index-ordered')
    expected = {(scene, n, engine) for scene, n in scenes for engine in engines}
    reference = Path(__file__).resolve().parent.parent / 'g1-performance-2026-11-19/153-control/snapshot.csv'
    previous = {key(row): row for row in rows(reference)}
    exact = 'pairs checksum scene_hash candidates node_visits cell_tests references max_refs'.split()
    gate_line = re.compile(r'^G1\s+hard snapshot int32 (\w+) n=(\d+) vs brute force \(brute\)\s+([\d.]+) \[([\d.]+)-([\d.]+)\]\s+<= 2 (pass|FAIL|inconclusive)$')
    variants = ('control', 'outlined', 'inline')
    runs = {}
    comparisons = []
    changes = []
    for rounds in (81, 117, 153):
        for variant in variants:
            name = f'{rounds}-{variant}'
            path = out / name
            summary = rows(path / 'snapshot.csv')
            engine_rows = runs[name] = {key(row): row for row in summary}
            assert len(summary) == len(expected) and engine_rows.keys() == expected, path
            assert (path / 'run.exit').read_text().strip() == '0'
            assert (path / 'gates.exit').read_text().strip() == '3'
            raw = rows(path / 'snapshot-raw.csv.gz', gzip.open)
            samples = Counter(map(key, raw))
            assert samples.keys() == expected, path
            for k, row in engine_rows.items():
                assert (row['status'], row['batch'], row['type'], row['rounds']) == ('ok', '256', 'int32', str(rounds)), (name, k)
                assert all(row[f] == previous[k][f] for f in exact), (name, k)
                assert samples[k] == int(row['reps']) >= rounds, (name, k)
                if k[-1] in ('index', 'index-ordered'):
                    assert float(row['allocs_per_step']) == 0, (name, k)
            gates = {}
            for line in (path / 'gates.txt').read_text().splitlines():
                if match := gate_line.fullmatch(line):
                    scene, n, *result = match.groups()
                    gates[scene, int(n)] = result
            assert gates.keys() == set(scenes), path
            verdicts = Counter(row[-1] for row in gates.values())
            print(f'{name}: {len(engine_rows)} ok rows, {len(raw)} raw samples, '
                  f'G1 {verdicts["pass"]}/{verdicts["FAIL"]}/{verdicts["inconclusive"]} (pass/fail/inconclusive)')
            for scene, n in scenes:
                index = engine_rows[scene, n, 'index']
                brute = engine_rows[scene, n, 'brute']
                comparisons.append((name, scene, n, index['estimate_ns'], index['err_ns'],
                                    brute['estimate_ns'], brute['err_ns'], *gates[scene, n]))
        for a, b in (('control', 'outlined'), ('control', 'inline'), ('outlined', 'inline')):
            for scene, n in scenes:
                for engine in ('brute', 'index', 'index-ordered'):
                    x, y = [runs[f'{rounds}-{v}'][scene, n, engine] for v in (a, b)]
                    tx, ty = float(x['estimate_ns']), float(y['estimate_ns'])
                    error = math.hypot(float(x['err_ns']) / tx, float(y['err_ns']) / ty)
                    ratio = ty / tx
                    changes.append((rounds, a, b, scene, n, engine, x['estimate_ns'], y['estimate_ns'],
                                    f'{tx-ty:+.1f}', f'{ratio:.4f}', f'{ratio*math.exp(-error):.4f}',
                                    f'{ratio*math.exp(error):.4f}'))
    write_csv(out / 'g1-comparison.csv', 'run scene n index_ns index_error_ns brute_ns brute_error_ns ratio ci_low ci_high verdict', comparisons)
    write_csv(out / 'index-change.csv', 'rounds base candidate scene n engine base_ns candidate_ns saving_ns ratio ci_low ci_high', changes)
    print('864 engine rows match retained counts, checksums, fixture hashes and traversal counters; 288 library rows have zero warm allocations.')


if __name__ == '__main__':
    main()
