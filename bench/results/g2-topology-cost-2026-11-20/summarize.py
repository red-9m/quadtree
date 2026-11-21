#!/usr/bin/env python3
"""Split topology work in the staged Release ELF, using its matched debug twin."""
import runpy
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
LOADER = runpy.run_path(str(HERE.parent / 'g2-staged-traffic-2026-11-20/summarize.py'))
BASE, PROF, MEM = (LOADER[k] for k in ('BASE', 'PROF', 'MEM'))
read, key, save, percent = (LOADER[k] for k in ('read', 'key', 'save', 'percent'))
IMPL = LOADER['IMPL']
BINS = ('node_init', 'dense_refs', 'own_summary', 'own_loop_other', 'subtree_refit', 'topology_other')
SYMBOLS = {'refit': (0x12f5e0, 0x12fa3a), 'own': (0x149da0, 0x14a40d), 'materialize': (0x14a410, 0x14aa14)}


def in_symbol(pc, fn):
    lo, hi = SYMBOLS[fn]
    return isinstance(pc, int) and lo <= pc < hi


def topology(stack, frames, sites):
    if not stack:
        return None
    pc = stack[0]
    if in_symbol(pc, 'refit'):
        return 'subtree_refit'  # Profiled snapshot build only; no mutation refits.
    if pc in sites:
        return sites[pc]
    if in_symbol(pc, 'own'):
        chain = frames[pc]
        if any(file == IMPL and line == 1867 for _, file, line in chain):
            return 'dense_refs'
        if any(file == IMPL and (line == 1863 or 1868 <= line <= 1876) for _, file, line in chain):
            return 'own_summary'
        return 'own_loop_other'
    if in_symbol(pc, 'materialize'):
        if any(file == IMPL and 1894 <= line <= 1898 for _, file, line in frames[pc]):
            return 'node_init'
        return 'topology_other'
    if LOADER['build_phase'](stack, frames) == 'topology_commit':
        return 'topology_other'  # Root setup, split constructor, etc.
    return None


def main(captures, debug, out):
    release = (captures / 'elf-path.txt').read_text().strip()
    text = PROF['TEXT']
    assert (captures / 'text.sha256').read_text().split() == [text, 'release.text', text, 'debug.text']
    ref = {key(r): r for r in read(LOADER['GATE'] / '15-control/snapshot.csv')}
    site_phase, operands = {}, {}
    for row in read(HERE / 'store-sites.csv'):
        for addr in row['pcs'].split():
            pc = int(addr, 16)
            assert pc not in site_phase
            operands[pc] = row['operand']
            site_phase[pc] = row['bucket']
    records, pcs = {}, set()
    for spec in read(captures / 'run-order.csv'):
        block, scene, n, engine, event = (spec[f] for f in ('block', 'scene', 'n', 'engine', 'event'))
        run = f'{block}-{scene}-{engine}-{event}'
        row, = read(captures / f'{run}.csv')
        t = key(row)
        assert t == (scene, n, engine) and n == '100000' and row['type'] == 'int32', run
        assert (row['status'], row['batch'], row['rounds']) == ('ok', '1', '1') and int(row['reps']) >= 15, run
        assert float(row['allocs_per_step']) == 0, run
        assert all(row[f] == ref[t][f] for f in (*BASE['EXACT'], 'peak_bytes', 'retained_bytes', 'cold_allocs')), run
        samples = captures / f'{run}.samples.gz'
        k = block, scene, n, engine, event
        assert k not in records
        if event == 'cycles':
            records[k] = list(BASE['chain_samples'](samples, release))
        else:
            assert event == 'ibs'
            records[k] = list(PROF['ibs_samples'](samples, release))
        pcs.update(pc for *_, stack in records[k] for pc in stack if isinstance(pc, int))
    assert len(records) == 16
    frames = BASE['symbolize'](debug, pcs)
    cycles, ibs, details = [], [], []
    for k, samples in records.items():
        counts, periods = Counter(), Counter()
        by_site = defaultdict(Counter)
        for *data, stack in samples:
            phase = LOADER['phase'](stack, frames)
            if phase not in LOADER['BUILD']:
                continue
            p = topology(stack, frames, site_phase)
            assert (phase == 'topology_commit') == (p is not None), (phase, p, stack)
            if k[-1] == 'cycles':
                period, = data
                periods['build_total'] += period
                counts['build_total'] += 1
                if p:
                    periods[p] += period
                    counts[p] += 1
            else:
                op, level, _, _ = data
                if op not in ('LOAD', 'STORE'):
                    continue
                counts['build_total', op, level] += 1
                if p:
                    counts[p, op, level] += 1
                if op == 'STORE' and stack and stack[0] in operands:
                    by_site[operands[stack[0]]][level] += 1
        if k[-1] == 'cycles':
            counts['topology_total'] = sum(counts[p] for p in BINS)
            periods['topology_total'] = sum(periods[p] for p in BINS)
            assert counts['topology_total'] >= 100
            for p in (*BINS, 'topology_total', 'build_total'):
                cycles.append((*k[:-1], p, counts[p], periods[p], percent(periods[p], periods['build_total']),
                               percent(periods[p], periods['topology_total']) if p != 'build_total' else ''))
        else:
            for (p, op, level), count in sorted(counts.items()):
                ibs.append((*k[:-1], p, op, level, count))
            for operand in sorted(set(operands.values())):
                for level, count in sorted(by_site[operand].items()):
                    details.append((*k[:-1], operand, level, count))
    prefix = 'block scene n engine '
    save(out / 'cycle-periods.csv', prefix + 'phase samples period pct_build_cycles pct_topology_cycles', cycles)
    save(out / 'ibs-data-sources.csv', prefix + 'phase op level samples', ibs)
    save(out / 'store-counts.csv', prefix + 'operand level samples', details)
    print('All 16 Release profiles match the staged control fixtures, pair checksums, work and memory; zero warm allocations.')
    print('Every staged topology sample reconciles with the prior build-phase attribution.')


if __name__ == '__main__':
    main(*map(Path, sys.argv[1:]))
