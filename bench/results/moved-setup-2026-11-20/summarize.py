#!/usr/bin/env python3
import csv
import statistics
import sys
from collections import defaultdict

sizes = (1000, 10000, 100000)
expected = {(n, order, case, moved) for n in sizes for order in ('ordered', 'unordered')
            for case, moved in (('empty', 0), ('single', 1), ('fixed-10', 10), ('one-percent', n // 100))}
expected.update((n, 'na', 'reset', 0) for n in sizes)
positions = {(run, batch) for run in range(1, 4) for batch in range(9)}
groups = defaultdict(dict)
with open(sys.argv[1], newline='') as f:
    for row in csv.DictReader(f):
        key = int(row['n']), row['order'], row['case'], int(row['moved'])
        position = int(row['run']), int(row['batch'])
        elapsed, reps = int(row['total_ns']), int(row['repetitions'])
        assert position not in groups[key] and elapsed > 0 and reps > 0 and row['allocations'] == '0', row
        groups[key][position] = elapsed / reps
assert set(groups) == expected
output = csv.writer(sys.stdout, lineterminator='\n')
output.writerow(('n', 'order', 'case', 'moved', 'batches', 'median_ns', 'min_ns', 'max_ns'))
for key, batches in sorted(groups.items()):
    assert set(batches) == positions, key
    v = batches.values()
    output.writerow((*key, len(v), *(f'{x:.2f}' for x in (statistics.median(v), min(v), max(v)))))
