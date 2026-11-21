#!/usr/bin/env python3
"""Attribute radix Release build cycles and IBS data sources; never time the debug twin."""
import csv
import gzip
import re
import runpy
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
BASE = runpy.run_path(str(HERE.parent / 'g2-phase-profile-2026-11-19/summarize.py'))
IMPL = 'quadtree-index-impl.h'
PHASES = ('adapter', 'capture', 'rank_validation', 'split_classify', 'split_scatter', 'split_other',
          'replacement', 'dense_commit', 'rank_insert', 'topology_commit', 'build_other')
PARTITION = ('split_classify', 'split_scatter', 'split_other')
TRAFFIC = ('load', 'known_load', 'non_l1_load', 'store')
IBS = re.compile(r'^\s*\d+.*?\|OP ([^|]+)\|LVL ([^|]+)\|')
# Operand roles checked in hot-loop.asm on the staged radix .text.
FOCUS = (
    ('split_classify', 'source_id', (0x145905,)),
    ('split_classify', 'descriptor_box', (0x145911, 0x145914, 0x145917, 0x14591b)),
    ('split_classify', 'tag_store', (0x1458ef,)),
    ('split_scatter', 'tag_load', (0x145b00,)),
    ('split_scatter', 'source_id', (0x145b05,)),
    ('split_scatter', 'destination_id', (0x145b26,)),
)

# These source ranges are for the hash-checked radix Release/debug pair only.
RANGES = {
    IMPL: (
        (1541, 1559, 'split_classify'), (1589, 1594, 'split_scatter'), (1522, 1608, 'split_other'),
        (2200, 2214, 'split_other'), (2186, 2193, 'rank_validation'),
        (2167, 2184, 'capture'), (2361, 2383, 'capture'),
        (2226, 2235, 'replacement'), (2427, 2470, 'replacement'),
        (2284, 2285, 'rank_insert'), (2241, 2289, 'dense_commit'), (2413, 2426, 'dense_commit'),
        (1784, 1838, 'topology_commit'), (1731, 1773, 'topology_commit'), (2290, 2300, 'topology_commit'),
        (2095, 2302, 'build_other'), (1120, 1149, 'pairs'),
    ),
    'quadtree-index.h': ((1155, 1165, 'rank_insert'),),
    'bench-index.h': ((40, 64, 'adapter'), (98, 104, 'pairs')),
    'quadtree-bench.cpp': ((684, 684, 'build_other'), (686, 686, 'pairs')),
}


def classify(stack, frames):
    # Inlining puts the innermost frame first. In a recursive partition, an
    # ancestor's split_other line must not override the leaf's source line.
    for pc in stack:
        for _, file, line in frames.get(pc, []):
            for first, last, phase in RANGES.get(file, ()):
                if first <= line <= last:
                    return phase
    return 'other_process'


def ibs_samples(path, release):
    with gzip.open(path, 'rt') as f:
        for record in f.read().strip().split('\n\n'):
            header, *chain = record.strip().splitlines()
            match = IBS.match(header)
            assert match, header
            op, level = match.groups()
            frames = []
            for i, frame in enumerate(chain):
                _, dso = frame.strip().split(maxsplit=1)
                name, sep, off = dso[1:-1].rpartition('+0x')
                if sep and name == release:
                    frames.append(int(off, 16) - bool(i))
                else:
                    frames.append(Path(name or dso[1:-1]).name)
            yield op.upper(), level.strip(), tuple(frames)


def pivot(counter):
    loads = sum(n for (op, _), n in counter.items() if op == 'LOAD')
    known = sum(n for (op, level), n in counter.items() if op == 'LOAD' and level != 'N/A')
    l1 = counter['LOAD', 'L1 hit']
    return {'load': loads, 'known_load': known, 'non_l1_load': known - l1,
            'store': sum(n for (op, _), n in counter.items() if op == 'STORE')}


def percent(a, b):
    return f'{100 * a / b:.2f}' if b else ''


def save(path, columns, rows):
    with path.open('w') as f:
        table = csv.writer(f, lineterminator='\n')
        table.writerow(columns.split())
        table.writerows(rows)


