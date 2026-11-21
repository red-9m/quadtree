#!/usr/bin/env python3
"""Attribute staged Release samples by call chain and audited node operand PC."""
import csv
import gzip
import re
import runpy
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
BASE = runpy.run_path(str(HERE.parent / 'g2-phase-profile-2026-11-19/summarize.py'))
MEM = runpy.run_path(str(HERE.parent / 'g2-build-traffic-2026-11-20/summarize.py'))
LOADS = {'count', 'bounds', 'firstChild'}
OTHER_FIELDS = {'summary', 'head', 'ownCount', 'linear'}
TEXT = '7b4c8406d554cc8772f2b40933f2f96257153164abcd77b30ad72f9cde2977fe'
IBS_HEADER = re.compile(r'^\s*\d+\s+(?:0x)?([\da-f]+)\s+[\da-f]+\s+\|OP ([^|]+)\|LVL ([^|]+)\|.*\s+(\d+)\s*$')


def ibs_samples(path, release):
    with gzip.open(path, 'rt') as f:
        for record in f.read().strip().split('\n\n'):
            header, *chain = record.strip().splitlines()
            match = IBS_HEADER.match(header)
            assert match, header
            addr, op, level, weight = match.groups()
            stack = []
            for i, frame in enumerate(chain):
                _, dso = frame.strip().split(maxsplit=1)
                name, sep, off = dso[1:-1].rpartition('+0x')
                if sep and name == release:
                    stack.append(int(off, 16) - bool(i))
                else:
                    stack.append(Path(name or dso[1:-1]).name)
            yield op, level.strip(), int(addr, 16), int(weight), tuple(stack)


def phase(stack, frames):
    chain = [frame for pc in stack for frame in frames.get(pc, ())]
    for name, file, line in chain:
        if file == 'quadtree-bench.cpp' and 'runStep(' in name:
            if line == 684:
                return 'build'
            if line == 686:
                return 'pairs'
    for name, file, line in chain:
        if '::build' in name or '::planBulk' in name or '::commitBulk' in name:
            return 'build'
        if file == 'bench-index.h' and 40 <= line <= 64:
            return 'build'
        if any(f'::{fn}<' in name or f'::{fn}(' in name for fn in (
            'pairs', 'forEachPair', 'orderedSelf', 'pairQualifies', 'testPair', 'reportPair',
            'crossPairs', 'selfPairs', 'itemsVsSubtree', 'walkOrdered', 'walk', 'sortRanks'
        )):
            return 'pairs'
        if file == 'quadtree-bench.cpp' and 436 <= line <= 442:
            return 'pairs'
    return 'other'


def query_function(leaf):
    if isinstance(leaf, int):
        if 0x9ad90 <= leaf < 0x9b297:
            return 'crossPairs'
        if 0x1e660 <= leaf < 0x1eb26:
            return 'walkOrdered'
    return 'other_pair'


