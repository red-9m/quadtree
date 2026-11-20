#!/usr/bin/env python3
"""Split retained bulk-entry cycle samples and precise Zen 3 IBS op samples."""
import csv
import gzip
import re
import runpy
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
PHASE = HERE.parent / 'g2-phase-profile-2026-11-19'
BASE = runpy.run_path(str(PHASE / 'summarize.py'))
SCENES = ('uniform', 'split-line', 'split-line-translated', 'clustered')
OPERATIONS = {
    'key_load': (0xe3780,),
    'descriptor_load': (0xe3791, 0x13d008, 0x13d01a, 0x13d02b, 0x13d035),
    'payload_load': (0x13d00c,),
    'dense_store': (0x13d013, 0x13d026, 0x13d044, 0x13d048, 0x13d04b, 0x13d04f, 0x13d056, 0x13d05d),
    'counts_rank_test_key_rewrite': (0xe37b4, 0xe37bc, 0xe37c4, 0xe37c9, 0xe37cf),
}
BY_PC = {pc: op for op, pcs in OPERATIONS.items() for pc in pcs}
# Skid / dependence sites, reported at their actual PCs rather than rebinned as loads.
CONTINUATIONS = {0xe3795: 'after_rank_flag_load', 0x13d00f: 'after_payload_load'}
BUCKETS = (*OPERATIONS, *CONTINUATIONS.values(), 'loop_helper_overhead', 'other_entry_commit')
LEVELS = ('L1 hit', 'L2 hit', 'L3 hit', 'core, same node Any cache hit', 'RAM hit')


def hot_path(pc):
    return isinstance(pc, int) and (0xe375d <= pc <= 0xe37da or 0x13d000 <= pc <= 0x13d061)


def bucket(pc):
    if not hot_path(pc):
        return 'other_entry_commit'
    return BY_PC.get(pc, CONTINUATIONS.get(pc, 'loop_helper_overhead'))


def writer(file):
    return csv.writer(file, lineterminator='\n')


def percent(part, whole):
    return f'{100 * part / whole:.2f}'


def verify(name, path, reference):
    row, = BASE['rows'](path)
    scene, size = name.split('-', 1)[1].rsplit('-', 1)
    assert (row['model'], row['scene'], row['n'], row['type'], row['engine'], row['batch'], row['status'],
            row['rounds'], row['allocs_per_step']) == (
        'snapshot', scene, size, 'int32', 'index', '1', 'ok', '1', '0.000'), name
    assert int(row['reps']) >= 20, name
    assert all(row[k] == reference[row['scene']][k] for k in (*BASE['EXACT'], 'peak_bytes', 'retained_bytes')), name
    return row


def cycles(captures, release, debug, names, reference, out):
    traces = {}
    for name in names:
        verify(name, captures / f'{name}.csv', reference)
        traces[name] = list(BASE['chain_samples'](captures / f'{name}.samples.gz', release))
    pcs = {pc for samples in traces.values() for _, stack in samples for pc in stack if isinstance(pc, int)}
    frames = BASE['symbolize'](debug, pcs)
    with (out / 'cycle-summary.csv').open('w') as f, (out / 'cycle-sites.csv').open('w') as g:
        summary, sites = writer(f), writer(g)
        summary.writerow(('run', 'commit_samples', 'commit_period', 'commit_pct_build',
                          *(p + '_pct_commit' for p in BUCKETS)))
        sites.writerow(('run', 'pc', 'bucket', 'samples', 'period', 'pct_commit', 'caller_pc', 'inline_location'))
        for name, samples in traces.items():
            counts, periods, phases = Counter(), Counter(), Counter()
            for period, stack in samples:
                phase = BASE['classify'](stack, frames)
                phases[phase] += period
                if phase == 'entry_commit':
                    pc, caller = (*stack, 'none')[:2]
                    counts[pc, caller] += 1
                    periods[pc, caller] += period
                    if isinstance(pc, int) and 0x13d000 <= pc <= 0x13d061:
                        assert caller == 0xe379f, (name, hex(pc), hex(caller))
            total = periods.total()
            assert counts.total() > 100 and total == phases['entry_commit'], name
            groups = Counter()
            for (pc, caller), period in sorted(periods.items(), key=lambda item: -item[1]):
                group = bucket(pc)
                groups[group] += period
                location = ' <- '.join(f'{file}:{line}' for _, file, line in frames.get(pc, ()))
                sites.writerow((name, hex(pc) if isinstance(pc, int) else pc, group, counts[pc, caller],
                                period, percent(period, total), hex(caller) if isinstance(caller, int) else caller,
                                location))
            summary.writerow((name, counts.total(), total, percent(total, sum(phases[p] for p in BASE['BUILD'])),
                              *(percent(groups[p], total) for p in BUCKETS)))


