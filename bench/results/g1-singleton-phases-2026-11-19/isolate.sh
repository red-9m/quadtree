#!/usr/bin/env bash
# Run at the repository root. Only the temporary probe headers are modified.
set -eu
export LC_ALL=C
out=$1 cpu=$2 rounds=${3:-9}
mkdir "$out"
out=$(realpath "$out")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
mkdir "$stage/detail"
cp quadtree-index.h "$stage/"
cp detail/quadtree-index-impl.h "$stage/detail/"
sha256sum quadtree-index.h detail/quadtree-index-impl.h bench/results/g1-flat-input-2026-11-19/phase.cpp > "$out/source-inputs.sha256"

# Use fixed, equal work per process. Keep the reference result and allocation checks.
python3 - "$stage" <<'PY'
from pathlib import Path
import sys

stage = Path(sys.argv[1])
phase = Path('bench/results/g1-flat-input-2026-11-19/phase.cpp').read_text()
assert phase.count('end - start < std::chrono::seconds(1)') == 1
(stage / 'phase.cpp').write_text(phase.replace('end - start < std::chrono::seconds(1)', 'repeats < 131072'))
PY

variants=(control prevalidated single-threaded no-extent emplace)
for variant in "${variants[@]}"; do
  python3 - "$stage" "$variant" <<'PY'
from pathlib import Path
import sys

stage, variant = Path(sys.argv[1]), sys.argv[2]
impl = Path('detail/quadtree-index-impl.h')
text = impl.read_text()
start = text.index('    std::optional<Status> buildFlatInteger(')
end = text.index('    template<bool Projected', start)
original = text[start:end]
changes = {
    'control': None,
    'prevalidated': ('        if (!detail::canonicalize(extent))\n            return Status::InvalidGeometry;\n', ''),
    'single-threaded': ('        const auto owner = Traits::acquireIdentity();',
                        '        const auto current = detail::moduleIdentityCounter.load(std::memory_order_relaxed);\n'
                        '        const auto owner = current == std::numeric_limits<std::uint64_t>::max() ? 0 : current + 1;\n'
                        '        if (owner != 0)\n'
                        '            detail::moduleIdentityCounter.store(owner, std::memory_order_relaxed);'),
    'no-extent': ('        mExtent = extent;\n', ''),
    'emplace': ('        mExtent = extent;', '        mExtent.emplace(extent);'),
}
if change := changes[variant]:
    old, new = change
    assert original.count(old) == 1
    text = text[:start] + original.replace(old, new) + text[end:]
(stage / impl).write_text(text)
PY
  "${CXX:-c++}" -std=gnu++20 -O3 -DNDEBUG -g -Wall -Wextra -Wpedantic -Wconversion \
    -Wsign-conversion -Wshadow -I"$stage" -Ibench -I. "$stage/phase.cpp" bench/bench-memory.cpp -o "$stage/$variant"
done

printf 'round,scene,variant,repeats,ns_per_scene,cycles,instructions\n' > "$out/raw.csv"
for ((round=0; round<rounds; ++round)); do
  for scene in uniform clustered; do
    for ((i=0; i<${#variants[@]}; ++i)); do
      variant=${variants[(round+i)%${#variants[@]}]}
      taskset -c "$cpu" perf stat -x, -e cycles:u,instructions:u -o "$stage/counters" -- \
        "$stage/$variant" "$scene" 1 > "$stage/timing"
      awk -v prefix="$((round+1)),$scene,$variant" '
        FNR==NR { ns=$3; repeats=substr($5,2); next }
        /,cycles:u,/ { split($0, row, ","); cycles=row[1] }
        /,instructions:u,/ { split($0, row, ","); instructions=row[1] }
        END {
          if (repeats != 131072 || ns+0 <= 0 || cycles+0 <= 0 || instructions+0 <= 0) exit 1
          print prefix "," repeats "," ns "," cycles "," instructions
        }' "$stage/timing" "$stage/counters" >> "$out/raw.csv"
    done
  done
done
