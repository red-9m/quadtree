#!/usr/bin/env bash
# Run at the repo root. Count calls in a temporary build; do not time it.
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
mkdir "$stage/detail"
cp quadtree-index.h "$stage/"
cp detail/quadtree-index-impl.h "$stage/detail/"
python3 - "$stage/detail/quadtree-index-impl.h" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()
names = 'WalkCharged SelfCharged CrossCalls CrossEmpty CrossCharged CrossBounds CrossFilter CrossPassed CrossChildCalls'
variables = '\n'.join(f'inline std::uint64_t profile{name} = 0;' for name in names.split())
start = 'namespace detail\n{'
assert text.count(start) == 1
text = text.replace(start, start + '\n' + variables + '\n')


def instrument(begin, end, edits):
    global text
    i = text.index(begin)
    j = text.index(end, i + len(begin))
    body = text[i:j]
    for before, after in edits:
        assert body.count(before) == 1, before
        body = body.replace(before, after)
    text = text[:i] + body + text[j:]


instrument('    void walk(', '    void scan(', [
    ('        if (!detail::closedOverlap(box, node.bounds))',
     '        ++detail::profileWalkCharged;\n        if (!detail::closedOverlap(box, node.bounds))'),
])
instrument('    void selfPairs(', '    bool crossQualifies(', [
    ('        if (!detail::compatible(node.summary, node.summary))',
     '        ++detail::profileSelfCharged;\n        if (!detail::compatible(node.summary, node.summary))'),
])
instrument('    void crossPairs(', '    void selfPairs(', [
    ('''        if (!context.active() || left.count == 0 || right.count == 0 || !context.node())
            return;
        if (!detail::closedOverlap(left.bounds, right.bounds))
            return;''', '''        ++detail::profileCrossCalls;
        if (!context.active())
            return;
        if (left.count == 0 || right.count == 0)
        {
            ++detail::profileCrossEmpty;
            return;
        }
        if (!context.node())
            return;
        ++detail::profileCrossCharged;
        if (!detail::closedOverlap(left.bounds, right.bounds))
        {
            ++detail::profileCrossBounds;
            return;
        }'''),
    ('            context.filtered();', '            ++detail::profileCrossFilter;\n            context.filtered();'),
    ('        for (std::uint32_t i = left.head;',
     '        ++detail::profileCrossPassed;\n        for (std::uint32_t i = left.head;'),
    ('                    crossPairs(left.firstChild + q, right.firstChild + r, context, visitor, predicate);',
     '''                {
                    ++detail::profileCrossChildCalls;
                    crossPairs(left.firstChild + q, right.firstChild + r, context, visitor, predicate);
                }'''),
])
path.write_text(text)
PY
"${CXX:-c++}" -std=gnu++20 -O3 -DNDEBUG -I"$stage" -Ibench "$here/census.cpp" bench/bench-memory.cpp -o "$stage/census"
{
    printf 'scene,n,pairs,checksum,candidates,node_visits,self_charged,walk_charged,cross_calls,cross_empty,cross_charged,cross_bounds_rejected,cross_filter_rejected,cross_passed,cross_recursive_calls\n'
    for scene in uniform split-line split-line-translated; do
        for n in 10000 100000; do
            "$stage/census" "$scene" "$n"
        done
    done
} > "$stage/census.csv"
python3 - "$stage/census.csv" "$here/../g2-phase-profile-2026-11-19/baseline/snapshot.csv" <<'PY'
import csv
from pathlib import Path
import sys

census, baseline = map(Path, sys.argv[1:])
with baseline.open() as f:
    reference = {(r['scene'], r['n']): r for r in csv.DictReader(line for line in f if not line.startswith('#'))
                 if r['engine'] == 'index'}
with census.open() as f:
    rows = list(csv.DictReader(f))
assert len(rows) == 6
for row in rows:
    ref = reference[row['scene'], row['n']]
    assert all(row[k] == ref[k] for k in ('pairs', 'checksum', 'candidates', 'node_visits')), row
print(census.read_text(), end='')
PY