def ibs(captures, release, names, reference, out):
    # DSO offsets are stable across PIE mappings. Drop other PCs and virtual addresses.
    record = re.compile(r'\|OP ([^|]+)\|LVL ([^|]+)\|.*?\s(\d+)\s+[0-9a-f]+\s+\('
                        + re.escape(release) + r'\+0x([0-9a-f]+)\)\s*$')
    with (out / 'ibs-summary.csv').open('w') as f, (out / 'ibs-run-results.csv').open('w') as g, \
         gzip.open(out / 'ibs-commit-ops.csv.gz', 'wt') as h:
        summary, results, ops = map(writer, (f, g, h))
        summary.writerow(('run', 'site', 'op', 'samples', 'l1_hit', 'l2_hit', 'l3_hit', 'same_node_cache_hit',
                          'ram_hit', 'other_level', 'miss_latency_samples', 'miss_latency_median_cycles',
                          'miss_latency_p90_cycles'))
        ops.writerow(('run', 'pc', 'site', 'op', 'level', 'reported_miss_latency_cycles'))
        for name in names:
            path = captures / f'{name}.csv'
            assert '# energy preference: performance\n' in path.read_text(), name
            row = verify(name, path, reference)
            if name == names[0]:
                results.writerow(('run', *row))
            results.writerow((name, *row.values()))
            levels, weights = defaultdict(Counter), defaultdict(list)
            with gzip.open(captures / f'{name}.ibs.gz', 'rt') as samples:
                for line in samples:
                    m = record.search(line)
                    if not m:
                        continue
                    op, level, weight, pc = m.groups()
                    pc, weight = int(pc, 16), int(weight)
                    if not hot_path(pc):
                        continue
                    group = bucket(pc)
                    ops.writerow((name, hex(pc), group, op, level, weight))
                    levels[group, op][level] += 1
                    if weight:
                        weights[group, op].append(weight)
            for group in ('payload_load', 'descriptor_load'):
                assert levels[group, 'LOAD'].total() > 100, (name, group)
            for (group, op), level in sorted(levels.items()):
                known = tuple(level[label] for label in LEVELS)
                hist = sorted(weights[group, op])
                summary.writerow((name, group, op, level.total(), *known, level.total() - sum(known), len(hist),
                                  hist[(len(hist) - 1) // 2] if hist else '',
                                  hist[(len(hist) * 9 - 1) // 10] if hist else ''))


def main(cycle_dir, ibs_dir, cycle_dso, debug, out):
    names = [f'{block}-{scene}-100000' for block in (1, 2) for scene in SCENES]
    reference = {r['scene']: r for r in BASE['rows'](PHASE / 'baseline/snapshot.csv')
                 if r['n'] == '100000' and r['engine'] == 'index'}
    ibs_dso, = [line.removeprefix('DSO: ') for line in (ibs_dir / 'environment.txt').read_text().splitlines()
                if line.startswith('DSO: ')]
    cycles(cycle_dir, cycle_dso, debug, names, reference, out)
    ibs(ibs_dir, ibs_dso, names, reference, out)
    print('8 archived cycle profiles and 8 new IBS profiles match retained fixtures, work, pair counts, checksums '
          'and footprint with zero warm allocations.')


if __name__ == '__main__':
    cycle_dir, ibs_dir, cycle_dso, debug, out = sys.argv[1:]
    main(Path(cycle_dir), Path(ibs_dir), cycle_dso, debug, Path(out))
