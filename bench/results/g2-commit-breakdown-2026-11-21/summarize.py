#!/usr/bin/env python3
"""Split existing dense/topology callchain periods without collecting new timings."""
import runpy
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
P = runpy.run_path(str(HERE.parent / 'g2-bulk-classifier-2026-11-21/summarize.py'))
BASE, MEM, SAVE = (P[k] for k in ('BASE', 'MEM', 'SAVE'))
DENSE = ('payload_block', 'box_block', 'meta_block', 'slot_block', 'construct_other',
         'lookahead', 'gather_call', 'rank_insert', 'publish_and_loop')
TOPO = ('own_range', 'node_init', 'refit', 'topology_other')
# Excerpts of the hash-checked constructBuildEntry<int32> body. Inclusive PC ranges.
ENTRY = ((0x147830, 0x147840, 'payload_block'), (0x147844, 0x147857, 'box_block'),
         (0x14785a, 0x14787d, 'meta_block'), (0x147880, 0x14788b, 'slot_block'),
         (0x14788f, 0x14788f, 'construct_other'))


def split(stack, frames, parent):
    leaf = stack[0] if stack else None
    chain = [f for pc in stack for f in frames.get(pc, [])]
    lib = {line for _, file, line in chain if file == 'quadtree-index-impl.h'}
    leaf_lines = {line for _, file, line in frames.get(leaf, []) if file == 'quadtree-index-impl.h'}

    def at(lines, a, b):
        return any(a <= x <= b for x in lines)

    if parent == 'dense_commit':
        if isinstance(leaf, int):
            for lo, hi, group in ENTRY:
                if lo <= leaf <= hi:
                    return group
        if any(file == 'quadtree-index.h' and ('::insertRank(' in name or '::hashInsert(' in name or
                                               '::buildRankTree(' in name) for name, file, _ in chain):
            return 'rank_insert'
        if at(leaf_lines, 2524, 2536):
            return 'construct_other'
        if at(leaf_lines, 2367, 2380):
            return 'lookahead'
        if at(leaf_lines, 2383, 2387):
            return 'gather_call'
        return 'publish_and_loop'
    if parent == 'topology_commit':
        if at(lib, 1888, 1907):
            return 'own_range'
        if at(lib, 1835, 1877):
            return 'refit'
        if at(lib, 1722, 1736) or at(lib, 1919, 1927) or at(lib, 2400, 2404):
            return 'node_init'
        return 'topology_other'
    return None


def main(cap, out):
    read, key = BASE['rows'], BASE['key']
    profile = HERE.parent / 'g2-bulk-classifier-2026-11-21'
    expected = (profile / 'text.sha256').read_text()
    assert (cap / 'text.sha256').read_text() == expected
    audit = P['sites'](cap / 'staged.debug')
    elf = (cap / 'elf-path.txt').read_text().strip()
    ref = {key(r): r for r in read(HERE.parent / 'g2-radix-outline-2026-11-21/frozen/9-control/snapshot.csv')}
    recorded = {(r['block'], r['scene'], r['n'], r['engine'], r['phase']): int(r['period'])
                for r in read(profile / 'cycle-periods.csv')}
    runs, all_pcs = [], set()
    for row in read(cap / 'run-order.csv'):
        b, scene, n, e, ev = (row[k] for k in ('block', 'scene', 'n', 'engine', 'event'))
        stem = f'{b}-{scene}-{e}-{ev}'
        result, = read(cap / f'{stem}.csv')
        assert key(result) == (scene, n, e) and result['status'] == 'ok'
        assert float(result['allocs_per_step']) == 0 and all(result[k] == ref[key(result)][k] for k in BASE['EXACT'])
        loader = BASE['chain_samples'] if ev == 'cycles' else MEM['ibs_samples']
        records = list(loader(cap / f'{stem}.samples.gz', elf))
        runs.append(((b, scene, n, e), ev, records))
        all_pcs.update(pc for *_, stack in records for pc in stack if isinstance(pc, int))
    assert len(runs) == 20
    frames = BASE['symbolize'](cap / 'staged.debug', all_pcs)
    shares, locations, memory = [], [], []
    for k, ev, records in runs:
        groups, counts, leaves = Counter(), Counter(), Counter()
        for *data, stack in records:
            parent = P['phase'](stack, frames, audit)
            group = split(stack, frames, parent)
            if group is None:
                continue
            pc = stack[0] if stack else None
            if ev == 'cycles':
                groups[group] += data[0]
                counts[group] += 1
                leaves[group, pc] += data[0]
            else:
                op, level, _, _ = data
                counts[group, pc, op, level] += 1
        if ev == 'cycles':
            for parent, parts in (('dense_commit', DENSE), ('topology_commit', TOPO)):
                assert sum(groups[p] for p in parts) == recorded[(*k, parent)], (k, parent)
            build = sum(recorded[(*k, parent)] for parent in P['BUCKETS'])
            shares.extend((*k, group, counts[group], groups[group], f'{100*groups[group]/build:.3f}')
                          for group in (*DENSE, *TOPO))
            total = sum(groups.values())
            for (group, pc), value in leaves.most_common(16):
                label = ' | '.join(f'{file}:{line}' for _, file, line in frames.get(pc, []))
                locations.append((*k, group, hex(pc) if isinstance(pc, int) else pc, value, f'{100*value/total:.3f}', label))
        else:
            for (group, pc, op, level), value in counts.most_common():
                memory.append((*k, group, hex(pc) if isinstance(pc, int) else pc, op, level, value))
    SAVE(out / 'commit-periods.csv', 'block scene n engine phase samples period pct_of_build', shares)
    SAVE(out / 'hot-pcs.csv', 'block scene n engine phase pc period pct_of_combined_commit inline_locations', locations)
    SAVE(out / 'ibs-commit-sites.csv', 'block scene n engine phase pc op level samples', memory)
    print('20 archived captures rechecked against frozen fixtures, checksums and work; zero warm allocations.')
    print('All 12 cycle profiles reconcile exactly with the prior dense/topology totals.')


if __name__ == '__main__':
    main(*(Path(v) for v in sys.argv[1:]))
