#!/usr/bin/env python3
import csv
import gzip
from collections import Counter
from pathlib import Path
from statistics import median
import sys


def rows(path, opener=open):
    with opener(path, 'rt') as f:
        return list(csv.DictReader(line for line in f if not line.startswith('#')))


def key(row):
    return row['scene'], row['engine']


def write_csv(path, fields, records):
    with path.open('w') as f:
        writer = csv.writer(f, lineterminator='\n')
        writer.writerow(fields.split())
        writer.writerows(records)


def main():
    out = Path(sys.argv[1])
    reference = Path(__file__).resolve().parent.parent / 'g1-performance-2026-11-19/153-control/snapshot.csv'
    scenes = ('uniform', 'clustered')
    engines = ('index', 'brute')
    variants = ('control', 'workspace', 'dispatch', 'combined')
    blocks = range(1, 13)
    keys = {(scene, engine) for scene in scenes for engine in engines}
    expected = {key(row): row for row in rows(reference) if row['n'] == '1' and key(row) in keys}
    assert expected.keys() == keys
    counts = 'pairs checksum scene_hash candidates node_visits cell_tests references max_refs'.split()
    measurements = {}
    n_samples = 0
    for block in blocks:
        for variant in variants:
            path = out / f'{block}-{variant}'
            summary = rows(path / 'snapshot.csv')
            assert len(summary) == len(keys) and {key(row) for row in summary} == keys, path
            raw = rows(path / 'snapshot-raw.csv.gz', gzip.open)
            samples = Counter(map(key, raw))
            assert samples.keys() == keys, path
            n_samples += len(raw)
            for row in summary:
                k = key(row)
                assert (row['status'], row['type'], row['n'], row['batch'], row['rounds']) == ('ok', 'int32', '1', '256', '67'), path
                assert all(row[f] == expected[k][f] for f in counts), (path, k)
                assert row['pairs'] == '0' and float(row['allocs_per_step']) == 0, (path, k)
                assert int(row['reps']) == samples[k], (path, k)
                measurements[block, variant, *k] = row

    comparisons = []
    aggregate = []
    for scene in scenes:
        for engine in engines:
            for variant in variants[1:]:
                deltas = []
                for block in blocks:
                    a, b = [measurements[block, kind, scene, engine] for kind in ('control', variant)]
                    delta = float(a['estimate_ns']) - float(b['estimate_ns'])
                    deltas.append(delta)
                    comparisons.append((block, scene, engine, variant, a['estimate_ns'], a['err_ns'],
                                        b['estimate_ns'], b['err_ns'], f'{delta:+.1f}'))
                # For 12 independent blocks, order statistics 3 and 10 bound
                # the population median with 96.1% binomial coverage (no ties).
                deltas.sort()
                central = median(deltas)
                aggregate.append((scene, engine, variant, f'{central:+.2f}', f'{deltas[2]:+.1f}',
                                  f'{deltas[9]:+.1f}', f'{deltas[0]:+.1f}', f'{deltas[-1]:+.1f}'))
    write_csv(out / 'block-change.csv', 'block scene engine variant control_ns control_error_ns variant_ns variant_error_ns saving_ns', comparisons)
    write_csv(out / 'summary.csv', 'scene engine variant median_saving_ns median_ci_low_ns median_ci_high_ns min_saving_ns max_saving_ns', aggregate)
    print(f'{len(measurements)} ok rows ({n_samples} raw samples); all match retained singleton pairs, checksums, '
          'fixture hashes, and counters and have zero warm allocations.')


if __name__ == '__main__':
    main()
