"""Attribute period-weighted cycles using the Release twin's inline frames."""
import csv
import gzip
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PHASES = (
    'input', 'replacement', 'pair_dispatch', 'pair_view', 'checksum',
    'aggregation', 'loop_timer', 'step_unclassified', 'other_exe', 'unresolved',
)


def classify(frames):
    for name, location in frames:
        file, _, line = location.partition(':')
        line = int(line.split()[0]) if line.split() and line.split()[0].isdigit() else 0
        if name.endswith('::acquireModuleIdentity()') or (
            'QuadTree::Index<' in name and '>::build' in name
        ):
            return 'replacement'
        if 'QuadTree::Index<' in name and '>::view(' in name:
            return 'pair_view'
        if file.endswith('/quadtree-bench.cpp') and 436 <= line <= 442:
            return 'checksum'  # PairSink callback, including its inlined mix64.
        if 'LibraryIndex<' in name or 'BruteForce<' in name:
            if '>::build(' in name:
                return 'input'
            if '>::pairs' in name or '>::counters(' in name:
                return 'pair_dispatch'
        if 'QuadTree::Index<' in name and any(
            f'>::{method}' in name for method in ('forEachPair', 'tinyPairs', 'selfPairs', 'pairQualifies', 'testPair')
        ):
            return 'pair_dispatch'
        if file.endswith('/quadtree-bench.cpp') and 675 <= line <= 705:
            if line == 684:
                return 'input'  # Includes the runner's build call boundary.
            if line == 686:
                return 'pair_dispatch'  # Includes the runner's pairs call boundary.
            if line == 699:
                return 'checksum'
            if 692 <= line <= 700:
                return 'aggregation'
            return 'loop_timer'
    if any(('::Runner<' in name and '::step(' in name) or name.startswith('runStep<') for name, _ in frames):
        return 'step_unclassified'
    return 'other_exe' if frames and frames[0][0] != '??' else 'unresolved'


def symbolize(twin, addresses):
    out = subprocess.run(
        ['addr2line', '-aifC', '-e', twin],
        input=''.join(f'{pc:#x}\n' for pc in sorted(addresses)),
        text=True, check=True, stdout=subprocess.PIPE,
    ).stdout.splitlines()
    frames = defaultdict(list)
    lines = iter(out)
    for line in lines:
        if re.fullmatch(r'0x[\da-f]+', line):
            pc = int(line, 16)
        else:
            location = next(lines)
            if location.startswith(str(ROOT) + '/'):
                location = location[len(str(ROOT)) + 1:]
            frames[pc].append((line, '/' + location if not location.startswith('/') else location))
    assert set(frames) == addresses
    return frames


def main(directory, exe):
    files = sorted(directory.glob('[12]-*-*-*.samples'))
    assert len(files) == 32, len(files)
    all_counts = {}
    all_periods = {}
    addresses = set()
    for path in files:
        counts, periods = Counter(), Counter()
        for line in path.read_text().splitlines():
            weight, _, dso = line.split(maxsplit=2)
            assert dso.startswith('(') and dso.endswith(')'), dso
            name, _, off = dso[1:-1].rpartition('+0x')
            if name == exe:
                # The PIE's executable LOAD has equal VMA and file offset.
                key = int(off, 16)
                addresses.add(key)
            else:
                key = Path(name or dso[1:-1]).name
            counts[key] += 1
            periods[key] += int(weight)
        run = tuple(path.stem.split('-'))
        all_counts[run], all_periods[run] = counts, periods
    frames = symbolize(directory / 'quadtree-bench', addresses)
    assignments = {pc: classify(chain) for pc, chain in frames.items()}
    results = {}
    with gzip.open(directory / 'period-by-pc.csv.gz', 'wt', newline='') as f:
        out = csv.writer(f, lineterminator='\n')
        out.writerow(['replicate', 'scene', 'n', 'engine', 'pc_or_dso', 'phase', 'samples', 'period', 'inline_frames'])
        for run, periods in all_periods.items():
            totals = Counter()
            for key, weight in periods.most_common():
                phase = assignments.get(key, 'unresolved')
                totals[phase] += weight
                where = ' <- '.join(f'{name} @ {location}' for name, location in frames.get(key, []))
                out.writerow([*run, hex(key) if isinstance(key, int) else key, phase, all_counts[run][key], weight, where])
            results[run] = totals
    with (directory / 'phase-summary.csv').open('w', newline='') as f:
        out = csv.writer(f, lineterminator='\n')
        out.writerow(['replicate', 'scene', 'n', 'engine', 'ns_per_scene', 'abs_error_ns', 'attributed_pct_process',
                      'samples', *(phase + '_pct_step' for phase in PHASES[:-2])])
        same_results = {}
        for run, totals in results.items():
            with (directory / ('-'.join(run) + '.csv')).open() as source:
                row, = csv.DictReader(line for line in source if not line.startswith('#'))
            assert row['status'] == 'ok' and float(row['allocs_per_step']) == 0
            assert (row['scene'], row['n'], row['engine']) == run[1:] and row['batch'] == '256'
            match = tuple(row[field] for field in ('pairs', 'checksum', 'scene_hash'))
            assert match == same_results.setdefault(run[1:3], match), (run, match)
            step = sum(totals[phase] for phase in PHASES[:-2])
            out.writerow([*run, row['estimate_ns'], row['err_ns'], f'{100 * step / totals.total():.2f}',
                          all_counts[run].total(), *(f'{100 * totals[phase] / step:.2f}' for phase in PHASES[:-2])])


if __name__ == '__main__':
    main(Path(sys.argv[1]), str(Path(sys.argv[2]).resolve()))
