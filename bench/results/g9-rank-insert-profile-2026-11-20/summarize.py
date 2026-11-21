#!/usr/bin/env python3
"""Attribute build-only rank hash insertion instructions on the hash-checked ELF."""
import runpy
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
LOADER = runpy.run_path(str(HERE.parent / 'g2-staged-traffic-2026-11-20/summarize.py'))
BASE, PROF, MEM = (LOADER[k] for k in ('BASE', 'PROF', 'MEM'))
read, key, save, percent = (LOADER[k] for k in ('read', 'key', 'save', 'percent'))
STAGED = HERE.parent / 'g2-topology-cost-2026-11-20'
VALIDATE = ((0xfae40, 0xfb0f4),)
INSERT = (0x144140, 0x144444)
SPLAY = (0x12d800, 0x12d952)
BINS = ('insert_setup_return', 'home_mix_mask', 'epoch_address_test', 'probe_advance',
        'probe_stats_and_bound', 'bucket_publish', 'tree_insert', 'tree_conversion')
# Inclusive start / exclusive end. Some operands share the cumulative bound
# and stats update: GCC keeps the count in a register across the probe loop.
RANGES = {
    'home_mix_mask': ((0x1441f8, 0x144202), (0x144206, 0x14420d), (0x144218, 0x144248)),
    'epoch_address_test': ((0x144293, 0x1442a3),),
    'probe_advance': ((0x144280, 0x144284), (0x144288, 0x14428b)),
    'probe_stats_and_bound': ((0x1441f4, 0x1441f8), (0x14420d, 0x144218), (0x144284, 0x144288),
                              (0x14428b, 0x144293), (0x1442a3, 0x1442a7)),
    'bucket_publish': ((0x1442a7, 0x1442b5),),
    'tree_insert': ((0x144156, 0x1441e0), (0x1442c0, 0x1442f8)),
    'tree_conversion': ((0x144300, 0x144444),),
}
SITES = {
    0x14429d: ('epoch_load', 'LOAD'),
    0x1441f4: ('probe_count_load', 'LOAD'),
    0x1442a3: ('probe_count_store', 'STORE'),
    0x1442a7: ('key_store', 'STORE'),
    0x1442aa: ('epoch_store', 'STORE'),
    0x1442ae: ('reserved_word_store', 'STORE'),
}


def inside(pc, ranges):
    return isinstance(pc, int) and any(a <= pc < b for a, b in ranges)


def phase(stack, frames):
    if stack:
        leaf = stack[0]
        if inside(leaf, (INSERT,)):
            return next((p for p, ranges in RANGES.items() if inside(leaf, ranges)), 'insert_setup_return')
        if inside(leaf, (SPLAY,)):
            # Parent PCs are return PC minus one. No mutation operations run here.
            if any(0x1443a5 <= pc < 0x1443aa for pc in stack[1:] if isinstance(pc, int)):
                return 'tree_conversion'
            return 'tree_insert'
        if inside(leaf, VALIDATE) or any(inside(pc, VALIDATE) for pc in stack[1:]):
            return 'rank_validate'
    p = PROF['phase'](stack, frames)
    if p == 'build' and any('::uniqueBuildRanks(' in fn for pc in stack for fn, _, _ in frames.get(pc, ())):
        return 'rank_validate'
    return {'build': 'other_build'}.get(p, p)


def check_outputs(row, ref):
    for f in BASE['EXACT']:
        equal = int(row[f], 16) == int(ref[f], 16) if f in ('checksum', 'scene_hash') else row[f] == ref[f]
        assert equal, (key(row), f)


def census(data, ref, out):
    seen = set()
    rows = []
    for row in read(data):
        k = key(row)
        assert k in ref and k not in seen and row['engine'] == 'index-ordered'
        seen.add(k)
        check_outputs(row, ref[k])
        n = int(row['n'])
        assert n == int(row['rank_count']) and row['allocs'] == '0'
        assert all(row[f] == '0' for f in ('tree', 'warm_resets', 'warm_conversions', 'cold_conversions'))
        assert row['warm_probes'] == row['warm_probes_from_displacement'] == row['cold_probes']
        cells, size = int(row['capacity']), int(row['cell_bytes'])
        assert row['cold_resets'] == row['capacity'] and size == 16 and 2 * n <= cells
        assert sum(int(row[f]) for f in row if f.startswith('distance_')) == n
        probes = int(row['warm_probes'])
        rows.append((*k, f'{n / cells:.4f}', probes, f'{probes / n:.4f}', percent(n - int(row['distance_0']), n),
                     percent(int(row['distance_8_plus']), n), row['max_displacement'], cells * size,
                     64 * int(row['home_epoch_lines']), 64 * int(row['occupied_epoch_lines'])))
    assert seen == {k for k in ref if k[-1] == 'index-ordered' and int(k[1]) >= 1000}
    save(out / 'census-summary.csv', 'scene n engine load_factor warm_probes probes_per_rank '
         'pct_displaced pct_distance_8_plus max_displacement table_bytes home_epoch_line_bytes '
         'occupied_epoch_line_bytes', rows)
    print('18 untimed ranked fixture rows match the staged gate on fixture hashes, pair checksums and work.')
    print('Zero warm allocations, bucket resets and conversions. The displacement census independently reconciles every probe total.')


