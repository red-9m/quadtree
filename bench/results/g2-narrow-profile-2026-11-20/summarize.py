#!/usr/bin/env python3
"""Attribute narrow-stream Release cycles and unordered build IBS sites."""
import csv
import gzip
import runpy
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
BASE = runpy.run_path(str(HERE.parent / 'g2-phase-profile-2026-11-19/summarize.py'))
MEM = runpy.run_path(str(HERE.parent / 'g2-build-traffic-2026-11-20/summarize.py'))
IMPL = 'quadtree-index-impl.h'
BUILD = ('adapter', 'capture', 'rank_validation', 'split_classify', 'split_scatter', 'split_other',
         'dense_commit', 'rank_insert', 'topology_commit', 'build_other')
PARTITION = ('split_classify', 'split_scatter', 'split_other')
QUERY = ('child_occupancy_bounds', 'child_dispatch_charges', 'popcount_calls', 'ordered_walk_local', 'source_sort',
         'row_sort', 'rank_check_collect', 'pair_eligibility', 'pair_report', 'walk_unordered',
         'cross_nodes', 'self_nodes', 'items_vs_subtree', 'pair_other')
WALK = ('child_occupancy_bounds', 'child_dispatch_charges', 'popcount_calls', 'ordered_walk_local')
# Instruction addresses checked in hot-instructions.asm for the frozen binary.
BOX_LOADS = (0x145e82, 0x145e85, 0x145e88, 0x145e8b)
POPCOUNT_CALLS = ((0x1e7cb, 0x1e7d0), (0x1e92d, 0x1e932))
RANGES = {
    IMPL: (
        (1613, 1629, 'split_classify'), (1659, 1663, 'split_scatter'), (1592, 1677, 'split_other'),
        (2269, 2284, 'split_other'), (288, 326, 'rank_validation'), (2260, 2261, 'rank_validation'),
        (2241, 2258, 'capture'), (2436, 2457, 'capture'),
        (2358, 2359, 'rank_insert'), (2310, 2363, 'dense_commit'), (2490, 2503, 'dense_commit'),
        (1853, 1904, 'topology_commit'), (1800, 1842, 'topology_commit'), (2364, 2375, 'topology_commit'),
        (2164, 2377, 'build_other'), (2505, 2549, 'build_other'),
        (2669, 2680, 'child_occupancy_bounds'), (2681, 2696, 'child_dispatch_charges'),
        (2657, 2668, 'ordered_walk_local'), (2914, 2916, 'source_sort'),
        (2868, 2908, 'row_sort'), (2947, 2949, 'row_sort'), (2952, 2952, 'row_sort'),
        (2921, 2927, 'rank_check_collect'), (2734, 2751, 'pair_eligibility'),
        (2754, 2759, 'pair_report'), (2727, 2731, 'pair_report'), (2953, 2954, 'pair_report'),
        (2635, 2653, 'walk_unordered'), (3045, 3072, 'cross_nodes'), (3075, 3097, 'self_nodes'),
        (3026, 3042, 'items_vs_subtree'), (2911, 2956, 'pair_other'), (1185, 1220, 'pair_other'),
    ),
    'quadtree-index.h': ((1137, 1161, 'rank_insert'),),
    'bench-index.h': ((40, 64, 'adapter'), (98, 104, 'pair_other')),
    'quadtree-bench.cpp': ((684, 684, 'build_other'), (686, 686, 'pair_other'), (436, 442, 'pair_report')),
}


def classify(stack, frames):
    # Count the external helper and its PLT stub only with a confirmed caller.
    if len(stack) >= 2 and (stack[0] == 'libgcc_s.so.1' or isinstance(stack[0], int) and 0x72c0 <= stack[0] < 0x72d0):
        caller = next((pc for pc in stack[1:] if isinstance(pc, int)), -1)
        if any(lo <= caller < hi for lo, hi in POPCOUNT_CALLS):
            return 'popcount_calls'
    # The inlined or actual leaf frame takes precedence over recursive callers.
    for pc in stack:
        for _, file, line in frames.get(pc, ()):
            for lo, hi, phase in RANGES.get(file, ()):
                if lo <= line <= hi:
                    return phase
    return 'other_process'


