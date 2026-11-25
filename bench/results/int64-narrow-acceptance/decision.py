#!/usr/bin/env python3
"""Produce a promotion recommendation only when all expected artifacts exist."""
import csv
import json
import re
from pathlib import Path

here = Path(__file__).resolve().parent
data = here / 'data'
ref = here.parent / 'packed-staged-2026-11-23' / 'data'
blocks = []

def require(ok, msg):
    if not ok:
        blocks.append(msg)

def rowkey(line):
    # Winner may differ between processes, but gate, scene, size and workload must match.
    return re.sub(r' \([^)]+\)$', '', line.strip())

def gates(file):
    rows = {}
    for line in file.read_text().splitlines():
        if m := re.match(r'^(G\w+)\s+hard (.+?)\s{2,}.* (pass|FAIL|inconclusive)$', line):
            rows[(m[1], rowkey(m[2]))] = m[3]
    return rows

def subjects(file):
    rows = {}
    for line in file.read_text().splitlines():
        if m := re.match(r'^  (G\w+)\s+(snapshot .+?)\s{2,}(accepted-exception|pass|fail|inconclusive)\b', line):
            rows[(m[1], rowkey(m[2]))] = m[3]
    return rows

old_final = subjects(ref / 'final-status.txt')
new_final = subjects(data / 'final-snapshot-status.txt')
for typ in ('int32', 'int64'):
    for mode in ('snapshot', 'persistent'):
        stage = f'{typ}-{mode}'
        old = gates(ref / stage / 'gates.txt')
        new = gates(data / stage / 'gates.txt')
        require(bool(new) and new.keys() == old.keys(), f'{stage}: missing or unexpected gate rows')
        for key, verdict in new.items():
            if mode == 'snapshot':
                verdict = new_final.get(key, verdict)
                if typ == 'int64' and old_final.get(key) in ('fail', 'inconclusive') and key != ('G2', 'snapshot int64 uniform n=100000 vs best baseline'):
                    continue
            require(verdict == 'pass' or (typ == 'int32' and verdict == 'accepted-exception'), f'{key}: {verdict}')

stages = list(csv.DictReader((data / 'run-order.csv').open()))
for s in stages:
    folder = data / s['stage']
    require(s['exit'] == '0' and (folder / 'exit.txt').read_text().split()[0] == '0', f'{folder.name}: benchmark failed')
    require((folder / 'raw.csv.gz').stat().st_size > 0, f'{folder.name}: no samples')
require((data / 'power-after.txt').read_bytes() == (data / 'power-before.txt').read_bytes(), 'power settings not restored')
require('100% tests passed out of 22' in (here / 'ctest.log').read_text(), 'CTest')
ci = json.loads((here / 'ci.json').read_text())
require(ci['head_sha'] == 'a58cff8f04bae51115335cc333e8e3460f36158a' and ci['conclusion'] == 'success'
        and len(ci['jobs']) == 9 and all(j['conclusion'] == 'success' for j in ci['jobs']), 'hosted portability')
print('HOLD' if blocks else 'ELIGIBLE FOR PATCH PROMOTION (not int64 type acceptance)')
for b in blocks: print('-', b)