def profile(captures, debug, out, ref):
    release = (captures / 'elf-path.txt').read_text().strip()
    text = next(s.split()[0] for s in (STAGED / 'text.sha256').read_text().splitlines() if s.endswith(' candidate.text'))
    assert (captures / 'text.sha256').read_text().split() == [text, 'release.text', text, 'debug.text']
    pcs, records = set(), {}
    for spec in read(captures / 'run-order.csv'):
        block, scene, n, engine, event = (spec[f] for f in ('block', 'scene', 'n', 'engine', 'event'))
        run = f'{block}-{scene}-{engine}-{event}'
        row, = read(captures / f'{run}.csv')
        k = key(row)
        assert k == (scene, n, engine) and engine == 'index-ordered' and n == '100000' and row['type'] == 'int32', run
        assert (row['status'], row['batch'], row['rounds']) == ('ok', '1', '1'), run
        assert int(row['reps']) >= 15 and float(row['allocs_per_step']) == 0, run
        check_outputs(row, ref[k])
        assert all(row[f] == ref[k][f] for f in ('peak_bytes', 'retained_bytes', 'cold_allocs')), run
        path = captures / f'{run}.samples.gz'
        samples = list(BASE['chain_samples'](path, release) if event == 'cycles' else PROF['ibs_samples'](path, release))
        k = block, scene, n, engine, event
        assert k not in records
        records[k] = samples
        pcs.update(pc for *_, stack in samples for pc in stack if isinstance(pc, int))
    assert set(records) == {(b, s, '100000', 'index-ordered', e) for b in ('1', '2')
                            for s in ('uniform', 'split-line', 'split-line-translated', 'mixed-size')
                            for e in ('cycles', 'ibs')}
    frames = BASE['symbolize'](debug, pcs)
    cycles, cycle_sites, ibs, focus = [], [], [], []
    for k, samples in records.items():
        count, periods, data = Counter(), Counter(), Counter()
        by_pc = defaultdict(Counter)
        site_samples, site_periods = Counter(), Counter()
        for *fields, stack in samples:
            p = phase(stack, frames)
            count[p] += 1
            if k[-1] == 'cycles':
                period, = fields
                periods[p] += period
                if p in BINS:
                    site_samples[p, stack[0]] += 1
                    site_periods[p, stack[0]] += period
            else:
                op, level, address, _ = fields
                if op in ('LOAD', 'STORE'):
                    data[p, op, level] += 1
                    if stack and stack[0] in SITES:
                        role, expected = SITES[stack[0]]
                        if op == expected:
                            by_pc[role][op, level] += 1
                            if role == 'epoch_load' and address:
                                assert address % 4 == 0
        assert count['tree_insert'] == count['tree_conversion'] == 0, k
        build_phases = (*BINS, 'rank_validate', 'other_build')
        if k[-1] == 'cycles':
            assert count['rank_validate'] >= 50 and count['pairs'] >= 1000, (k, count)
            total = sum(periods[p] for p in build_phases)
            insert = sum(periods[p] for p in BINS)
            assert sum(count[p] for p in BINS) >= 100, (k, count)
            groups = [*((p, (p,)) for p in BINS), ('rank_insert_total', BINS), ('rank_validate', ('rank_validate',)),
                      ('other_build', ('other_build',)), ('build_total', build_phases), ('pairs', ('pairs',)), ('other', ('other',))]
            for phase_name, group in groups:
                period = sum(periods[p] for p in group)
                cycles.append((*k[:-1], phase_name, sum(count[p] for p in group), period,
                               percent(period, total) if phase_name not in ('pairs', 'other') else '',
                               percent(period, insert) if phase_name in BINS else ''))
            for (p, pc), period in sorted(site_periods.items()):
                cycle_sites.append((*k[:-1], p, hex(pc), site_samples[p, pc], period, percent(period, insert)))
        else:
            build_non_l1 = sum(n for (p, op, level), n in data.items()
                               if p in build_phases and op == 'LOAD' and level not in ('L1 hit', 'N/A'))
            assert build_non_l1 > 0 and by_pc['epoch_load'].total() > 0, (k, by_pc)
            for (p, op, level), n in sorted(data.items()):
                if p in build_phases:
                    ibs.append((*k[:-1], p, op, level, n))
            for pc, (role, _) in SITES.items():
                m = MEM['pivot'](by_pc[role])
                focus.append((*k[:-1], role, hex(pc), *m.values(), percent(m['non_l1_load'], m['known_load']),
                              percent(m['non_l1_load'], build_non_l1)))
    prefix = 'block scene n engine '
    save(out / 'cycle-periods.csv', prefix + 'phase samples period pct_build_cycles pct_insert_cycles', cycles)
    save(out / 'cycle-sites.csv', prefix + 'phase pc samples period pct_insert_cycles', cycle_sites)
    save(out / 'ibs-data-sources.csv', prefix + 'phase op level samples', ibs)
    save(out / 'focus-pcs.csv', prefix + 'operand pc loads known_loads known_non_l1_loads stores '
         'pct_known_loads_non_l1 pct_build_non_l1_loads', focus)
    print('16 staged Release captures match the frozen work, pair checksums, allocations and footprint.')
    print('No tree conversion or splay samples in the timed fixture profiles.')


if __name__ == '__main__':
    captures, debug, counts, out = map(Path, sys.argv[1:])
    ref = {key(r): r for r in read(STAGED / 'frozen/15-candidate/snapshot.csv')}
    census(counts, ref, out)
    profile(captures, debug, out, ref)
