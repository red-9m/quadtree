#!/usr/bin/env python3
import re
import runpy
import sys
from collections import Counter
from pathlib import Path

out = Path(sys.argv[1])
old = runpy.run_path(str(Path(__file__).resolve().parent.parent / 'g2-integer-key-2026-11-19/summarize.py'))
old['main'](out)
gate9 = re.compile(r'G9\s+hard snapshot int32 (\S+) n=(\d+) vs unordered \(index\)\s+'
                   r'(\S+) \[(\S+)-(\S+)\]\s+<= (\d+) (pass|FAIL|inconclusive)')
phases = ('estimate_ns', 'err_ns', 'prepare_median_ns', 'pairs_median_ns')
comparisons = []
gate_counts = []
for rounds in (9, 15):
    for kind in ('control', 'candidate'):
        run = f'{rounds}-{kind}'
        path = out / run
        rows = {old['key'](r): r for r in old['read_rows'](path / 'snapshot.csv')}
        lines = (path / 'gates.txt').read_text().splitlines()
        counts = Counter()
        for line in lines:
            if m := gate9.fullmatch(line):
                scene, n, ratio, low, high, limit, verdict = m.groups()
                a, b = (rows[scene, int(n), engine] for engine in ('index-ordered', 'index'))
                comparisons.append((run, scene, n, *(a[k] for k in phases), *(b[k] for k in phases),
                                    ratio, low, high, limit, verdict))
                counts[verdict] += 1
            if m := re.fullmatch(r'  (G\d+) hard\s+(\d+)\s+(\d+)\s+(\d+)', line):
                gate_counts.append((run, *m.groups()))
        assert sum(counts.values()) == 15, run
        print(f'{run}: G9 {counts["pass"]}/{counts["FAIL"]}/{counts["inconclusive"]} (pass/fail/inconclusive)')
old['save'](out / 'g9-comparison.csv', 'run scene n ordered_ns ordered_error_ns ordered_build_ns ordered_pairs_ns '
            'unordered_ns unordered_error_ns unordered_build_ns unordered_pairs_ns ratio ci_low ci_high limit verdict',
            comparisons)
old['save'](out / 'gate-counts.csv', 'run gate pass fail inconclusive', gate_counts)
