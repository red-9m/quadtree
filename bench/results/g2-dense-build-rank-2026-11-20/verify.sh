#!/usr/bin/env bash
# Use the previous exact key/topology/callback probe, exposing the bulk key
# path on the candidate only for its valid input domain (contained, root depth).
set -euo pipefail
export LC_ALL=C
control=$(realpath "$1") candidate=$(realpath "$2")
here=$(dirname "$(realpath "$0")")
probe="$here/../g2-integer-key-2026-11-19/equivalence.cpp"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for kind in control candidate; do
    source=${!kind}
    mkdir -p "$stage/$kind/detail"
    cp "$source/quadtree-index.h" "$stage/$kind/"
    cp "$source/detail/quadtree-index-impl.h" "$stage/$kind/detail/"
    python3 - "$stage/$kind/detail/quadtree-index-impl.h" "$kind" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
text = p.read_text()
needle = '    static std::uint32_t digit('
assert text.count(needle) == 1
fast = ('        if (base == 0 && detail::contains(root, box))\n'
        '            return makeBuildKey(Cell{root}, id, box);\n') if sys.argv[2] == 'candidate' else ''
p.write_text(text.replace(needle, '''public:
    auto profileKey(const Box<T>& root, std::uint32_t base, std::uint32_t id, const Box<T>& box) const noexcept
    {
''' + fast + '''        return makeKey(Cell{root}, base, id, box);
    }

private:
''' + needle))
PY
    "${CXX:-c++}" -std=gnu++20 -O3 -DNDEBUG -I"$stage/$kind" -I"$source/bench" \
        "$probe" "$source/bench/bench-memory.cpp" -o "$stage/$kind-probe"
done
compare() {
    for kind in control candidate; do
        "$stage/$kind-probe" "$@" > "$stage/$kind.bin"
    done
    cmp "$stage/control.bin" "$stage/candidate.bin"
    printf 'matched %s: ' "$*"
    sha256sum < "$stage/control.bin" | cut -d' ' -f1
}
compare extremes
for scene in uniform clustered split-line split-line-translated mixed-size; do
    for n in 1000 10000 100000; do
        compare "$scene" "$n"
    done
done
