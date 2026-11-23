#!/usr/bin/env python3
"""Apply resolution-rule.md: pool the two 45-round processes for every hard snapshot row that was
inconclusive in the 5-round matrix; report final status per type with the 08 exceptions (int32 only)."""
import math
import re
import sys
from collections import Counter
from pathlib import Path

data = Path(sys.argv[1])
LINE = re.compile(r'^(G\d+[a-z]?)\s+(hard|soft)\s+(.*?)\s{2,}(\S+)(?: \[(\S+)-(\S+)\])?\s+(<=|x/)\s*(\S+) (pass|FAIL|inconclusive)$')
G2_EXC = re.compile(r'^snapshot int32 (split-line-translated n=100|uniform n=1000) vs best baseline')
G1_SMALL = re.compile(r'^snapshot (\S+) (uniform|clustered) n=([0-4]) vs brute force')

def gates(path):
    rows = {}
    for line in path.read_text().splitlines():
        if (m := LINE.match(line)) and m[2] == 'hard':
            rows[m[1], m[3]] = (float(m[4]), float(m[5]) if m[5] else None, float(m[6]) if m[6] else None, float(m[8]), m[9])
    return rows

for t in ('int32', 'int64', 'float', 'double'):
    matrix = gates(data / f'candidate-{t}' / 'gates.txt')
    reruns = [gates(data / f'candidate-{t}-{s}' / 'gates.txt') for s in ('resolve', 'resolve-2')]
    final = Counter()
    print(f'== {t}')
    for key, (v, lo, hi, limit, verdict) in sorted(matrix.items()):
        gate, subject = key
        status, upper, note = {'pass': 'pass', 'FAIL': 'fail', 'inconclusive': 'inconclusive'}[verdict], hi, ''
        if verdict == 'inconclusive':
            got = [r[key] for r in reruns if key in r]
            assert len(got) == 2, (t, key)
            logs = [math.log(g[0]) for g in got]
            hs = [(math.log(g[2]) - math.log(g[1])) / 2 for g in got]
            m, h = sum(logs) / 2, math.sqrt(sum(x * x for x in hs)) / 2
            plo, phi = math.exp(m - h), math.exp(m + h)
            status = 'pass' if phi <= limit else 'fail' if plo > limit else 'inconclusive'
            upper = phi
            note = (f' pooled {math.exp(m):.3f} [{plo:.3f}-{phi:.3f}] from ' +
                    ', '.join(f'{g[0]:.3f} [{g[1]:.3f}-{g[2]:.3f}]' for g in got))
        if status != 'pass':
            if t == 'int32' and gate == 'G1' and G1_SMALL.match(subject) and upper is not None and upper <= 3.5:
                status = 'accepted-exception'
            elif t == 'int32' and gate == 'G2' and G2_EXC.match(subject) and upper is not None and upper <= 4.5:
                status = 'accepted-exception'
        final[gate, status] += 1
        if status != 'pass' or note:
            print(f'  {gate:3} {subject[:70]:70} {status:18}{note or f" {v:.3f} [{lo}-{hi}]"}')
    print('  summary:', ', '.join(f'{g} {s} {n}' for (g, s), n in sorted(final.items())))
