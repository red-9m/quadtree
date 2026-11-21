#!/usr/bin/env python3
"""Attribute exact item periods using the staged twin and audited cross helper PCs."""
import runpy
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
TRAV = runpy.run_path(str(HERE.parent / 'g2-staged-traversal-2026-11-21/summarize.py'))
BASE, P, SAVE = (TRAV[k] for k in ('BASE','P','SAVE'))
GROUPS = ('filter','charge','geometry_mode_setup','closed_endpoints','strict_endpoints','return_other')
# Includes register/address setup assigned to the enclosing operation. Epilogues
# reached from several exits remain unassigned in return_other.
CROSS = ((0x11ca0,0x11cd3,'filter'),(0x11cd9,0x11d02,'charge'),
         (0x11d06,0x11d3e,'geometry_mode_setup'),(0x11d40,0x11d56,'closed_endpoints'),
         (0x11d60,0x11d6e,'filter'),(0x11d78,0x11d7c,'charge'),
         (0x11d80,0x11d91,'geometry_mode_setup'),(0x11d93,0x11dae,'strict_endpoints'))


def part(stack, frames):
    leaf = stack[0] if stack else None
    if isinstance(leaf,int) and 0x11ca0 <= leaf <= 0x11db0:
        return next((g for lo,hi,g in CROSS if lo <= leaf <= hi), 'return_other')
    lib = {n for _,f,n in frames.get(leaf,()) if f == 'quadtree-index-impl.h'}
    if 393 in lib or 394 in lib:
        return 'strict_endpoints'
    if 392 in lib or any(382 <= n <= 385 for n in lib):
        return 'closed_endpoints'
    if any(388 <= n <= 391 for n in lib):
        return 'geometry_mode_setup'
    if any(369 <= n <= 373 or 2777 <= n <= 2780 or 2628 <= n <= 2631 for n in lib):
        return 'filter'
    if any(2588 <= n <= 2603 or n == 2627 for n in lib):
        return 'charge'
    return 'return_other'


def main(cap,out):
    old = HERE.parent / 'g2-staged-traversal-2026-11-21'
    read, key = BASE['rows'], BASE['key']
    assert (cap/'text.sha256').read_text() == (HERE.parent/'g2-bulk-classifier-2026-11-21/text.sha256').read_text()
    elf = (cap/'elf-path.txt').read_text().strip()
    debug = cap/'staged.debug'
    audit=P['sites'](debug)
    expected={(r['block'],r['scene'],r['n'],r['owner']):int(r['period'])
              for r in read(old/'attribution.csv') if r['work']=='exact'}
    pair_totals=Counter()
    for r in read(old/'attribution.csv'):
        pair_totals[r['block'],r['scene'],r['n']]+=int(r['period'])
    ref = {key(r): r for r in read(HERE.parent/'g2-radix-outline-2026-11-21/frozen/9-control/snapshot.csv')}
    records, pcs={},set()
    for r in read(cap/'run-order.csv'):
        if r['event']!='cycles' or r['engine']!='index':
            continue
        k=r['block'], r['scene'],r['n']
        stem=f'{k[0]}-{k[1]}-index-cycles'
        row,=read(cap/f'{stem}.csv')
        assert key(row)==(k[1],k[2],'index') and row['status']=='ok' and float(row['allocs_per_step'])==0
        assert all(row[x]==ref[key(row)][x] for x in BASE['EXACT'])
        events=list(BASE['chain_samples'](cap/f'{stem}.samples.gz',elf))
        records[k]=events
        pcs.update(pc for _,stack in events for pc in stack if isinstance(pc,int))
    assert len(records)==6
    frames=BASE['symbolize'](debug,pcs)
    rows, hotspots=[],[]
    for k,events in records.items():
        cycles,counts,sites=Counter(),Counter(),Counter()
        for period,stack in events:
            if P['phase'](stack,frames,audit)!='pairs':
                continue
            owner, work=TRAV['classify'](stack,frames)
            if work!='exact':
                continue
            g=part(stack,frames)
            cycles[owner,g]+=period
            counts[owner,g]+=1
            sites[owner,g,stack[0]]+=period
        for owner in TRAV['OWNERS']:
            assert sum(cycles[owner,g] for g in GROUPS)==expected[*k,owner]
            for g in GROUPS:
                rows.append((*k,owner,g,counts[owner,g],cycles[owner,g],f'{100*cycles[owner,g]/pair_totals[k]:.3f}'))
        for (owner,g,pc),p in sites.most_common(24):
            label=' | '.join(f'{f}:{n}' for _,f,n in frames.get(pc,()))
            hotspots.append((*k,owner,g,hex(pc) if isinstance(pc,int) else pc,p,label))
    SAVE(out/'exact-periods.csv','block scene n owner phase samples period pct_of_pairs',rows)
    SAVE(out/'hot-pcs.csv','block scene n owner phase pc period inline_locations',hotspots)
    print('Six verified Release captures reconcile exactly with the previous per-origin eligibility periods.')

if __name__=='__main__':
    main(*(Path(arg) for arg in sys.argv[1:]))