def main(captures, debug, out):
    read, key = BASE['rows'], BASE['key']
    save, percent, pivot = (MEM[k] for k in ('save', 'percent', 'pivot'))
    release = (captures / 'elf-path.txt').read_text().strip()
    ref = {key(r): r for r in read(HERE.parent / 'g2-bulk-streams-2026-11-20/15-candidate/snapshot.csv')}
    specs = list(csv.DictReader((captures / 'run-order.csv').open()))
    cycles, ibs, pcs, bench = {}, {}, set(), []
    for spec in specs:
        block, scene, n, engine, event = (spec[f] for f in ('block', 'scene', 'n', 'engine', 'event'))
        run = f'{block}-{scene}-{engine}-{event}'
        row, = read(captures / f'{run}.csv')
        assert key(row) == (scene, n, engine) and n == '100000' and row['type'] == 'int32', run
        assert (row['status'], row['batch'], row['rounds']) == ('ok', '1', '1'), run
        assert float(row['allocs_per_step']) == 0 and int(row['reps']) >= 15, run
        for f in (*BASE['EXACT'], 'cold_allocs', 'peak_bytes', 'retained_bytes'):
            assert row[f] == ref[key(row)][f], (run, f)
        bench.append((run, *row.values()))
        path = captures / f'{run}.samples.gz'
        k = block, scene, n, engine
        if event == 'cycles':
            assert k not in cycles
            cycles[k] = list(BASE['chain_samples'](path, release))
            stacks = (stack for _, stack in cycles[k])
        else:
            assert event == 'ibs' and engine == 'index' and k not in ibs
            ibs[k] = list(MEM['ibs_samples'](path, release))
            stacks = (stack for _, _, stack in ibs[k])
        pcs.update(pc for stack in stacks for pc in stack if isinstance(pc, int))
    assert len(specs) == 12 and len(cycles) == 8 and len(ibs) == 4 and ibs.keys() <= cycles.keys()
    frames = BASE['symbolize'](debug, pcs)

    totals, build_rows, query_rows, count_rows, ibs_rows, ibs_labels, hot_loads = [], [], [], [], [], [], []
    with gzip.open(out / 'sites.csv.gz', 'wt', newline='') as f:
        sites = csv.writer(f, lineterminator='\n')
        sites.writerow('block scene n engine phase pc cycle_samples cycle_period ibs_load ibs_known_load '
                       'ibs_non_l1_load ibs_store leaf_inline_frames'.split())
        for k, samples in cycles.items():
            periods, counts, site_periods, site_counts = Counter(), Counter(), Counter(), Counter()
            for period, stack in samples:
                phase = classify(stack, frames)
                leaf = stack[0] if stack else 'unknown'
                periods[phase] += period
                counts[phase] += 1
                site_periods[phase, leaf] += period
                site_counts[phase, leaf] += 1
            build = sum(periods[p] for p in BUILD)
            query = sum(periods[p] for p in QUERY)
            assert sum(counts[p] for p in BUILD) > 500 and sum(counts[p] for p in QUERY) > 500, k
            totals.append((*k, counts.total(), build, query, periods['other_process'], percent(build, build + query)))
            build_rows.extend((*k, p, percent(periods[p], build)) for p in BUILD)
            build_rows.append((*k, 'partition_total', percent(sum(periods[p] for p in PARTITION), build)))
            query_rows.extend((*k, p, percent(periods[p], query)) for p in QUERY)
            query_rows.append((*k, 'ordered_walk_total', percent(sum(periods[p] for p in WALK), query)))
            query_rows.append((*k, 'child_dispatch_total',
                               percent(periods['child_dispatch_charges'] + periods['popcount_calls'], query)))
            count_rows.extend((*k, p, counts[p], periods[p]) for p in (*BUILD, *QUERY, 'other_process'))

            site_mem, mem = defaultdict(Counter), defaultdict(Counter)
            for op, level, stack in ibs.get(k, ()):
                phase = classify(stack, frames)
                leaf = stack[0] if stack else 'unknown'
                site_mem[phase, leaf][op, level] += 1
                mem[phase][op, level] += 1
            if k in ibs:
                build_mem = pivot(sum((mem[p] for p in BUILD), Counter()))
                assert build_mem['known_load'] > 500 and build_mem['non_l1_load'] > 100, (k, build_mem)
                for name, offsets in (('box_edges', BOX_LOADS), ('source_id', (0x145e77,))):
                    m = pivot(sum((site_mem['split_classify', pc] for pc in offsets), Counter()))
                    assert m['known_load'] > 0, (k, name)
                    hot_loads.append((*k, name, ' '.join(map(hex, offsets)), m['known_load'], m['non_l1_load'],
                                      percent(m['non_l1_load'], build_mem['non_l1_load'])))
                for p in (*BUILD, 'partition_total', 'build_total'):
                    if p == 'partition_total':
                        counts_mem = sum((mem[q] for q in PARTITION), Counter())
                    elif p == 'build_total':
                        counts_mem = sum((mem[q] for q in BUILD), Counter())
                    else:
                        counts_mem = mem[p]
                        ibs_labels.extend((*k, p, op, level, n) for (op, level), n in sorted(counts_mem.items()))
                    m = pivot(counts_mem)
                    ibs_rows.append((*k, p, *m.values(), percent(m['non_l1_load'], build_mem['non_l1_load']),
                                     percent(m['store'], build_mem['store'])))
            for p, leaf in sorted(site_counts.keys() | site_mem.keys(), key=lambda x: (x[0], str(x[1]))):
                site = p, leaf
                label = ' <- '.join(f'{name} @ {file}:{line}' for name, file, line in frames.get(leaf, ()))
                sites.writerow((*k, p, hex(leaf) if isinstance(leaf, int) else leaf, site_counts[site],
                                site_periods[site], *pivot(site_mem[site]).values(), label))
    prefix = 'block scene n engine '
    save(out / 'run-totals.csv', prefix + 'cycle_samples build_period pair_period other_period build_pct_step', totals)
    save(out / 'build-attribution.csv', prefix + 'phase pct_build_cycles', build_rows)
    save(out / 'pair-attribution.csv', prefix + 'phase pct_pair_cycles', query_rows)
    save(out / 'cycle-periods.csv', prefix + 'phase samples period', count_rows)
    save(out / 'ibs-build.csv', prefix + 'phase ibs_load ibs_known_load ibs_non_l1_load ibs_store '
         'pct_build_non_l1_load pct_build_store', ibs_rows)
    save(out / 'ibs-data-sources.csv', prefix + 'phase op level samples', ibs_labels)
    save(out / 'focus-loads.csv', prefix + 'operand pcs known_loads non_l1_loads pct_build_non_l1_load', hot_loads)
    save(out / 'profile-rows.csv', 'run ' + ' '.join(row), bench)
    print('12 Release profiles agree with the frozen narrow build on fixtures, checksums, work, memory and cold allocations.')
    print('All profiles had zero warm allocations. Eight cycle profiles and four IBS profiles have build sample coverage.')
    census = read(out / 'census.csv')
    assert {(r['scene'], r['packet']) for r in census} == {
        (scene, packet) for scene in ('split-line-translated', 'mixed-size') for packet in ('4', '8')}
    assert len(census) == 4
    for r in census:
        frozen = ref[r['scene'], r['n'], 'index-ordered']
        assert all(r[f] == frozen[f] for f in ('pairs', 'checksum', 'candidates', 'node_visits')), r
        c = {field: int(r[field]) for field in r if field not in ('scene', 'checksum')}
        assert sum(c[f'admit_{i}'] for i in range(5)) == c['groups']
        assert sum(i * c[f'admit_{i}'] for i in range(5)) == c['overlapping_children']
        assert c['occupied_children'] + c['n'] - 1 == c['node_visits']
        assert c['distinct_groups'] <= c['groups'] and c['upper_distinct_groups'] <= c['upper_groups']
    print('All four untimed census rows reconcile with the gate results and visit accounting.')


if __name__ == '__main__':
    captures, debug, out = sys.argv[1:]
    main(Path(captures), debug, Path(out))
