#!/usr/bin/env python3
"""Analyze audited scatter PCs and a separate untimed cursor-reuse census."""
import runpy
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
RANK = runpy.run_path(str(HERE.parent / 'g9-rank-insert-profile-2026-11-20/summarize.py'))
BASE, PROF, MEM = (RANK[k] for k in ('BASE', 'PROF', 'MEM'))
read, key, save, percent = (RANK[k] for k in ('read', 'key', 'save', 'percent'))
STAGED = HERE.parent / 'g2-topology-cost-2026-11-20'
SCENES = ('uniform', 'split-line-translated')
ENGINES = ('index', 'index-ordered')
GAPS = ('first', 'gap_1', 'gap_2', 'gap_3_4', 'gap_5_8', 'gap_9_plus')


def census(path, out, ref):
    known = {(r['scene'], r['n']): r for r in read(HERE.parent / 'g2-staged-traffic-2026-11-20/traffic-model.csv')}
    datasets = defaultdict(list)
    for row in read(path):
        k = key(row)
        RANK['check_outputs'](row, ref[k])
        assert float(row['allocs']) == float(ref[k]['allocs_per_step']) == 0, k
        datasets[k].append(row)
    assert datasets.keys() == {(s, n, e) for s in SCENES for n in ('10000', '100000') for e in ENGINES}
    all_rows, depth_rows = [], []
    for k, group in datasets.items():
        if k[-1] != 'index':
            continue
        columns = list(group[0])
        sites = ('nodes', 'depth', *columns[columns.index('groups'):])
        other = datasets[(*k[:2], 'index-ordered')]
        assert [tuple(r[f] for f in sites) for r in group] == [tuple(r[f] for f in sites) for r in other], k
        counts = Counter()
        expected = 0.0
        for i, r in enumerate(group):
            d = {f: int(r[f]) for f in sites if f != 'expected_adjacent'}
            assert d.pop('depth') == i
            del d['nodes']
            assert d['ids'] == sum(d[f] for f in GAPS) == sum(d[f'tag_{j}'] for j in range(5)), (k, i)
            assert d['groups'] <= d['first'] <= 5 * d['groups'] and d['gap_1'] <= d['ids'] - d['groups']
            assert 2 * d['half_pairs'] <= d['ids'] <= 2 * d['half_pairs'] + d['groups']
            assert d['same_tag_half_pairs'] <= d['half_pairs']
            counts.update({f: x for f, x in d.items() if f != 'max_run'})
            counts['max_run'] = max(d['max_run'], counts['max_run'])
            adj = float(r['expected_adjacent'])
            expected += adj
            depth_rows.append((*k[:2], i, *shares(d, adj)))
        old = known[k[:2]]
        assert counts['ids'] == int(old['scatter_ids']) and int(group[0]['nodes']) == 1 + 4 * counts['groups'] == int(old['nodes']), k
        assert counts['single_bin_ids'] == int(old['single_bin_ids']) and counts['single_bin_groups'] == int(old['single_bin_nodes'])
        all_rows.append((*k[:2], *shares(counts, expected)))
    columns = ('groups ids adjacent_same_pct shuffled_same_pct recent_2_pct_ids recent_4_pct_ids recent_8_pct_ids '
               'single_bin_ids non_single_adjacent_same_pct max_run half_paired_pct_ids half_pairs_same_tag_pct')
    save(out / 'census-by-depth.csv', 'scene n depth ' + columns, depth_rows)
    save(out / 'census-summary.csv', 'scene n ' + columns, all_rows)
    print('Eight untimed fixture rows match staged work and pair checksums. Ordered and unordered scatter reuse matches per depth.')
    print('Gap and tag counts reconcile with every scatter ID; total groups and IDs match the staged routing census.')


def shares(d, expected):
    adj = d['ids'] - d['groups']
    single_adj = d['single_bin_ids'] - d['single_bin_groups']
    recent2 = d['gap_1'] + d['gap_2']
    recent4 = recent2 + d['gap_3_4']
    return (d['groups'], d['ids'], percent(d['gap_1'], adj), percent(expected, adj), percent(recent2, d['ids']),
            percent(recent4, d['ids']), percent(recent4 + d['gap_5_8'], d['ids']), d['single_bin_ids'],
            percent(d['gap_1'] - single_adj, adj - single_adj), d['max_run'],
            percent(2 * d['half_pairs'], d['ids']), percent(d['same_tag_half_pairs'], d['half_pairs']))


