#!/usr/bin/env bash
# Instrument a scratch header for comparison counts only, never timing.
set -euo pipefail
export LC_ALL=C
source=$(realpath "$1")
here=$(dirname "$(realpath "$0")")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
(cd "$source" && sha256sum --check --quiet "$here/../g2-grouped-child-screen-2026-11-20/candidate-inputs.sha256")
mkdir "$stage/detail"
cp "$source/quadtree-index.h" "$stage/"
python3 - "$source/detail/quadtree-index-impl.h" "$stage/detail/quadtree-index-impl.h" <<'PY'
from pathlib import Path
import sys

text = Path(sys.argv[1]).read_text()
anchor = 'template<Coordinate T>\nstruct Descriptor'
assert text.count(anchor) == 1
text = text.replace(anchor, '''struct SortCensus
{
    std::uint64_t comparisons[7]{}, calls[7]{}, elements[7]{};
    unsigned phase = 0;
};
inline SortCensus sortCensus;

''' + anchor)
a, b = text.index('    void orderedSelf('), text.index('    void orderedMoved(')
body = text[a:b]
for needle, phase in (
    ('        sortRanks(scratch.mOrder.data(), size());', '1'),
    ('            sortRanks(scratch.mRow.data(), count);',
     'count <= 1 ? 2u : count <= 8 ? 3u : count <= 16 ? 4u : count <= 64 ? 5u : 6u'),
):
    assert body.count(needle) == 1
    indent = needle[:len(needle) - len(needle.lstrip())]
    body = body.replace(needle, indent + 'detail::sortCensus.phase = ' + phase + ';\n' + needle +
                        '\n' + indent + 'detail::sortCensus.phase = 0;')
text = text[:a] + body + text[b:]
old = '''        detail::rankSort(order, count, [this](std::uint32_t a, std::uint32_t b) noexcept
            { return mEntries.mMeta[a].rank < mEntries.mMeta[b].rank; });'''
new = '''        auto& stats = detail::sortCensus;
        const auto phase = stats.phase;
        ++stats.calls[phase];
        stats.elements[phase] += count;
        detail::rankSort(order, count, [this, phase](std::uint32_t a, std::uint32_t b) noexcept
            {
                ++detail::sortCensus.comparisons[phase];
                return mEntries.mMeta[a].rank < mEntries.mMeta[b].rank;
            });'''
assert text.count(old) == 1
Path(sys.argv[2]).write_text(text.replace(old, new))
PY
"${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror \
    -I"$stage" -I"$source/bench" "$here/census.cpp" "$source/bench/bench-memory.cpp" -o "$stage/census"
printf '%s\n' 'scene,bucket,calls,elements,comparisons,pairs,checksum,candidates,node_visits'
for scene in mixed-size split-line-translated; do "$stage/census" "$scene"; done
