#!/usr/bin/env python3
"""Verify the frozen slice, allowing only the deliberate extra cold allocation."""
import gzip
import math
import re
import runpy
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
BASE = runpy.run_path(str(HERE.parent / 'g2-integer-key-2026-11-19/summarize.py'))
G9 = re.compile(r'^G9\s+hard snapshot int32 (\S+) n=(\d+) vs unordered \(index\)\s+'
                r'(\S+) \[(\S+)-(\S+)\]\s+<= (\d+) (pass|FAIL|inconclusive)$')
PHASES = ('estimate_ns', 'err_ns', 'prepare_median_ns', 'pairs_median_ns')
LIB = ('index', 'index-ordered')


def main(out):
    read, key, save = (BASE[n] for n in ('read_rows', 'key', 'save'))
    gates, g2, g9, steps, allocations = [], [], [], [], []
    for rounds in (9, 15):
        runs = {}
        for kind in ('control', 'candidate'):
            name = f'{rounds}-{kind}'
            path = out / name
            rows = read(path / 'snapshot.csv')
            raw = read(path / 'snapshot-raw.csv.gz', gzip.open)
            data = runs[kind] = {key(r): r for r in rows}
            assert len(rows) == len(data) == 204, name
            assert (path / 'run.exit').read_text().strip() == '0', name
            assert (path / 'gates.exit').read_text().strip() in ('0', '3', '4'), name
            assert all(r['type'] == 'int32' and r['status'] in ('ok', 'capped') for r in rows), name
            samples = Counter(map(key, raw))
            assert not (samples.keys() - data.keys()), name
            assert all(samples[key(r)] == int(r['reps']) for r in rows), name
            lib = [r for r in rows if r['engine'] in LIB]
            assert len(lib) == 68 and all(r['status'] == 'ok' and float(r['allocs_per_step']) == 0 for r in lib), name
            counts = Counter()
            for line in (path / 'gates.txt').read_text().splitlines():
                if m := BASE['GATE'].fullmatch(line):
                    scene, n, baseline, ratio, low, high, limit, verdict = m.groups()
                    a, b = (data[scene, int(n), e] for e in ('index', baseline))
                    g2.append((name, scene, n, baseline, *(a[f] for f in PHASES), *(b[f] for f in PHASES),
                               ratio, low, high, limit, verdict, *(a[f] for f in ('pairs', 'candidates', 'node_visits'))))
                    counts['G2'] += 1
                if m := G9.fullmatch(line):
                    scene, n, ratio, low, high, limit, verdict = m.groups()
                    a, b = (data[scene, int(n), e] for e in ('index-ordered', 'index'))
                    g9.append((name, scene, n, *(a[f] for f in PHASES), *(b[f] for f in PHASES),
                               ratio, low, high, limit, verdict))
                    counts['G9'] += 1
                if m := re.fullmatch(r'  (G\d+) hard\s+(\d+)\s+(\d+)\s+(\d+)', line):
                    gates.append((name, *m.groups()))
            assert counts == {'G2': 15, 'G9': 15}, name
            print(f'{name}: {len(raw)} raw samples; 68 completed library rows with zero warm allocations.')
        control, candidate = (runs[kind] for kind in ('control', 'candidate'))
        assert control.keys() == candidate.keys()
        for k, a in sorted(control.items()):
            b = candidate[k]
            assert a['status'] == b['status'] and a['scene_hash'] == b['scene_hash'], (rounds, k)
            if a['status'] != 'ok':
                continue
            expected_delta = int(k[2] in LIB and k[1] != 0)
            assert int(b['cold_allocs']) - int(a['cold_allocs']) == expected_delta, (rounds, k)
            assert all(a[f] == b[f] for f in BASE['COUNTS'] if f != 'cold_allocs'), (rounds, k)
            if k[2] in LIB:
                allocations.append((rounds, *k, a['allocs_per_step'], b['allocs_per_step'], a['cold_allocs'],
                                    b['cold_allocs'], a['peak_bytes'], b['peak_bytes'], a['retained_bytes'], b['retained_bytes']))
            x, y = (float(r['estimate_ns']) for r in (a, b))
            ratio = y / x
            half = math.hypot(float(a['err_ns']) / x, float(b['err_ns']) / y)
            steps.append((rounds, *k, a['estimate_ns'], b['estimate_ns'], f'{ratio:.4f}',
                          f'{ratio * math.exp(-half):.4f}', f'{ratio * math.exp(half):.4f}',
                          a['prepare_median_ns'], b['prepare_median_ns'], a['pairs_median_ns'], b['pairs_median_ns']))
    print('All matched uncapped rows agree on fixtures, pairs, checksums, work, warm allocations, peak and retained bytes.')
    print('Exactly one extra cold allocation for each nonempty library row; no change for any other row.')
    save(out / 'gate-counts.csv', 'run gate pass fail inconclusive', gates)
    save(out / 'g2-comparison.csv', 'run scene n baseline index_ns index_error_ns index_build_ns index_pairs_ns '
         'baseline_ns baseline_error_ns baseline_build_ns baseline_pairs_ns ratio ci_low ci_high limit verdict '
         'pairs candidates node_visits', g2)
    save(out / 'g9-comparison.csv', 'run scene n ordered_ns ordered_error_ns ordered_build_ns ordered_pairs_ns '
         'unordered_ns unordered_error_ns unordered_build_ns unordered_pairs_ns ratio ci_low ci_high limit verdict', g9)
    save(out / 'step-change.csv', 'rounds scene n engine control_ns candidate_ns candidate_over_control ci_low ci_high '
         'control_build_ns candidate_build_ns control_pairs_ns candidate_pairs_ns', steps)
    save(out / 'allocations.csv', 'rounds scene n engine control_warm candidate_warm control_cold candidate_cold '
         'control_peak_bytes candidate_peak_bytes control_retained_bytes candidate_retained_bytes', allocations)


if __name__ == '__main__':
    main(Path(sys.argv[1]))
