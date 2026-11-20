#!/usr/bin/env python3
import csv
import gzip
import math
import re
import sys
from collections import Counter
from pathlib import Path


COUNTS = ('pairs', 'checksum', 'scene_hash', 'candidates', 'node_visits', 'cell_tests',
          'references', 'max_refs', 'allocs_per_step', 'cold_allocs', 'peak_bytes', 'retained_bytes')
GATE = re.compile(r'^G2\s+hard snapshot int32 (\S+) n=(\d+) vs best baseline \((\S+)\)\s+'
                  r'(\S+) \[(\S+)-(\S+)\]\s+<= (\d+) (pass|FAIL|inconclusive)$')


def read_rows(path, opener=open):
    with opener(path, 'rt') as f:
        return list(csv.DictReader(line for line in f if not line.startswith('#')))


def key(row):
    return row['scene'], int(row['n']), row['engine']


def save(path, header, rows):
    with path.open('w') as f:
        writer = csv.writer(f, lineterminator='\n')
        writer.writerow(header.split())
        writer.writerows(rows)


def main(out):
    comparisons, changes = [], []
    for rounds in (9, 15):
        runs = {}
        for kind in ('control', 'candidate'):
            name = f'{rounds}-{kind}'
            path = out / name
            rows = read_rows(path / 'snapshot.csv')
            raw = read_rows(path / 'snapshot-raw.csv.gz', gzip.open)
            data = runs[kind] = {key(row): row for row in rows}
            assert len(rows) == len(data) == 204, name
            assert (path / 'run.exit').read_text().strip() == '0', name
            assert (path / 'gates.exit').read_text().strip() in ('0', '3', '4'), name
            assert all(row['status'] in ('ok', 'capped') and row['type'] == 'int32' for row in rows), name
            samples = Counter(map(key, raw))
            assert not (samples.keys() - data.keys()), name
            assert all(samples[key(row)] == int(row['reps']) for row in rows), name
            library = [row for row in rows if row['engine'] in ('index', 'index-ordered')]
            assert len(library) == 68 and all(row['status'] == 'ok' and float(row['allocs_per_step']) == 0
                                              for row in library), name
            gates = []
            text = (path / 'gates.txt').read_text()
            for line in text.splitlines():
                if match := GATE.fullmatch(line):
                    scene, n, engine, ratio, low, high, limit, verdict = match.groups()
                    n = int(n)
                    index, base = (data[scene, n, e] for e in ('index', engine))
                    phases = ('estimate_ns', 'err_ns', 'prepare_median_ns', 'pairs_median_ns')
                    comparisons.append((name, scene, n, engine, *(index[k] for k in phases),
                                        *(base[k] for k in phases), ratio, low, high, limit, verdict,
                                        *(index[k] for k in ('pairs', 'candidates', 'node_visits', 'allocs_per_step'))))
                    gates.append(verdict)
            assert len(gates) == 15, name
            print(f'{name}: {len(raw)} raw samples; 68 ok library rows with zero warm allocations; '
                  f'G2 {gates.count("pass")}/{gates.count("FAIL")}/{gates.count("inconclusive")} (pass/fail/inconclusive)')
        control, candidate = (runs[kind] for kind in ('control', 'candidate'))
        assert control.keys() == candidate.keys()
        for k, a in sorted(control.items()):
            b = candidate[k]
            assert a['status'] == b['status'] and a['scene_hash'] == b['scene_hash'], (rounds, k)
            if a['status'] != 'ok':
                continue
            assert all(a[field] == b[field] for field in COUNTS), (rounds, k)
            x, y = float(a['estimate_ns']), float(b['estimate_ns'])
            ratio = y / x
            half = math.hypot(float(a['err_ns']) / x, float(b['err_ns']) / y)
            changes.append((rounds, *k, a['estimate_ns'], b['estimate_ns'], f'{ratio:.4f}',
                            f'{ratio*math.exp(-half):.4f}', f'{ratio*math.exp(half):.4f}',
                            a['prepare_median_ns'], b['prepare_median_ns'],
                            a['pairs_median_ns'], b['pairs_median_ns']))
    print('All matched uncapped rows agree on pair counts, checksums, fixtures, work, allocations and memory.')
    save(out / 'g2-comparison.csv', 'run scene n baseline index_ns index_error_ns index_build_ns index_pairs_ns '
         'baseline_ns baseline_error_ns baseline_build_ns baseline_pairs_ns ratio ci_low ci_high limit verdict '
         'pairs candidates node_visits allocs_per_step', comparisons)
    save(out / 'step-change.csv', 'rounds scene n engine control_ns candidate_ns candidate_over_control ci_low ci_high '
         'control_build_ns candidate_build_ns control_pairs_ns candidate_pairs_ns', changes)


if __name__ == '__main__':
    main(Path(sys.argv[1]))
