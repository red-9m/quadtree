#!/usr/bin/env python3
"""Attribute the audited int32 planBulk instructions and build commit callsites."""
import re
import runpy
import subprocess
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
MEM = runpy.run_path(str(HERE.parent / 'g2-node-locality-2026-11-20/summarize.py'))
BASE = MEM['BASE']
SAVE = runpy.run_path(str(HERE.parent / 'g2-integer-key-2026-11-19/summarize.py'))['save']
# Six emitted loops: lower/upper half for each geometry specialization.
LOOPS = ((0x146960, 0x1469c7), (0x146ab8, 0x146b1e), (0x146ba0, 0x146c2d),
         (0x147170, 0x1471eb), (0x147258, 0x1472e3), (0x147348, 0x1473be))
PREFETCH = ((0x14697e, 0x146995), (0x146ad6, 0x146aeb), (0x146bbf, 0x146bd3),
            (0x147187, 0x14719f), (0x147277, 0x14728a), (0x14735e, 0x147376))
GATHER = ((0x146999, 0x1469a7), (0x146aef, 0x146afe), (0x146bd7, 0x146bea),
          (0x1471a3, 0x1471b8), (0x14728e, 0x1472a0), (0x14737a, 0x14738e))
HIST = {0x146965, 0x146abd, 0x146bb2, 0x147170, 0x1471db, 0x14726a, 0x147348, 0x1473af}
TAGS = {0x14696d, 0x146ac5, 0x146ba7, 0x147178, 0x1471e3, 0x14725f, 0x147350, 0x1473b7}
LOOP_CONTROL = ((0x146971, 0x14697a), (0x146ac9, 0x146ad2), (0x146bbA, 0x146bbd),
                (0x14717e, 0x147183), (0x147272, 0x147275), (0x147355, 0x14735a), (0x1473bb, 0x1473be))
SCATTER = ((0x146e20, 0x146e6a), (0x146e8a, 0x146eae), (0x147440, 0x147471))
BUCKETS = ('gather', 'geometry', 'histogram', 'tag_store', 'prefetch', 'classify_loop',
           'scatter', 'plan_other', 'dense_commit', 'topology_commit', 'build_other')


def in_ranges(pc, ranges):
    return any(a <= pc <= b for a, b in ranges)


def sites(debug):
    cmds = ['objdump', '-d', '-Mintel', '--no-show-raw-insn', '--start-address=0x1466c0', '--stop-address=0x14782a', debug]
    asm = subprocess.check_output(cmds, text=True)
    dest = {}
    for line in asm.splitlines():
        hit = re.match(r'^\s+([\da-f]+):\s+(.+?)\s*$', line)
        if not hit:
            continue
        pc, text = int(hit[1], 16), hit[2].split(' <')[0]
        if in_ranges(pc, SCATTER):
            group = 'scatter'
        elif not in_ranges(pc, LOOPS):
            continue
        elif pc in HIST:
            assert text.startswith('add ') and '[rsp' in text, text
            group = 'histogram'
        elif pc in TAGS:
            assert text.startswith('mov ') and 'BYTE PTR' in text, text
            group = 'tag_store'
        elif in_ranges(pc, PREFETCH):
            group = 'prefetch'
        elif in_ranges(pc, GATHER):
            group = 'gather'
        elif in_ranges(pc, LOOP_CONTROL) or pc in {0x146ba4, 0x146baa, 0x146bae, 0x14725c, 0x147262, 0x147266}:
            group = 'classify_loop'
        else:
            group = 'geometry'
        dest[pc] = group, text
    assert HIST | TAGS <= dest.keys()
    return dest


