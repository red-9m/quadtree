#!/usr/bin/env python3
"""Verify matched fixed-size workload and analyze the invalid two-entry copy."""
import csv
import gzip
import math
import re
import runpy
import sys
from collections import Counter
from pathlib import Path

here = Path(__file__).resolve().parent
out = Path(sys.argv[1])
base = runpy.run_path(str(here.parent/'g2-integer-key-2026-11-19/summarize.py'))
read, key, save = (base[k] for k in ('read_rows','key','save'))
regex = re.compile(r'G1\s+hard snapshot int32 (\S+) n=2 vs brute force \(brute\)\s+(\S+) \[(\S+)-(\S+)\]\s+<= 2 (pass|FAIL|inconclusive)')
assert (out/'power-before.txt').read_bytes() == (out/'power-after.txt').read_bytes()
with (out/'run-order.csv').open() as f:
    temps = {r['run']:int(r['start_tctl_millidegrees']) for r in csv.DictReader(f)}
summary = []
for rounds in (81,117):
    assert len(temps)==4 and max(temps.values())<=40000
    assert abs(temps[f'{rounds}-control']-temps[f'{rounds}-candidate'])<=1000
    matched = []
    for kind in ('control','candidate'):
        d = out/f'{rounds}-{kind}'
        assert (d/'run.exit').read_text().strip()=='0'
        rows = read(d/'snapshot.csv')
        raw = read(d/'snapshot-raw.csv.gz',gzip.open)
        data = {key(r):r for r in rows}
        reps = Counter((key(r),int(r['round'])) for r in raw)
        assert len(rows)==len(data)==96 and len(reps)==len(raw)==96*rounds
        for k,r in data.items():
            assert r['status']=='ok' and r['type']=='int32' and r['batch']=='256' and r['rounds']==r['reps']==str(rounds)
            assert all(reps[k,i]==1 for i in range(rounds))
            if k[-1] in ('index','index-ordered'):
                assert float(r['allocs_per_step'])==0
        gates = {}
        for line in (d/'gates.txt').read_text().splitlines():
            if m := regex.fullmatch(line):
                scene,*g=m.groups()
                gates[scene]=g
        assert set(gates)=={'uniform','clustered'}
        matched.append((data,gates))
    (a,ac),(b,bc)=matched
    assert a.keys()==b.keys()
    assert all(a[k][f]==b[k][f] for k in a for f in base['COUNTS'])
    print(f'{rounds}: {rounds*96} raw samples per process; 96 matched rows agree on checksums, work and memory, with 32 zero-warm-allocation library rows per process.')
    for scene in ('clustered','uniform'):
        t,u=(float(x[scene,2,'index']['estimate_ns']) for x in (a,b))
        e0,e1=(float(x[scene,2,'index']['err_ns']) for x in (a,b))
        ref0,ref1=(float(x[scene,2,'brute']['estimate_ns']) for x in (a,b))
        h=math.hypot(e0/t,e1/u)
        lo,hi=(u/t*math.exp(sign*h) for sign in (-1,1))
        summary.append((rounds,scene,t,u,ref0,ref1,f'{t-u:.2f}',f'{lo:.4f}',f'{hi:.4f}',
                        'faster' if hi<1 else 'slower' if lo>1 else 'unresolved',*ac[scene],*bc[scene][:3],
                        f'{t-2*ref0:.2f}',f'{u*(2/float(bc[scene][2])-1):.2f}','INVALID'))
save(out/'two-entry-headroom.csv', 'rounds scene control_ns probe_ns control_brute_ns probe_brute_ns saved_ns '
     'ab_lower ab_upper ab_verdict control_ratio control_lower control_upper control_gate synthetic_ratio '
     'synthetic_lower synthetic_upper control_point_gap_ns approx_probe_margin_ns probe_status', summary)
print('The scratch build is invalid outside the fixed-size benchmark history; no candidate gate verdict is claimed.')
