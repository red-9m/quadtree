#!/usr/bin/env python3
"""Split the staged Release build phase and verify/model the untimed routing census."""
import csv
import runpy
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROF = runpy.run_path(str(HERE.parent / 'g2-node-locality-2026-11-20/summarize.py'))
BASE, MEM = PROF['BASE'], PROF['MEM']
read, key = BASE['rows'], BASE['key']
save, percent = MEM['save'], MEM['percent']
GATE = HERE.parent / 'g2-contiguous-pairs-2026-11-20'
IMPL = 'quadtree-index-impl.h'
PARTITION = ('permutation_seed', 'gather_classify', 'stable_scatter', 'own_copy', 'finish_copy', 'plan_other')
BUILD = (*PARTITION, 'adapter', 'capture', 'rank_validate', 'rank_insert', 'dense_commit', 'topology_commit', 'build_other')
# Audited operands in load-instructions.asm on the staged (not narrow) binary.
SITES = {
    'gather_box': (0x145c9c, 0x145c9f, 0x145ca2, 0x145ca6, 0x145d92, 0x145d95, 0x145d98, 0x145d9b),
    'gather_id': (0x145c93, 0x145d87),
    'lookahead_id': (0x145c87, 0x145d7b),
    'scatter_tag': (0x145f00,),
    'scatter_id': (0x145f05,),
    'scatter_destination': (0x145f18,),
}


def build_phase(stack, frames):
    # The caller PC is return-PC minus one; these windows include the call's bytes.
    # Attribute memmove leaf samples even when its DSO lacks debug symbols.
    for pc in stack:
        if isinstance(pc, int):
            if 0x145f5e <= pc < 0x145f63:
                return 'own_copy'
            if 0x14597c <= pc < 0x145981 or 0x145af9 <= pc < 0x145afe:
                return 'finish_copy'
        for name, file, line in frames.get(pc, ()):
            if '::uniqueBuildRanks(' in name:
                return 'rank_validate'
            if '::captureBuildEntry<' in name:
                return 'capture'
            if file == IMPL:
                if 1611 <= line <= 1633:
                    return 'gather_classify'
                if 1663 <= line <= 1666:
                    return 'stable_scatter'
                if 1592 <= line <= 1682:
                    return 'plan_other'
                if 2275 <= line <= 2276:
                    return 'permutation_seed'
                if 2253 <= line <= 2270:
                    return 'rank_validate' if 2265 <= line <= 2266 else 'capture'
                if 288 <= line <= 327:
                    return 'rank_validate'
                if 2363 <= line <= 2364:
                    return 'rank_insert'
                if 2322 <= line <= 2369 or 2495 <= line <= 2508:
                    return 'dense_commit'
                if 1853 <= line <= 1916 or 2371 <= line <= 2382:
                    return 'topology_commit'
            if file == 'quadtree-index.h' and 1137 <= line <= 1161:
                return 'rank_insert'
            if file == 'bench-index.h' and 40 <= line <= 64:
                return 'adapter'
            if '::planBulk(' in name:
                return 'plan_other'
    return 'build_other'


def phase(stack, frames):
    p = PROF['phase'](stack, frames)
    if p == 'build':
        return build_phase(stack, frames)
    if p == 'other' and build_phase(stack, frames) == 'rank_validate':
        # The rank validator is called only by build; its outer DWARF chain
        # can stop before the benchmark's phase marker.
        return 'rank_validate'
    return p


