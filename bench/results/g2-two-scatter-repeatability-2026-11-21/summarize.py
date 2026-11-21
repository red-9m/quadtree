#!/usr/bin/env python3
import gzip
import re
import runpy
import sys
from collections import Counter
from pathlib import Path

here = Path(__file__).resolve().parent
base = runpy.run_path(str(here.parent / 'g2-integer-key-2026-11-19/summarize.py'))
read, key, save = (base[name] for name in ('read_rows', 'key', 'save'))
out = Path(sys.argv[1])
reference = {key(row): row for row in read(here.parent / 'g2-two-scatter-2026-11-21/frozen/15-candidate/snapshot.csv')}
phases = ('estimate_ns', 'err_ns', 'prepare_median_ns', 'pairs_median_ns')
work = ('pairs', 'candidates', 'node_visits', 'allocs_per_step')
gate9 = re.compile(r'G9\s+hard snapshot int32 (\S+) n=(\d+) vs unordered \(index\)\s+'
                   r'(\S+) \[(\S+)-(\S+)\]\s+<= (\d+) (pass|FAIL|inconclusive)')
rows2, rows9, gate_counts = [], [], []
for run in ('1', '2'):
    path = out / run
    rows = read(path / 'snapshot.csv')
    data = {key(row): row for row in rows}
    raw = read(path / 'snapshot-raw.csv.gz', gzip.open)
    samples = Counter(map(key, raw))
    assert len(rows) == len(data) == 204 and data.keys() == reference.keys(), run
    assert (path / 'run.exit').read_text().strip() == '0', run
    assert (path / 'gates.exit').read_text().strip() in ('0', '3', '4'), run
    assert samples.keys() <= data.keys(), run
    for k, row in data.items():
        ref = reference[k]
        assert row['type'] == 'int32' and row['status'] == ref['status'] in ('ok', 'capped'), (run, k)
        assert row['rounds'] == ref['rounds'], (run, k)
        assert row['scene_hash'] == ref['scene_hash'] and samples[k] == int(row['reps']), (run, k)
        if row['status'] == 'ok':
            assert row['rounds'] == '15' and all(row[f] == ref[f] for f in base['COUNTS']), (run, k)
    library = [row for row in rows if row['engine'] in ('index', 'index-ordered')]
    assert len(library) == 68 and all(row['status'] == 'ok' and float(row['allocs_per_step']) == 0
                                      for row in library), run
    print(f'Run {run}: {len(raw)} raw samples, 204 rows, 68 warm allocation-free library rows; '
          'fixtures, uncapped checksums, work and footprint agree with the staged reference.')
    counts2, counts9 = Counter(), Counter()
    for line in (path / 'gates.txt').read_text().splitlines():
        if m := base['GATE'].fullmatch(line):
            scene, n, engine, ratio, lo, hi, limit, verdict = m.groups()
            a, b = (data[scene, int(n), e] for e in ('index', engine))
            rows2.append((run, scene, n, engine, *(a[f] for f in phases), *(b[f] for f in phases),
                          ratio, lo, hi, limit, verdict, *(a[f] for f in work)))
            counts2[verdict] += 1
        if m := gate9.fullmatch(line):
            scene, n, ratio, lo, hi, limit, verdict = m.groups()
            a, b = (data[scene, int(n), e] for e in ('index-ordered', 'index'))
            rows9.append((run, scene, n, *(a[f] for f in phases), *(b[f] for f in phases),
                          ratio, lo, hi, limit, verdict, *(a[f] for f in work)))
            counts9[verdict] += 1
        if m := re.fullmatch(r'  (G\d+) hard\s+(\d+)\s+(\d+)\s+(\d+)', line):
            gate_counts.append((run, *m.groups()))
    for gate, counts in (('G2', counts2), ('G9', counts9)):
        assert sum(counts.values()) == 15, (run, gate)
        print(f'Run {run}: {gate} {counts["pass"]}/{counts["FAIL"]}/{counts["inconclusive"]} (pass/fail/inconclusive).')
save(out / 'g2-comparison.csv', 'run scene n baseline index_ns index_error_ns index_build_ns index_pairs_ns '
     'baseline_ns baseline_error_ns baseline_build_ns baseline_pairs_ns ratio ci_low ci_high limit verdict '
     'pairs candidates node_visits allocs_per_step', rows2)