def main(captures, debug, out):
    read = BASE['rows']
    key = BASE['key']
    release = (captures / 'elf-path.txt').read_text().strip()
    previous = HERE.parent / 'g2-cross-screen-2026-11-20/15-control/snapshot.csv'
    ref = {key(r): r for r in read(previous)}
    runs = list(csv.DictReader((captures / 'run-order.csv').open()))
    assert len(runs) == 16
    run_names = set()
    cycles, ibs = {}, {}
    bench, pcs = [], set()
    for r in runs:
        block, scene, n, engine, event = (r[k] for k in ('block', 'scene', 'n', 'engine', 'event'))
        name = f'{block}-{scene}-{engine}-{event}'
        assert name not in run_names, name
        run_names.add(name)
        row, = read(captures / f'{name}.csv')
        assert key(row) == (scene, n, engine) and row['type'] == 'int32' and n == '100000', name
        assert (row['status'], row['rounds'], row['batch']) == ('ok', '1', '1'), name
        assert float(row['allocs_per_step']) == 0 and int(row['reps']) >= 20, name
        for f in (*BASE['EXACT'], 'peak_bytes', 'retained_bytes', 'cold_allocs'):
            assert row[f] == ref[key(row)][f], (name, f)
        bench.append((name, *row.values()))
        samples = captures / f'{name}.samples.gz'
        runner = block, scene, n, engine
        if event == 'cycles':
            cycles[runner] = list(BASE['chain_samples'](samples, release))
            stacks = (stack for _, stack in cycles[runner])
        else:
            assert event == 'ibs'
            ibs[runner] = list(ibs_samples(samples, release))
            stacks = (stack for _, _, stack in ibs[runner])
        pcs.update(pc for stack in stacks for pc in stack if isinstance(pc, int))
    assert len(cycles) == len(ibs) == 8 and cycles.keys() == ibs.keys()
    frames = BASE['symbolize'](debug, pcs)

    attribution, cycle_rows, mem_rows, run_totals, focus_rows = [], [], [], [], []
    with gzip.open(out / 'sites.csv.gz', 'wt', newline='') as f:
        sites = csv.writer(f, lineterminator='\n')
        sites.writerow('block scene n engine phase pc cycle_samples cycles_period ibs_load ibs_known_load '
                       'ibs_non_l1_load ibs_store leaf_inline_frames'.split())
        for runner, samples in cycles.items():
            periods, counts = Counter(), Counter()
            site_periods, site_cycles = Counter(), Counter()
            for period, stack in samples:
                phase = classify(stack, frames)
                leaf = stack[0] if stack else 'unknown'
                periods[phase] += period
                counts[phase] += 1
                site_periods[phase, leaf] += period
                site_cycles[phase, leaf] += 1
            build_cycles = sum(periods[p] for p in PHASES)
            build_samples = sum(counts[p] for p in PHASES)
            assert build_samples >= 500 and counts['pairs'] >= 500, (runner, counts)

            mem, site_mem = defaultdict(Counter), defaultdict(Counter)
            for op, level, stack in ibs[runner]:
                phase = classify(stack, frames)
                leaf = stack[0] if stack else 'unknown'
                mem[phase][op, level] += 1
                site_mem[phase, leaf][op, level] += 1
            build_ibs = sum((mem[p] for p in PHASES), Counter())
            build_mem = pivot(build_ibs)
            assert build_mem['known_load'] > 500 and build_mem['non_l1_load'] > 100, (runner, build_mem)
            run_totals.append((*runner, build_samples, counts.total(), build_cycles, periods.total(),
                               percent(build_cycles, periods.total()), len(ibs[runner]), *build_mem.values(),
                               percent(build_mem['non_l1_load'], build_mem['known_load'])))

            for phase in (*PHASES, 'partition_total', 'pairs', 'other_process'):
                if phase == 'partition_total':
                    phase_mem = sum((mem[p] for p in PARTITION), Counter())
                    phase_cycles = sum(periods[p] for p in PARTITION)
                else:
                    phase_mem = mem[phase]
                    phase_cycles = periods[phase]
                    cycle_rows.append((*runner, phase, counts[phase], phase_cycles))
                    for (op, level), n in sorted(phase_mem.items()):
                        mem_rows.append((*runner, phase, op, level, n))
                if phase not in ('pairs', 'other_process'):
                    detail = pivot(phase_mem)
                    attribution.append((*runner, phase, percent(phase_cycles, build_cycles), *detail.values(),
                                        *(percent(detail[k], build_mem[k]) for k in TRAFFIC),
                                        percent(detail['non_l1_load'], detail['known_load'])))
            for phase, label, offsets in FOCUS:
                detail = pivot(sum((site_mem[phase, pc] for pc in offsets), Counter()))
                assert detail['load'] or detail['store'], (runner, phase, label)
                focus_rows.append((*runner, phase, label, ' '.join(map(hex, offsets)), *detail.values(),
                                   percent(detail['non_l1_load'], build_mem['non_l1_load'])))
            for phase, leaf in sorted((site_periods.keys() | site_mem.keys()), key=lambda k: (k[0], str(k[1]))):
                if phase not in PHASES:
                    continue
                site = phase, leaf
                symbol = ' <- '.join(f'{name} @ {file}:{line}' for name, file, line in frames.get(leaf, ()))
                sites.writerow((*runner, phase, hex(leaf) if isinstance(leaf, int) else leaf, site_cycles[site],
                                site_periods[site], *pivot(site_mem[site]).values(), symbol))
    prefix = 'block scene n engine '
    save(out / 'attribution.csv', prefix + 'phase pct_build_cycles ibs_load ibs_known_load ibs_non_l1_load ibs_store '
         'pct_build_load pct_build_known_load pct_build_non_l1_load pct_build_store pct_phase_known_load_non_l1', attribution)
    save(out / 'cycle-periods.csv', prefix + 'phase cycles_samples cycles_period', cycle_rows)
    save(out / 'ibs-data-sources.csv', prefix + 'phase op level samples', mem_rows)
    save(out / 'run-totals.csv', prefix + 'build_cycle_samples process_cycle_samples build_cycle_period '
         'process_cycle_period build_pct_process ibs_process_samples ibs_build_load ibs_build_known_load '
         'ibs_build_non_l1_load ibs_build_store pct_build_known_load_non_l1', run_totals)
    save(out / 'focus-pcs.csv', prefix + 'phase operand pcs ibs_load ibs_known_load ibs_non_l1_load ibs_store '
         'pct_build_non_l1_load', focus_rows)
    save(out / 'profile-rows.csv', 'run ' + ' '.join(row), bench)
    print('16 Release captures match the staged gate rows: fixtures, pairs, checksums, work, footprint and cold allocations.')
    print('All captures completed with zero warm allocations; build and pair cycle samples and IBS coverage verified.')


if __name__ == '__main__':
    captures, debug, out = sys.argv[1:]
    main(Path(captures), debug, Path(out))