def profile(captures, debug, out, ref):
    release = (captures / 'elf-path.txt').read_text().strip()
    text = PROF['TEXT']
    assert (captures / 'text.sha256').read_text().split() == [text, 'release.text', text, 'debug.text']
    records, pcs = {}, set()
    for spec in read(captures / 'run-order.csv'):
        block, scene, n, engine, event = (spec[f] for f in ('block', 'scene', 'n', 'engine', 'event'))
        run = f'{block}-{scene}-{engine}-{event}'
        row, = read(captures / f'{run}.csv')
        assert key(row) == (scene, n, engine) and n == '100000' and row['type'] == 'int32', run
        assert (row['status'], row['batch'], row['rounds']) == ('ok', '1', '1'), run
        assert int(row['reps']) >= 15 and float(row['allocs_per_step']) == 0, run
        for f in (*BASE['EXACT'], 'peak_bytes', 'retained_bytes', 'cold_allocs'):
            assert row[f] == ref[key(row)][f], (run, f)
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
    cycle_rows, memory_rows, focus_rows = [], [], []
    for k, samples in records.items():
        if k[-1] == 'cycles':
            periods, count = Counter(), Counter()
            for period, stack in samples:
                p = phase(stack, frames)
                periods[p] += period
                count[p] += 1
            build = sum(periods[p] for p in BUILD)
            assert sum(count[p] for p in BUILD) >= 500 and count['pairs'] >= 500, (k, count)
            for name, groups in [*((p, (p,)) for p in BUILD), ('partition_total', PARTITION),
                                 ('build_total', BUILD), ('pairs', ('pairs',)), ('other', ('other',))]:
                period = sum(periods[p] for p in groups)
                cycle_rows.append((*k[:-1], name, sum(count[p] for p in groups), period,
                                   percent(period, build) if name not in ('pairs', 'other') else ''))
        else:
            by_phase, by_pc = defaultdict(Counter), defaultdict(Counter)
            for op, level, _, _, stack in samples:
                p = phase(stack, frames)
                if p not in BUILD:
                    continue
                by_phase[p][op, level] += 1
                by_pc[stack[0]][op, level] += 1
            pivot = MEM['pivot']
            build = sum((by_phase[p] for p in BUILD), Counter())
            totals = pivot(build)
            assert totals['known_load'] > 500 and totals['non_l1_load'] >= 30, (k, totals)
            for name in (*BUILD, 'partition_total', 'build_total'):
                counts = build if name == 'build_total' else by_phase[name]
                if name == 'partition_total':
                    counts = sum((by_phase[p] for p in PARTITION), Counter())
                for (op, level), n in sorted(counts.items()):
                    if op in ('LOAD', 'STORE', 'PFETCH'):
                        memory_rows.append((*k[:-1], name, op, level, n))
            for operand, offsets in SITES.items():
                m = pivot(sum((by_pc[pc] for pc in offsets), Counter()))
                assert m['store' if operand == 'scatter_destination' else 'load'] > 0, (k, operand, m)
                focus_rows.append((*k[:-1], operand, ' '.join(map(hex, offsets)), *m.values(),
                                   percent(m['non_l1_load'], totals['non_l1_load'])))
    prefix = 'block scene n engine '
    save(out / 'cycle-periods.csv', prefix + 'phase samples period pct_build_cycles', cycle_rows)
    save(out / 'ibs-data-sources.csv', prefix + 'phase op level samples', memory_rows)
    save(out / 'focus-pcs.csv', prefix + 'operand pcs loads known_loads known_non_l1_loads stores '
         'pct_build_non_l1_loads', focus_rows)
    print('16 reused Release profiles match the staged gate rows, including pair checksums, work and allocations.')


