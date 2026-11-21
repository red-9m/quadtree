#!/usr/bin/env python3
"""Report target full-step headroom, with the singleton lane marked invalid."""
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
counts=base['COUNTS']
regex=re.compile(r'G1\s+hard snapshot int32 (\S+) n=(\d+) vs brute force \(brute\)\s+(\S+) \[(\S+)-(\S+)\]\s+<= 2 (pass|FAIL|inconclusive)')
headroom=[]
changes=[]
g1=[]
for probe,n in (('empty',0),('singleton',1)):
    print(f'{probe} ({"invalid diagnostic only" if n else "contract-preserving probe"}):')
    for rounds in (81,117):
        matched={}
        parent=out/probe
        if probe=='singleton' and rounds==117 and (out/'singleton-retry').is_dir():
            parent=out/'singleton-retry'
            assert (out/probe/'aborted-117.txt').exists()
        assert (parent/'power-before.txt').read_bytes()==(parent/'power-after.txt').read_bytes()
        with (parent/'run-order.csv').open() as f:
            launches={row['run']:int(row['start_tctl_millidegrees']) for row in csv.DictReader(f)}
        a,b=(launches[f'{rounds}-{kind}'] for kind in ('control','candidate'))
        assert max(a,b)<=40000 and abs(a-b)<=1000
        for kind in ('control','candidate'):
            d=parent/f'{rounds}-{kind}'
            a=read(d/'snapshot.csv')
            raw=read(d/'snapshot-raw.csv.gz',gzip.open)
            b={key(r):r for r in a}
            rt=Counter((key(r),int(r['round'])) for r in raw)
            assert (d/'run.exit').read_text().strip()=='0' and len(a)==len(b)==96
            assert len(rt)==len(raw)==96*rounds
            for k,r in b.items():
                assert r['status']=='ok' and r['type']=='int32' and r['batch']=='256'
                assert r['rounds']==r['reps']==str(rounds) and all(rt[k,i]==1 for i in range(rounds)), k
                if k[-1] in ('index','index-ordered'):
                    assert float(r['allocs_per_step'])==0
            entries={}
            for line in (d/'gates.txt').read_text().splitlines():
                if m:=regex.fullmatch(line):
                    scene,num,*rest=m.groups()
                    entries[scene,int(num)]=rest
            assert len(entries)==16
            if probe=='empty':
                g1.extend((rounds,kind,scene,num,b[scene,num,'index']['estimate_ns'],b[scene,num,'brute']['estimate_ns'],*entry)
                          for (scene,num),entry in sorted(entries.items()))
            matched[kind]=b,entries
        (control,cgate),(candidate,tgate)=(matched[k] for k in ('control','candidate'))
        assert control.keys()==candidate.keys()
        for k,a in sorted(control.items()):
            b=candidate[k]
            assert all(a[f]==b[f] for f in counts), (probe,rounds,k)
            if k[-1] not in ('index','index-ordered','brute'):
                continue
            old,new=(float(r['estimate_ns']) for r in (a,b))
            h=math.hypot(float(a['err_ns'])/old,float(b['err_ns'])/new)
            factor=new/old
            lo,hi=factor*math.exp(-h),factor*math.exp(h)
            changes.append((probe,rounds,*k,old,new,f'{old-new:.2f}',f'{factor:.4f}',f'{lo:.4f}',f'{hi:.4f}',
                            'faster' if hi<1 else 'slower' if lo>1 else 'unresolved'))
            if k[-2:]!=(n,'index'):
                continue
            scene=k[0]
            brute0,brute1=(float(rs[scene,n,'brute']['estimate_ns']) for rs in (control,candidate))
            first,last=(rs[scene,n] for rs in (cgate,tgate))
            # Indicative remaining gap relative to the independently measured
            # brute in the same process, using the existing log ratio bound.
            # Rounding of the published bound makes this approximate.
            remaining=max(0,new*(1-2/float(last[2])))
            headroom.append((probe,rounds,scene,n,old,new,brute0,brute1,f'{old-new:.2f}',f'{lo:.4f}',f'{hi:.4f}',
                             *first[:3],*last[:3],f'{remaining:.2f}',last[3] if n==0 else 'INVALID'))
        print(f'  {rounds}: {rounds*96} raw samples per process; all 96 matched engine rows agree on work, fixture, checksums and memory; zero warm library allocations.')
save(out/'target-headroom.csv','probe rounds scene n control_ns probe_ns control_brute_ns probe_brute_ns saved_ns ab_lower ab_upper '
     'control_ratio control_lower control_upper probe_ratio probe_lower probe_upper approx_gap_to_upper_pass_ns verdict',headroom)
save(out/'step-change.csv','probe rounds scene n engine control_ns probe_ns saved_ns factor lower upper verdict',changes)
save(out/'empty-probe-g1.csv','rounds engine_variant scene n index_ns brute_ns ratio lower upper verdict',g1)
