#!/usr/bin/env bash
# Stage an instrumented header for work counts only; never time this build.
set -euo pipefail
export LC_ALL=C
source=$(realpath "$1")
here=$(dirname "$(realpath "$0")")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
(cd "$source" && sha256sum --check --quiet "$here/../g2-topdown-build-2026-11-20/candidate-inputs.sha256")
mkdir "$stage/detail"
cp "$source/quadtree-index.h" "$stage/"
python3 - "$source/detail/quadtree-index-impl.h" "$stage/detail/quadtree-index-impl.h" <<'PY'
from pathlib import Path
import sys

text = Path(sys.argv[1]).read_text()
pos = text.index('template<Coordinate T>\nstruct Descriptor')
text = text[:pos] + '''struct SnapshotCensus {
    std::uint64_t walkEmpty{}, walkDisjoint{}, walkFilter{}, walkAdmitted{};
    std::uint64_t crossEmpty{}, crossDisjoint{}, crossFilter{}, crossAdmitted{};
    std::uint64_t rankChecks{}, rankRejects{}, rows[6]{}, partners[6]{};
    std::size_t maxRow{};
};
inline SnapshotCensus snapshotCensus;

''' + text[pos:]


def annotate(start, end, changes):
    global text
    a = text.index(start)
    b = text.index(end, a + 1)
    body = text[a:b]
    for original, replacement in changes:
        assert body.count(original) == 1, original
        body = body.replace(original, replacement)
    text = text[:a] + body + text[b:]


for key, first, last, empty, overlap, compatible, callback in (
    ('walk', '    void walk(', '    void scan(', 'node.count == 0',
     'box, node.bounds', 'queryFilter && !detail::compatible(*queryFilter, node.summary)',
     '        for (std::uint32_t i = node.head'),
    ('cross', '    void crossPairs(', '    void selfPairs(', 'left.count == 0 || right.count == 0',
     'left.bounds, right.bounds', '!detail::compatible(left.summary, right.summary)',
     '        for (std::uint32_t i = left.head'),
):
    stat = 'detail::snapshotCensus.' + key
    annotate(first, last, [
        (f'        if (!context.active() || {empty} || !context.node())\n            return;',
         f'        if ({empty}) ++{stat}Empty;\n'
         f'        if (!context.active() || {empty} || !context.node())\n            return;'),
        (f'        if (!detail::closedOverlap({overlap}))\n            return;',
         f'        if (!detail::closedOverlap({overlap}))\n'
         f'        {{ ++{stat}Disjoint; return; }}'),
        (f'        if ({compatible})\n        {{',
         f'        if ({compatible})\n        {{\n            ++{stat}Filter;'),
        (callback, f'        ++{stat}Admitted;\n' + callback),
    ])
annotate('    void orderedSelf(', '    void orderedMoved(', [
    ('                if (mEntries.mMeta[b].rank <=',
     '                ++detail::snapshotCensus.rankChecks;\n                if (mEntries.mMeta[b].rank <='),
    ('                    return;\n                if (pairQualifies',
     '                { ++detail::snapshotCensus.rankRejects; return; }\n                if (pairQualifies'),
    ('            sortRanks(scratch.mRow.data(), count);', '''            auto& stats = detail::snapshotCensus;
            const auto bin = count <= 1 ? count : count <= 8 ? 2u : count <= 16 ? 3u : count <= 64 ? 4u : 5u;
            ++stats.rows[bin];
            stats.partners[bin] += count;
            stats.maxRow = std::max(stats.maxRow, count);
            sortRanks(scratch.mRow.data(), count);'''),
])
Path(sys.argv[2]).write_text(text)
PY
"${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror \
    -I"$stage" -I"$source/bench" "$here/census.cpp" "$source/bench/bench-memory.cpp" -o "$stage/census"
printf '%s\n' 'scene,engine,pairs,checksum,candidates,node_visits,walk_empty,walk_disjoint,walk_filter,walk_admitted,cross_empty,cross_disjoint,cross_filter,cross_admitted,rank_checks,rank_rejects,rows_0,rows_1,rows_2_8,rows_9_16,rows_17_64,rows_65_plus,partners_0,partners_1,partners_2_8,partners_9_16,partners_17_64,partners_65_plus,max_row'
for scene in split-line split-line-translated mixed-size; do "$stage/census" index-ordered "$scene"; done
for scene in uniform split-line-translated; do "$stage/census" index "$scene"; done