def main(captures, debug, out):
    read, key = BASE['rows'], BASE['key']
    percent, save = MEM['percent'], MEM['save']
    release = (captures / 'elf-path.txt').read_text().strip()
    assert (captures / 'text.sha256').read_text().split() == [TEXT, 'release.text', TEXT, 'debug.text']
    previous = HERE.parent / 'g2-contiguous-pairs-2026-11-20/15-control/snapshot.csv'
    ref = {key(r): r for r in read(previous)}
    site_map = {}
    for r in read(HERE / 'node-loads.csv'):
        for s in r['pcs'].split():
            pc = int(s, 16)
            assert pc not in site_map and query_function(pc) == r['function']
            site_map[pc] = r['field'], int(r['node_offset'])
    specs = read(captures / 'run-order.csv')
    cycles, ibs, pcs, rows = {}, {}, set(), []
    for spec in specs:
        block, scene, n, engine, event = (spec[f] for f in ('block', 'scene', 'n', 'engine', 'event'))
        run = f'{block}-{scene}-{engine}-{event}'
        r, = read(captures / f'{run}.csv')
        assert key(r) == (scene, n, engine) and n == '100000' and r['type'] == 'int32', run
        assert (r['status'], r['batch'], r['rounds']) == ('ok', '1', '1'), run
        assert float(r['allocs_per_step']) == 0 and int(r['reps']) >= 15, run
        for field in (*BASE['EXACT'], 'peak_bytes', 'retained_bytes', 'cold_allocs'):
            assert r[field] == ref[key(r)][field], (run, field)
        rows.append((run, *r.values()))
        k = block, scene, engine
        path = captures / f'{run}.samples.gz'
        if event == 'cycles':
            assert k not in cycles
            cycles[k] = list(BASE['chain_samples'](path, release))
            stacks = (stack for _, stack in cycles[k])
        else:
            assert event == 'ibs' and k not in ibs
            ibs[k] = list(ibs_samples(path, release))
            stacks = (stack for *_, stack in ibs[k])
        pcs.update(pc for stack in stacks for pc in stack if isinstance(pc, int))
    assert len(specs) == 16 and len(cycles) == len(ibs) == 8 and cycles.keys() == ibs.keys()
    frames = BASE['symbolize'](debug, pcs)

    overview, detail, levels, residues, cycle_rows = [], [], [], [], []
    for k, cycle_samples in cycles.items():
        focus = 'crossPairs' if k[-1] == 'index' else 'walkOrdered'
        periods, sample_counts, pc_cycles = Counter(), Counter(), Counter()
        pc_loads, pc_levels, addr_counts = defaultdict(Counter), defaultdict(Counter), Counter()
        pair_levels, phase_loads = Counter(), Counter()
        for period, stack in cycle_samples:
            leaf = stack[0] if stack else None
            p = phase(stack, frames)
            periods[p] += period
            sample_counts[p] += 1
            if p == 'pairs':
                pc_cycles[leaf] += period
        assert min(sample_counts['build'], sample_counts['pairs']) >= 500, (k, sample_counts)
        focus_period = sum(count for pc, count in pc_cycles.items() if query_function(pc) == focus)
        cycle_rows.append((*k, focus, len(cycle_samples), periods['build'], periods['pairs'], periods['other'],
                           focus_period, percent(focus_period, periods['pairs'])))

        for op, level, addr, weight, stack in ibs[k]:
            if op != 'LOAD':
                continue
            p = phase(stack, frames)
            phase_loads[p] += 1
            if p != 'pairs':
                continue
            pair_levels[level] += 1
            leaf = stack[0] if stack else None
            pc_levels[leaf][level] += 1
            if leaf in site_map:
                assert addr == 0 or (addr - site_map[leaf][1]) % 8 == 0, (k, leaf, hex(addr))
                field, offset = site_map[leaf]
                if addr:
                    addr_counts[field, (addr - offset) % 64, level] += 1
                pc_loads[leaf]['valid_addr'] += bool(addr)
                if level not in ('N/A', 'L1 hit'):
                    pc_loads[leaf]['non_l1_weight_sum'] += weight

        def summarize(counts):
            all_loads = counts.total()
            known = all_loads - counts['N/A']
            non_l1 = known - counts['L1 hit']
            return all_loads, known, non_l1

        focus_levels = sum((v for pc, v in pc_levels.items() if query_function(pc) == focus), Counter())
        assert min(phase_loads['build'], phase_loads['pairs'], summarize(focus_levels)[1]) >= 500, k
        groups = [('pair_total', pair_levels), ('function_total', focus_levels)]
        group_periods = {'pair_total': periods['pairs'], 'function_total': focus_period}
        target_nodes = {pc for pc in site_map if query_function(pc) == focus}
        for group, names in [('requested_fields', LOADS), ('other_node_fields', OTHER_FIELDS),
                             *[(name, {name}) for name in sorted(LOADS | OTHER_FIELDS)]]:
            group_pcs = {pc for pc in target_nodes if site_map[pc][0] in names}
            groups.append((group, sum((pc_levels[pc] for pc in group_pcs), Counter())))
            group_periods[group] = sum(pc_cycles[pc] for pc in group_pcs)
        for group, counts in groups:
            all_loads, known, non_l1 = summarize(counts)
            overview.append((*k, focus, group, all_loads, known, non_l1, percent(non_l1, known),
                             percent(non_l1, summarize(pair_levels)[2]),
                             percent(non_l1, summarize(focus_levels)[2]) if group != 'pair_total' else '',
                             percent(group_periods[group], periods['pairs'])))
            levels.extend((*k, focus, group, l, n) for l, n in sorted(counts.items()))
        for (field, residue, level), count in sorted(addr_counts.items()):
            residues.append((*k, focus, field, residue, level, count))
        for pc in sorted((pc for pc in pc_levels if isinstance(pc, int) and query_function(pc) == focus)):
            name, offset = site_map.get(pc, ('unmapped', ''))
            chain = ' <- '.join(f'{fn.split("<")[0]} @ {file}:{line}' for fn, file, line in frames[pc])
            detail.append((*k, focus, hex(pc), name, offset, *summarize(pc_levels[pc]),
                           pc_loads[pc]['valid_addr'], pc_loads[pc]['non_l1_weight_sum'], pc_cycles[pc], chain))
    prefix = 'block scene engine function '
    save(out / 'profile-rows.csv', 'capture ' + ' '.join(r), rows)
    save(out / 'cycle-periods.csv', prefix + 'cycle_samples build_period pair_period other_period '
         'function_period function_pct_pair_cycles', cycle_rows)
    save(out / 'ibs-summary.csv', prefix + 'group loads known_loads known_non_l1_loads pct_known_non_l1 '
         'pct_pair_non_l1 pct_function_non_l1 pct_pair_cycle_period_at_pcs', overview)
    save(out / 'ibs-levels.csv', prefix + 'group level samples', levels)
    save(out / 'ibs-node-residues.csv', prefix + 'field node_start_mod64 level samples', residues)
    save(out / 'ibs-sites.csv', prefix + 'pc field node_offset loads known_loads known_non_l1_loads '
         'loads_with_address non_l1_weight_sum cycle_period leaf_frames', detail)
    print('16 captures match the 15-round staged control on fixture, pair checksum, work, memory and cold allocations.')
    print('All 16 warm rows allocate zero. Both events cover build and pairs; IBS covers both target functions.')
    print('All nonzero mapped IBS data addresses have the expected alignment for the audited node field.')


if __name__ == '__main__':
    captures, debug, out = map(Path, sys.argv[1:])
    main(captures, debug, out)