def census(path, out, ref):
    runs = defaultdict(list)
    for row in read(path):
        k = key(row)
        for f in BASE['EXACT']:
            if f in ('scene_hash', 'checksum'):
                assert int(row[f], 16) == int(ref[k][f], 16), (k, f)
            else:
                assert row[f] == ref[k][f], (k, f)
        assert int(row['allocs']) == float(ref[k]['allocs_per_step']) == 0, k
        if not runs[k]:
            assert row['calls'] == '1' and row['input_ids'] == row['n'], k
        runs[k].append(row)
    assert len(runs) == 8
    assert {k[:2] for k in runs} == {(s, n) for s in ('uniform', 'split-line-translated') for n in ('10000', '100000')}
    totals = []
    for k, depths in runs.items():
        d = [{f: int(v) for f, v in r.items() if f not in ('scene', 'engine', 'scene_hash', 'checksum')} for r in depths]
        other = 'index-ordered' if k[-1] == 'index' else 'index'
        columns = list(depths[0])
        traffic = ('depth', 'nodes', *columns[columns.index('calls'):])
        assert [tuple(r[f] for f in traffic) for r in depths] == [
            tuple(r[f] for f in traffic) for r in runs[(*k[:2], other)]], k
        for i, r in enumerate(d):
            assert r['depth'] == i
            assert r['classify_ids'] <= r['input_ids'] and r['lookahead_ids'] <= r['classify_ids']
            assert r['scatter_ids'] <= r['classify_ids'] and r['child_ids'] <= r['scatter_ids']
            assert r['child_ids'] <= r['classify_child_ids'] <= r['classify_ids']
            assert r['single_bin_ids'] <= r['monotone_ids'] <= r['scatter_ids']
            assert r['single_bin_nodes'] <= r['monotone_nodes'] <= r['split_nodes'] <= r['classify_nodes'] <= r['calls']
            assert r['own_copy_ids'] <= r['scatter_ids'] - r['child_ids']
            if i:
                assert r['input_ids'] == d[i - 1]['child_ids'] and r['calls'] == 4 * d[i - 1]['split_nodes']
        s = Counter()
        for r in d:
            s.update(r)
        assert s['calls'] == 1 + 4 * s['split_nodes'] == d[0]['nodes'], k  # Top-down plan, not fallback.
        assert s['input_ids'] - s['child_ids'] == int(k[1]), k
        if k[-1] != 'index':
            continue  # Routing census is identical for both engines.
        # Logical bytes at current algorithm sites, not cache-line reads/writes.
        # Classify = source ID + normalized box + tag store, plus lookahead ID loads.
        # Scatter = tag read + source ID read + destination ID write.
        demand = 16 * s['classify_ids']
        classify = 21 * s['classify_ids'] + 4 * s['lookahead_ids']
        scatter = 9 * s['scatter_ids']
        copies = 8 * (s['own_copy_ids'] + s['finish_copy_ids'])
        direct_own = 8 * s['own_copy_ids']
        monotone_local = 9 * s['monotone_ids'] + 8 * (s['monotone_removed_own_copy_ids'] - s['monotone_added_own_copy_ids'])
        # Two-level memo: even split passes put the predicted odd tag in the high bits.
        # Charge one output tag byte per routed child and one extra tag read per odd
        # classify; odd classify avoids its ID, box, prefetch ID, and tag store.
        compute = sum(r['classify_child_ids'] for r in d[::2])
        scatter_tags = sum(r['child_ids'] for r in d[::2])
        use = sum(r['classify_ids'] for r in d[1::2])
        lookahead = sum(r['lookahead_ids'] for r in d[1::2])
        assert 0 < use <= scatter_tags <= compute, k
        two_level = 20 * use + 4 * lookahead - scatter_tags
        totals.append((*k[:2], d[0]['nodes'], s['classify_ids'], s['scatter_ids'], s['lookahead_ids'], s['prefix_ids'],
                       demand, classify, scatter, copies, direct_own, s['single_bin_nodes'], s['single_bin_ids'],
                       9 * s['single_bin_ids'], s['monotone_nodes'], s['monotone_ids'], monotone_local,
                       compute, scatter_tags, use, compute - use, percent(use, s['classify_ids']), two_level))
    save(out / 'traffic-model.csv', 'scene n nodes classify_ids scatter_ids lookahead_ids prefix_keys '
         'demand_box_bytes classify_bytes scatter_bytes copy_bytes direct_own_saved_bytes single_bin_nodes '
         'single_bin_ids single_bin_local_saved_bytes monotone_nodes monotone_ids monotone_local_saved_bytes '
         'two_level_tags_computed two_level_tags_scattered two_level_tags_used '
         'two_level_tags_unused two_level_pct_gathers_avoided '
         'two_level_net_bytes_before_control_overhead', totals)
    print('Eight census rows match fixture/work/pair checksums and warm allocations; per-depth counts reconcile with topology.')
    print('Ordered and unordered routing counters match at each depth on all four fixtures.')


if __name__ == '__main__':
    captures, debug, counts, out = map(Path, sys.argv[1:])
    ref = {key(r): r for r in read(GATE / '15-control/snapshot.csv')}
    profile(captures, debug, out, ref)
    census(counts, out, ref)
