"""Split replacement in the actual G1 Runner using inline frames and one PC range."""
import csv
import gzip
import runpy
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
RUNSTEP = HERE.parent / 'g1-runstep-profile-2026-11-19'
PHASES = (
    'build_range', 'prepared_dispatch', 'validate_extent', 'identity_atomic', 'identity_boundary',
    'commit_setup', 'entry_stores', 'publish', 'replacement_other',
)


def split(pc, frames):
    # Verified by .text hash + disassembly: spills, CAS call, status and reloads.
    if 0xe3cfb <= pc < 0xe3d44:
        return 'identity_boundary'
    for name, location in frames:
        if name.endswith('::acquireModuleIdentity()'):
            return 'identity_atomic'
        if '>::buildFlatInteger(' in name:
            line = int(location.rpartition(':')[2].split()[0])
            if 2004 <= line <= 2018:
                return 'validate_extent'
            if 2019 <= line <= 2023:
                return 'identity_boundary'
            if 2024 <= line <= 2027:
                return 'commit_setup'
            if 2028 <= line <= 2045:
                return 'entry_stores'
            if 2046 <= line <= 2052:
                return 'publish'
            return 'replacement_other'
        if '>::buildPrepared<' in name:
            return 'prepared_dispatch'
        if '>::buildRange<' in name or '>::build<' in name:
            return 'build_range'
    return 'replacement_other'


def fresh_rows(directory, exe, twin):
    helpers = runpy.run_path(str(RUNSTEP / 'summarize.py'))
    samples = {}
    addresses = set()
    results = {}
    for path in sorted(directory.glob('*.samples')):
        run = tuple(path.stem.split('-'))
        counts, periods = Counter(), Counter()
        for line in path.read_text().splitlines():
            weight, _, dso = line.split(maxsplit=2)
            name, _, offset = dso[1:-1].rpartition('+0x')
            if name == exe:
                key = int(offset, 16)
                addresses.add(key)
            else:
                key = Path(name or dso[1:-1]).name
            counts[key] += 1
            periods[key] += int(weight)
        samples[run] = counts, periods
        with path.with_suffix('.csv').open() as f:
            row, = csv.DictReader(line for line in f if not line.startswith('#'))
        assert row['status'] == 'ok' and float(row['allocs_per_step']) == 0
        assert (row['scene'], row['n']) == run[1:] and row['batch'] == '256' and row['engine'] == 'index'
        results[run] = row
    assert len(samples) == 8
    frames = helpers['symbolize'](twin, addresses)
    assignments = {pc: helpers['classify'](chain) for pc, chain in frames.items()}
    for run, (counts, periods) in samples.items():
        for key, weight in periods.items():
            yield ('fresh', *run), key, assignments.get(key, 'unresolved'), frames.get(key, []), counts[key], weight
    with (directory / 'profile-run-results.csv').open('w', newline='') as f:
        out = csv.writer(f, lineterminator='\n')
        out.writerow(['event', *next(iter(results.values()))])
        baseline = {}
        for run, row in results.items():
            signature = tuple(row[field] for field in ('pairs', 'checksum', 'scene_hash', 'candidates', 'node_visits'))
            assert signature == baseline.setdefault(run[1:], signature), run
            out.writerow([run[0], *row.values()])


def archived_rows():
    with gzip.open(RUNSTEP / 'period-by-pc.csv.gz', 'rt') as f:
        for row in csv.DictReader(f):
            if row['engine'] != 'index' or row['n'] not in ('2', '4'):
                continue
            where = [item.rsplit(' @ ', 1) for item in row['inline_frames'].split(' <- ')] if row['inline_frames'] else []
            pc = row['pc_or_dso']
            key = int(pc, 16) if pc.startswith('0x') else pc
            yield ('prior-' + row['replicate'], 'cycles', row['scene'], row['n']), key, row['phase'], where, int(row['samples']), int(row['period'])


def main(directory, exe, twin):
    totals = {}
    with gzip.open(directory / 'replacement-by-pc.csv.gz', 'wt', newline='') as f:
        out = csv.writer(f, lineterminator='\n')
        out.writerow(['run', 'event', 'scene', 'n', 'pc', 'phase', 'samples', 'period', 'inline_frames'])
        for rows in (archived_rows(), fresh_rows(directory, exe, twin)):
            for run, key, phase, frames, count, weight in rows:
                total = totals.setdefault(run, Counter())
                if phase == 'replacement':
                    subphase = split(key, frames)
                    total[subphase] += weight
                    total['replacement'] += weight
                    if key == 0xe3d3f:
                        total['reload'] += weight
                    out.writerow([*run, hex(key), subphase, count, weight,
                                  ' <- '.join(f'{name} @ {location}' for name, location in frames)])
                if phase not in ('other_exe', 'unresolved'):
                    total['step'] += weight
                total['process'] += weight
    with (directory / 'phase-summary.csv').open('w', newline='') as f:
        out = csv.writer(f, lineterminator='\n')
        out.writerow(['run', 'event', 'scene', 'n', 'step_pct_process', 'replacement_pct_step',
                      *(p + '_pct_replacement' for p in PHASES), 'reload_pc_pct_step'])
        for run, total in sorted(totals.items()):
            assert sum(total[p] for p in PHASES) == total['replacement']
            assert total['replacement_other'] < total['replacement'] * 0.10, (run, total)
            out.writerow([*run, f'{100 * total["step"] / total["process"]:.2f}',
                          f'{100 * total["replacement"] / total["step"]:.2f}',
                          *(f'{100 * total[p] / total["replacement"]:.2f}' for p in PHASES),
                          f'{100 * total["reload"] / total["step"]:.2f}'])


if __name__ == '__main__':
    main(Path(sys.argv[1]), str(Path(sys.argv[2]).resolve()), Path(sys.argv[3]))