def profile(captures, debug, out, ref):
    release = (captures / 'elf-path.txt').read_text().strip()
    text = next(s.split()[0] for s in (STAGED / 'text.sha256').read_text().splitlines() if s.endswith(' candidate.text'))
    assert (captures / 'text.sha256').read_text().split() == [text, 'release.text', text, 'debug.text']
    sites = {int(r['pc'], 16): r for r in read(HERE / 'operands.csv')}
    groups = tuple(dict.fromkeys(r['group'] for r in sites.values()))
    records, pcs = {}, set()
    for spec in read(captures / 'run-order.csv'):
        block, scene, n, engine, event = (spec[f] for f in ('block', 'scene', 'n', 'engine', 'event'))
        name = f'{block}-{scene}-{engine}-{event}'
        row, = read(captures / f'{name}.csv')
        k = key(row)
        assert k == (scene, n, engine) and row['type'] == 'int32', name
        assert (row['status'], row['batch'], row['rounds']) == ('ok', '1', '1') and int(row['reps']) >= 15, name
        RANK['check_outputs'](row, ref[k])
        assert float(row['allocs_per_step']) == 0 and all(row[f] == ref[k][f] for f in ('cold_allocs', 'peak_bytes', 'retained_bytes')), name
        path = captures / f'{name}.samples.gz'
        k = block, scene, n, engine, event
        assert k not in records
        records[k] = list(BASE['chain_samples'](path, release) if event == 'cycles' else PROF['ibs_samples'](path, release))
        pcs.update(pc for *_, stack in records[k] for pc in stack if isinstance(pc, int))
    assert set(records) == {(b, s, '100000', e, v) for b in ('1', '2') for s in SCENES for e in ENGINES for v in ('cycles', 'ibs')}
    frames = BASE['symbolize'](debug, pcs)
    cycle_rows, details, mem = [], [], []
    for k, samples in records.items():
        bperiod, bcount = 0, 0
        counts, periods = Counter(), Counter()
        for *data, stack in samples:
            phase = RANK['phase'](stack, frames)
            if phase in ('other', 'pairs'):
                continue
            pc = stack[0] if stack else None
            if k[-1] == 'cycles':
                period, = data
                bperiod += period
                bcount += 1
                if pc in sites:
                    assert RANK['LOADER']['build_phase'](stack, frames) == 'stable_scatter'
                    counts[pc] += 1
                    periods[pc] += period
            elif pc in sites:
                op, level, address, _ = data
                if op == sites[pc]['op']:
                    if address and op == 'LOAD' and sites[pc]['operand'] != 'tag':
                        assert address % 4 == 0
                    counts[pc, op, level] += 1
        if k[-1] == 'cycles':
            total = periods.total()
            assert counts.total() >= 100 and bcount >= 500, (k, counts)
            for group in (*groups, 'scatter_loop', 'build_total'):
                names = [pc for pc in sites if sites[pc]['group'] == group] if group in groups else sites
                n, p = ((bcount, bperiod) if group == 'build_total' else
                        (sum(counts[pc] for pc in names), sum(periods[pc] for pc in names)))
                cycle_rows.append((*k[:-1], group, n, p, percent(p, bperiod), percent(p, total) if group != 'build_total' else ''))
            for pc, r in sites.items():
                details.append((*k[:-1], hex(pc), r['operand'], counts[pc], periods[pc], percent(periods[pc], total)))
        else:
            assert all(any(p == pc for p, _, _ in counts) for pc in sites if sites[pc]['op'] != '-'), (k, counts)
            for (pc, op, level), number in sorted(counts.items()):
                mem.append((*k[:-1], hex(pc), sites[pc]['operand'], op, level, number))
    prefix = 'block scene n engine '
    save(out / 'cycle-periods.csv', prefix + 'group samples period pct_build_cycles pct_scatter_cycles', cycle_rows)
    save(out / 'cycle-sites.csv', prefix + 'pc operand samples period pct_scatter_cycles', details)
    save(out / 'ibs-sites.csv', prefix + 'pc operand op level samples', mem)
    print('16 Release captures match frozen fixture hashes, pair checksums, work, and footprint; zero warm allocations.')


if __name__ == '__main__':
    captures, debug, count_path, out = map(Path, sys.argv[1:])
    ref = {key(r): r for r in read(STAGED / 'frozen/15-candidate/snapshot.csv')}
    census(count_path, out, ref)
    profile(captures, debug, out, ref)
