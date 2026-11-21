#!/usr/bin/env python3
"""Verify the fresh 45-round candidate against the exact prior frozen work."""
import gzip
import re
import runpy
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
base = runpy.run_path(str(HERE.parent/'g2-integer-key-2026-11-19/summarize.py'))
read, key, save = (base[k] for k in ('read_rows','key','save'))
out = Path(sys.argv[1])
record = out/'45-candidate'
old = HERE.parent/'g2-packed-cross-item-2026-11-21/frozen/15-candidate'
rows = read(record/'snapshot.csv')
data = {key(row):row for row in rows}
ref = {key(row):row for row in read(old/'snapshot.csv')}
raw = read(record/'snapshot-raw.csv.gz', gzip.open)
counts=Counter(map(key,raw))
assert len(rows) == len(data) == len(ref) == 204 and data.keys() == ref.keys()
assert (record/'run.exit').read_text().strip() == '0'
assert (record/'gates.exit').read_text().strip() in ('0','3','4')
assert counts.keys() <= data.keys()
rt = {(key(r),int(r['round'])) for r in raw}
for k,row in data.items():
    prior = ref[k]
    assert row['type']=='int32' and row['status']==prior['status'] in ('ok','capped'), k
    assert row['scene_hash']==prior['scene_hash'] and counts[k]==int(row['reps']), k
    if row['status']=='ok':
        assert row['rounds']=='45' and all((k,i) in rt for i in range(45)), k
        assert all(row[f]==prior[f] for f in base['COUNTS']), k
lib=[r for r in rows if r['engine'] in ('index','index-ordered')]
assert len(lib)==68 and all(r['status']=='ok' and float(r['allocs_per_step'])==0 for r in lib)
phases=('estimate_ns','err_ns','prepare_median_ns','pairs_median_ns')
g9=re.compile(r'G9\s+hard snapshot int32 (\S+) n=(\d+) vs unordered \(index\)\s+'
              r'(\S+) \[(\S+)-(\S+)\]\s+<= (\d+) (pass|FAIL|inconclusive)')
gates, g2_rows, g9_rows = [], [], []
for line in (record/'gates.txt').read_text().splitlines():
    if m:=re.fullmatch(r'  (G\d+) hard\s+(\d+)\s+(\d+)\s+(\d+)',line):
        gates.append(m.groups())
    if m:=base['GATE'].fullmatch(line):
        s,n,e,ratio,lo,hi,limit,verdict=m.groups()
        a,b=(data[s,int(n),engine] for engine in ('index',e))
        g2_rows.append((s,n,e,*(a[f] for f in phases),*(b[f] for f in phases),ratio,lo,hi,limit,verdict,
                        *(a[k] for k in ('pairs','node_visits','candidates','allocs_per_step'))))
    if m:=g9.fullmatch(line):
        s,n,ratio,lo,hi,limit,verdict=m.groups()
        a,b=(data[s,int(n),engine] for engine in ('index-ordered','index'))
        g9_rows.append((s,n,*(a[f] for f in phases),*(b[f] for f in phases),ratio,lo,hi,limit,verdict))
assert len(g2_rows)==len(g9_rows)==15
save(out/'gate-counts.csv', 'gate pass fail inconclusive', gates)
save(out/'g2-comparison.csv', 'scene n baseline index_ns index_error_ns index_build_ns index_pairs_ns '
     'baseline_ns baseline_error_ns baseline_build_ns baseline_pairs_ns ratio ci_low ci_high limit verdict '
     'pairs node_visits candidates allocs_per_step', g2_rows)
save(out/'g9-comparison.csv', 'scene n ordered_ns ordered_error_ns ordered_build_ns ordered_pairs_ns '
     'unordered_ns unordered_error_ns unordered_build_ns unordered_pairs_ns ratio ci_low ci_high limit verdict', g9_rows)
print(f'{len(raw)} raw samples; all 204 fixture/status rows and uncapped checksum/work/memory fields match prior frozen candidate.')
print('All 68 warm library rows allocate zero; runtime benchmark and frozen evaluator complete.')
for gate,p,f,i in gates:
    print(f'{gate}: {p}/{f}/{i} (pass/fail/inconclusive).')
