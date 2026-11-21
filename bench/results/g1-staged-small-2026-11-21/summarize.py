#!/usr/bin/env python3
"""Compare the two full G1 pairs and check the diagnostic fixture/work IDs."""
import gzip
import math
import re
import runpy
import sys
import tarfile
from collections import Counter
from pathlib import Path
import csv

here = Path(__file__).resolve().parent
out = Path(sys.argv[1])
base = runpy.run_path(str(here.parent/'g2-integer-key-2026-11-19/summarize.py'))
read, key, save = (base[k] for k in ('read_rows','key','save'))
counts = base['COUNTS']
regex = re.compile(r'G1\s+hard snapshot int32 (\S+) n=(\d+) vs brute force \(brute\)\s+(\S+) \[(\S+)-(\S+)\]\s+<= 2 (pass|FAIL|inconclusive)')
gate, compare = [], []
for rounds in (81,117):
    data = {}
    for kind in ('control','candidate'):
        run = f'{rounds}-{kind}'
        d = out/run
        rows = read(d/'snapshot.csv')
        raw = read(d/'snapshot-raw.csv.gz', gzip.open)
        per_key = Counter(map(key,raw))
        round_keys = {(key(r),int(r['round'])) for r in raw}
        assert (d/'run.exit').read_text().strip() == '0'
        data[kind] = {key(r):r for r in rows}
        assert len(rows) == len(data[kind]) == len(per_key) == 96
        for k,r in data[kind].items():
            assert r['status']=='ok' and r['type']=='int32' and r['batch']=='256' and int(r['rounds'])==rounds
            assert per_key[k]==int(r['reps'])==rounds and all((k,i) in round_keys for i in range(rounds))
            if k[-1] in ('index','index-ordered'):
                assert float(r['allocs_per_step'])==0
        seen = Counter()
        for line in (d/'gates.txt').read_text().splitlines():
            if m:=regex.fullmatch(line):
                s,n,p,lo,hi,v=m.groups()
                a,b=(data[kind][s,int(n),e] for e in ('index','brute'))
                # Snapshot per-phase columns are not collected for batch > 1.
                gate.append((run,s,n,*(r[f] for r in (a,b) for f in ('estimate_ns','err_ns','median_ns','p95_ns')),
                             p,lo,hi,v,*(a[f] for f in ('pairs','checksum','candidates','node_visits','allocs_per_step'))))
                seen[v]+=1
        assert seen.total()==16
        print(f'{run}: {len(raw)} samples, 32 warm library rows allocate zero, G1 {seen["pass"]}/{seen["FAIL"]}/{seen["inconclusive"]} (pass/fail/inconclusive)')
    c,t=(data[k] for k in ('control','candidate'))
    assert c.keys()==t.keys()
    for k,a in sorted(c.items()):
        b=t[k]
        assert all(a[f]==b[f] for f in counts), k
        if k[-1] in ('index','index-ordered','brute'):
            before,after=(float(r['estimate_ns']) for r in (a,b))
            error=math.hypot(float(a['err_ns'])/before,float(b['err_ns'])/after)
            ratio=after/before
            lo,hi=ratio*math.exp(-error),ratio*math.exp(error)
            compare.append((rounds,*k,before,after,f'{after-before:.2f}',f'{ratio:.4f}',f'{lo:.4f}',f'{hi:.4f}',
                            'faster' if hi<1 else 'slower' if lo>1 else 'unresolved'))
print('192 matched uncapped engine rows agree on pair counts, checksums, fixtures, work, allocations and memory.')
with tarfile.open(here/'profile/raw-perf.tar.gz') as archive:
    profile_rows=0
    for member in archive.getmembers():
        if re.fullmatch(r'\./[12]-(clustered|uniform)-[0124]-(brute|index)\.csv',member.name):
            r,=csv.DictReader(l.decode() for l in archive.extractfile(member) if not l.startswith(b'#'))
            assert r['status']=='ok'
            assert all(r[f]==c[key(r)][f] for f in ('scene_hash','pairs','checksum','candidates','node_visits','cell_tests'))
            profile_rows+=1
    assert profile_rows==32
print('All 32 diagnostic profile rows match frozen control fixture IDs, pair checksums and traversal work.')
save(out/'g1-comparison.csv','run scene n index_ns index_error_ns index_median_ns index_p95_ns brute_ns brute_error_ns '
     'brute_median_ns brute_p95_ns ratio lower upper verdict pairs checksum candidates node_visits allocs_per_step',gate)
save(out/'step-change.csv','rounds scene n engine control_ns candidate_ns delta_ns ratio lower upper verdict',compare)
