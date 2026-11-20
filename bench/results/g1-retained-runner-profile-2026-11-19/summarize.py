"""Attribute retained runner samples and compare costs against a separate G1 run."""
import csv
import gzip
import hashlib
import runpy
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
HELPERS = runpy.run_path(str(HERE.parent / 'g1-runstep-profile-2026-11-19' / 'summarize.py'))
TEXT = '34423ec81fa44ba7b4a0b139799c19be26e916cfd268c072b5e22e4ac19c9807'
BUILD = ('build_dispatch', 'validate_extent', 'identity', 'commit_setup', 'entry_stores', 'publish', 'build_other')
STEP = ('input', 'replacement', 'pair_dispatch', 'pair_result', 'pair_view', 'checksum', 'aggregation', 'loop_timer', 'step_unclassified')
SIGNATURE = ('scene_hash', 'pairs', 'checksum', 'candidates', 'node_visits', 'references', 'max_refs')


def classify(pc, frames):
    # Flat-path saturating inline CAS and result check, verified by .text hash.
    if 0xe3c4e <= pc < 0xe3c84:
        return 'replacement', 'identity'
    # QueryResult copy and adapter Counters construction, including the <2 shortcut.
    if 0xd3442 <= pc < 0xd34c3 or 0xd3508 <= pc < 0xd3515:
        return 'pair_result', ''
    phase = HELPERS['classify'](frames)
    if phase != 'replacement':
        return phase, ''
    for name, where in frames:
        if '>::buildFlatInteger(' in name:
            line = int(where.rpartition(':')[2].split()[0])
            for first, last, sub in ((2005, 2018, 'validate_extent'), (2019, 2022, 'identity'),
                                     (2024, 2027, 'commit_setup'), (2028, 2045, 'entry_stores'),
                                     (2046, 2052, 'publish')):
                if first <= line <= last:
                    return phase, sub
            return phase, 'build_other'
        if '>::buildPrepared<' in name or '>::buildRange<' in name or '>::build<' in name:
            return phase, 'build_dispatch'
    return phase, 'build_other'


def rows(path):
    with path.open() as source:
        return list(csv.DictReader(line for line in source if not line.startswith('#')))


def main(directory, exe):
    assert hashlib.sha256((directory / 'release.text').read_bytes()).hexdigest() == TEXT
    gate = {(r['scene'], r['n'], r['engine']): r for r in rows(directory / '99-control/snapshot.csv')}
    assert len(gate) == 96 and all(r['status'] == 'ok' for r in gate.values())
    files = sorted(directory.glob('*.samples'))
    assert len(files) == 20, len(files)
    sample_runs = {}
    addresses = set()
    for path in files:
        event, scene, n, engine = path.stem.split('-')
        row, = rows(path.with_suffix('.csv'))
        assert row['status'] == 'ok' and row['allocs_per_step'] == '0.000' and row['batch'] == '256'
        assert (row['scene'], row['n'], row['engine']) == (scene, n, engine)
        assert all(row[k] == gate[scene, n, engine][k] for k in SIGNATURE), path
        counts, periods = Counter(), Counter()
        for line in path.read_text().splitlines():
            weight, _, dso = line.split(maxsplit=2)
            assert dso.startswith('(') and dso.endswith(')'), dso
            name, _, off = dso[1:-1].rpartition('+0x')
            if name == exe:
                # The PIE executable LOAD's file offset equals its VMA.
                key = int(off, 16)
                addresses.add(key)
            else:
                key = Path(name or dso[1:-1]).name
            counts[key] += 1
            periods[key] += int(weight)
        sample_runs[event, scene, n, engine] = row, counts, periods

    frames = HELPERS['symbolize'](directory / 'quadtree-bench', addresses)
    assignments = {pc: classify(pc, chain) for pc, chain in frames.items()}
    with (gzip.open(directory / 'period-by-pc.csv.gz', 'wt', newline='') as histogram,
          (directory / 'phase-summary.csv').open('w', newline='') as summary,
          (directory / 'profile-run-results.csv').open('w', newline='') as results):
        pcs = csv.writer(histogram, lineterminator='\n')
        phases = csv.writer(summary, lineterminator='\n')
        runs = csv.writer(results, lineterminator='\n')
        pcs.writerow(['event', 'scene', 'n', 'engine', 'pc_or_dso', 'phase', 'replacement_phase', 'samples', 'period', 'inline_frames'])
        phases.writerow(['event', 'scene', 'n', 'engine', 'samples', 'period_process', 'step_pct_process',
                         *(p + '_pct_step' for p in STEP), *(p + '_pct_replacement' for p in BUILD)])
        runs.writerow(['event', *next(iter(sample_runs.values()))[0]])
        for run, (row, counts, periods) in sample_runs.items():
            totals, build = Counter(), Counter()
            for key, weight in periods.most_common():
                phase, sub = assignments.get(key, ('unresolved', ''))
                totals[phase] += weight
                if sub:
                    build[sub] += weight
                where = ' <- '.join(f'{name} @ {loc}' for name, loc in frames.get(key, []))
                pcs.writerow([*run, hex(key) if isinstance(key, int) else key, phase, sub, counts[key], weight, where])
            step, replacement = sum(totals[p] for p in STEP), totals['replacement']
            assert build.total() == replacement
            phases.writerow([*run, counts.total(), periods.total(), f'{100 * step / periods.total():.2f}',
                             *(f'{100 * totals[p] / step:.2f}' if step else '' for p in STEP),
                             *(f'{100 * build[p] / replacement:.2f}' if replacement else '' for p in BUILD)])
            runs.writerow([run[0], *row.values()])

    with (directory / 'g1-budget.csv').open('w', newline='') as f:
        out = csv.writer(f, lineterminator='\n')
        out.writerow(['scene', 'n', 'index_ns', 'index_error_ns', 'brute_ns', 'brute_error_ns',
                      'limit_ns_from_estimate', 'saving_ns_to_limit_from_estimates'])
        for scene in ('uniform', 'clustered'):
            for n in ('1', '2'):
                i, b = (gate[scene, n, engine] for engine in ('index', 'brute'))
                limit = 2 * float(b['estimate_ns'])
                out.writerow([scene, n, i['estimate_ns'], i['err_ns'], b['estimate_ns'], b['err_ns'],
                              f'{limit:.1f}', f'{float(i["estimate_ns"]) - limit:.1f}'])


if __name__ == '__main__':
    main(Path(sys.argv[1]), str(Path(sys.argv[2]).resolve()))