def phase(stack, frames, audit):
    if stack and stack[0] in audit:
        return audit[stack[0]][0]
    lib = {line for pc in stack for _, file, line in frames.get(pc, []) if file == 'quadtree-index-impl.h'}
    if any(1592 <= line <= 1714 for line in lib):
        return 'plan_other'
    if any(1909 <= line <= 1968 or 2398 <= line <= 2409 for line in lib):
        return 'topology_commit'
    if any(2343 <= line <= 2397 or 2524 <= line <= 2543 for line in lib):
        return 'dense_commit'
    return {'build': 'build_other'}.get(p := MEM['phase'](stack, frames), p)


def main(captures, out):
    read, key = BASE['rows'], BASE['key']
    ref = {key(row): row for row in read(HERE.parent / 'g2-radix-outline-2026-11-21/frozen/9-control/snapshot.csv')}
    expected = ((HERE.parent / 'g9-two-scatter-source-radix-2026-11-21/text.sha256').read_text().splitlines()[1].split())[0]
    assert (captures / 'text.sha256').read_text().split() == [expected, 'release.text', expected, 'debug.text']
    exe, debug = (captures / 'elf-path.txt').read_text().strip(), captures / 'staged.debug'
    audit = sites(debug)
    runs, pcset = [], set()
    for spec in read(captures / 'run-order.csv'):
        b, scene, n, e, event = (spec[k] for k in ('block', 'scene', 'n', 'engine', 'event'))
        name = f'{b}-{scene}-{e}-{event}'
        row, = read(captures / f'{name}.csv')
        assert key(row) == (scene, n, e) and row['status'] == 'ok' and row['type'] == 'int32', name
        assert row['rounds'] == '1' and row['batch'] == '1' and float(row['allocs_per_step']) == 0, name
        assert all(row[k] == ref[key(row)][k] for k in BASE['EXACT']), name
        path = captures / f'{name}.samples.gz'
        records = list((BASE['chain_samples'] if event == 'cycles' else MEM['ibs_samples'])(path, exe))
        pcset.update(pc for *_, stack in records for pc in stack if isinstance(pc, int))
        runs.append((b, scene, n, e, event, records))
    assert len(runs) == 20
    frames = BASE['symbolize'](debug, pcset)
    cycles, memory, pcs = [], [], []
    for b, scene, n, e, event, records in runs:
        k = b, scene, n, e
        counts, periods, by_pc = Counter(), Counter(), Counter()
        for *data, stack in records:
            leaf = stack[0] if stack else None
            group = phase(stack, frames, audit)
            if event == 'cycles':
                period, = data
                periods[group] += period
                counts[group] += 1
                if leaf in audit:
                    by_pc[leaf] += period
            elif leaf in audit:
                op, level, _, weight = data
                counts[group, hex(leaf), op, level] += 1
        if event == 'cycles':
            total = sum(periods[g] for g in BUCKETS)
            assert total and all(periods[g] for g in ('gather','histogram','scatter','topology_commit','dense_commit')), k
            for g in (*BUCKETS, 'pairs', 'other'):
                cycles.append((*k, g, counts[g], periods[g], f'{100*periods[g]/total:.3f}' if g in BUCKETS else ''))
            assert sum(periods.values()) == sum(row[-2] for row in cycles if row[:4] == k)
            for pc, v in sorted(by_pc.items()):
                pcs.append((*k, hex(pc), audit[pc][0], v))
        else:
            memory.extend((*k, *s, c) for s,c in sorted(counts.items()))
    SAVE(out / 'operands.csv', 'pc phase instruction', [(hex(pc), *v) for pc,v in sorted(audit.items())])
    SAVE(out / 'cycle-periods.csv', 'block scene n engine phase samples period pct_of_build', cycles)
    SAVE(out / 'cycle-sites.csv', 'block scene n engine pc phase period', pcs)
    SAVE(out / 'ibs-sites.csv', 'block scene n engine phase pc op level samples', memory)
    print('20 Release captures match frozen fixture hashes, checksums and traversal counts; zero warm allocations.')


if __name__ == '__main__':
    main(*(Path(v) for v in sys.argv[1:]))
