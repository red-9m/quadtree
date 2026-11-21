#!/usr/bin/env python3
import csv
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
p = Path(sys.argv[1])

def rows(name):
    with (p / name).open() as f:
        return list(csv.DictReader(f))

assert (p / 'work.csv').read_bytes() == (HERE.parent / 'g2-cross-admission-census-2026-11-21/work.csv').read_bytes()
outcomes = rows('item-outcomes.csv')
assert len(outcomes) == 24
with (p / 'summary.csv').open('w') as f:
    w = csv.writer(f, lineterminator='\n')
    w.writerow(('scene','n','origin','calls','filter_reject','degenerate_calls','disjoint','contact','positive_extent',
                'reports','disjoint_pct','contact_pct'))
    for work in rows('work.csv'):
        k = work['scene'], work['n']
        for number, owner in enumerate(('self', 'cross', 'holder')):
            items = [x for x in outcomes if (x['scene'],x['n']) == k and int(x['origin']) == number]
            assert len(items) == 2 and [x['degenerate'] for x in items] == ['0', '1']
            for x in items:
                assert int(x['calls']) == sum(int(x[c]) for c in ('filter_reject','x_only','y_only','both','contact','strict'))
                assert int(x['reports']) == int(x['strict']) + (int(x['contact']) if x['degenerate'] == '1' else 0)
            def total(c): return sum(int(x[c]) for x in items)
            calls, rej, contact, strict, reports = (total(c) for c in ('calls','filter_reject','contact','strict','reports'))
            disjoint = sum(total(c) for c in ('x_only','y_only','both'))
            assert calls - rej == int(work[owner+'_tests']) and reports == int(work[owner+'_reports'])
            w.writerow((*k,owner,calls,rej,items[1]['calls'],disjoint,contact,strict,reports,
                        f'{100*disjoint/calls:.3f}',f'{100*contact/calls:.3f}'))
print('Exact item outcomes reconcile by origin with the real callback stream and all frozen work counters.')
