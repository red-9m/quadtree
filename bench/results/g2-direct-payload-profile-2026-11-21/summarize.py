#!/usr/bin/env python3
"""Attribute matched Release profiles; normalize user-cycle periods by warm reps."""
import runpy
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
MEM = runpy.run_path(str(HERE.parent / 'g2-node-locality-2026-11-20/summarize.py'))
BASE = MEM['BASE']
SAVE = runpy.run_path(str(HERE.parent / 'g2-integer-key-2026-11-19/summarize.py'))['save']
DENSE = ('construct', 'lookahead', 'current_gather_call', 'rank_insert', 'dense_other')
BUILD = ('plan', *DENSE, 'topology', 'build_other')
PHASES = (*BUILD, 'pairs', 'other')


def phase(stack, frames, variant):
    leaf = stack[0] if stack else None
    lines = {line for _, file, line in frames.get(leaf, ()) if file == 'quadtree-index-impl.h'}
    chain = [f for pc in stack for f in frames.get(pc, ())]
    lib = {line for _, file, line in chain if file == 'quadtree-index-impl.h'}
    patched = variant == 'candidate'

    def at(a, b, source=lib):
        return any(a <= n <= b for n in source)

    if at(1592, 1714):
        return 'plan'
    topology = (2419, 2430) if patched else (2398, 2409)
    if at(1909, 1968) or at(*topology):
        return 'topology'
    dense = (2352, 2418) if patched else (2343, 2397)
    construct = (2545, 2563) if patched else (2524, 2536)
    if at(*construct, lines):
        return 'construct'
    if at(*dense) or at(*construct):
        if any(file == 'quadtree-index.h' and ('::insertRank(' in name or '::hashInsert(' in name or
                                               '::buildRankTree(' in name) for name, file, _ in chain):
            return 'rank_insert'
        look = (2378, 2396) if patched else (2367, 2380)
        current = (2400, 2408) if patched else (2383, 2387)
        if at(*look, lines):
            return 'lookahead'
        if at(*current, lines):
            return 'current_gather_call'
        return 'dense_other'
    return {'build': 'build_other'}.get(p := MEM['phase'](stack, frames), p)


def main(cap, out):
    read, key = BASE['rows'], BASE['key']
    ref = {key(r): r for r in read(HERE.parent / 'g2-direct-payload-2026-11-21/pilot/1-control/snapshot.csv')}
    rows = list(read(cap / 'run-order.csv'))
    runs, pcs = [], {'control': set(), 'candidate': set()}
    for case in rows:
        block, kind, scene, engine, event = (case[f] for f in ('block', 'kind', 'scene', 'engine', 'event'))
        stem = f'{block}-{kind}-{scene}-{engine}-{event}'
        row, = read(cap / f'{stem}.csv')
        assert key(row) == (scene, '100000', engine)
        assert row['rounds'] == '1' and row['batch'] == '1' and row['status'] == 'ok' and float(row['allocs_per_step']) == 0
        assert all(row[f] == ref[key(row)][f] for f in BASE['EXACT']), stem
        load = BASE['chain_samples'] if event == 'cycles' else MEM['ibs_samples']
        samples = list(load(cap / f'{stem}.samples.gz', (cap / f'{kind}.elf-path.txt').read_text().strip()))
        pcs[kind].update(pc for *_, stack in samples for pc in stack if isinstance(pc, int))
        runs.append(((block, kind, scene, engine), event, row, samples))
    assert len(runs) == 16
    symbols = {kind: BASE['symbolize'](cap / f'{kind}.debug', sites) for kind, sites in pcs.items()}
    cycles, counts, hot, comps = [], [], [], {}
    for k, ev, row, records in runs:
        _, kind, _, _ = k
        frames = symbols[kind]
        reps = int(row['reps'])
        totals, sites = Counter(), Counter()
        for *data, stack in records:
            group = phase(stack, frames, kind)
            assert group in PHASES
            pc = stack[0] if stack else None
            if ev == 'cycles':
                totals[group] += data[0]
                if group in BUILD:
                    sites[group, pc] += data[0]
            elif group in DENSE:
                op, level, _, _ = data
                sites[group, pc, op, level] += 1
        if ev == 'cycles':
            build, dense = (sum(totals[g] for g in keys) for keys in (BUILD, DENSE))
            assert all(totals[g] for g in ('plan', 'construct', 'topology', 'pairs')) and reps > 20, k
            cycles.extend((*k, reps, g, totals[g], f'{totals[g]/reps:.1f}') for g in PHASES)
            comps[k] = dict(totals, build=build, dense=dense, reps=reps)
            for (g, pc), p in sites.most_common(20):
                location = ' | '.join(f'{file}:{line}' for _, file, line in frames.get(pc, ()))
                hot.append((*k, g, hex(pc) if isinstance(pc, int) else pc, p, location))
        else:
            counts.extend((*k, g, hex(pc) if isinstance(pc, int) else pc, op, level, n)
                          for (g, pc, op, level), n in sites.most_common())
    changes = []
    for block, kind, scene, engine in comps:
        if kind != 'control':
            continue
        a, b = (comps[block, v, scene, engine] for v in ('control', 'candidate'))
        for group in ('build', 'dense', *PHASES):
            x, y = (s.get(group, 0)/s['reps'] for s in (a, b))
            changes.append((block, scene, engine, group, f'{x:.1f}', f'{y:.1f}', f'{y-x:.1f}'))
    SAVE(out / 'cycle-periods.csv', 'block kind scene engine reps phase period cycles_per_warm_rep', cycles)
    SAVE(out / 'cycle-change.csv', 'block scene engine phase control_cycles_per_rep candidate_cycles_per_rep delta', changes)
    SAVE(out / 'hot-pcs.csv', 'block kind scene engine phase pc period inline_locations', hot)
    SAVE(out / 'ibs-dense.csv', 'block kind scene engine phase pc op level samples', counts)
    print('16 captures match exact pilot fixtures, pair results and work with zero warm allocations.')


if __name__ == '__main__':
    main(*(Path(arg) for arg in sys.argv[1:]))