save(out / 'g9-comparison.csv', 'run scene n ordered_ns ordered_error_ns ordered_build_ns ordered_pairs_ns '
     'unordered_ns unordered_error_ns unordered_build_ns unordered_pairs_ns ratio ci_low ci_high limit verdict '
     'pairs candidates node_visits allocs_per_step', rows9)
save(out / 'gate-counts.csv', 'run gate pass fail inconclusive', gate_counts)

summary = (out / 'repeatability-diff.txt').read_text().splitlines()[-1]
m = re.fullmatch(r'compared (\d+): faster (\d+), slower (\d+), same within error (\d+); geometric mean new/old .*', summary)
assert m, summary
n, faster, slower, same = map(int, m.groups())
assert n == faster + slower + same and n > 0, summary
passed = 100 * same >= 95 * n
print(f'Coverage: {same}/{n} ({100 * same / n:.1f}%) within combined error; '
      f'{"meets" if passed else "below"} the 95% target.')

def busy(a, b):
    return 100 * (int(b['busy_jiffies']) - int(a['busy_jiffies'])) / (
        int(b['total_jiffies']) - int(a['total_jiffies']))


def peak_busy(samples):
    return max(busy(a, b) for a, b in zip(samples, samples[1:])
               if float(b['unix_seconds']) - float(a['unix_seconds']) >= 3)


load = read(out / 'load.csv')
boundaries = out / 'run-boundaries.csv'
if boundaries.exists():
    markers = {(row['stage'], row['cpu']): row for row in read(boundaries)}
    actual, accepted = [], []
    for run in ('1', '2'):
        check = read(out / f'quiet-start-{run}.csv')[-1]
        assert check['accepted'] == '1' and all(float(check[k]) <= 5 for k in (
            'host_busy_pct', 'cpu6_busy_pct', 'cpu7_busy_pct')), run
        accepted.append(int(check['tctl_millidegrees']))
        for cpu in ('cpu', 'cpu6', 'cpu7'):
            first, last = (markers[f'{stage}-{run}', cpu] for stage in ('start', 'finish'))
            start, end = (float(row['unix_seconds']) for row in (first, last))
            samples = [row for row in load if row['stage'] == 'monitor' and row['cpu'] == cpu
                       and start < float(row['unix_seconds']) < end]
            print(f'Run {run} {cpu}: {busy(first, last):.2f}% busy; {peak_busy(samples):.2f}% peak sampled interval.')
            if cpu == 'cpu':
                actual.append(int(first['tctl_millidegrees']))
                temps = [int(row['tctl_millidegrees']) / 1000 for row in (first, *samples, last)]
                freq = sorted(int(row['cpu6_scaling_khz']) / 1000 for row in samples)
                print(f'Run {run} Tctl: {temps[0]:.3f} °C start, {min(temps):.1f}–{max(temps):.1f} °C range; '
                      f'CPU 6 sampled frequency {freq[0]:.0f}–{freq[-1]:.0f} MHz.')
    for label, starts in (('Preflight', accepted), ('Run boundary', actual)):
        assert max(starts) <= 40000 and abs(starts[1] - starts[0]) <= 1000, (label, starts)
    print(f'Run boundary Tctl difference: {abs(actual[1] - actual[0]) / 1000:.3f} °C (≤1 °C).')
else:
    for cpu in ('cpu', 'cpu6', 'cpu7'):
        samples = [row for row in load if row['cpu'] == cpu and row['stage'] in ('monitor', 'end')]
        assert samples and samples[-1]['stage'] == 'end', cpu
        print(f'{cpu}: {busy(samples[0], samples[-1]):.2f}% busy across both runs; '
              f'{peak_busy(samples):.2f}% peak sampled interval.')
    temps = [int(row['tctl_millidegrees']) / 1000 for row in load
             if row['cpu'] == 'cpu' and row['stage'] in ('monitor', 'end') and row['tctl_millidegrees']]
    if temps:
        print(f'Package Tctl: {min(temps):.1f}–{max(temps):.1f} °C during the measured block.')
