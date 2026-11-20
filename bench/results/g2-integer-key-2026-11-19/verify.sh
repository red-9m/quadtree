#!/usr/bin/env bash
# Compare raw key, topology, dense payload and pair streams. Never time these binaries.
set -euo pipefail
export LC_ALL=C
control=$(realpath "$1") candidate=$(realpath "$2")
here=$(dirname "$(realpath "$0")")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for kind in control candidate; do
    source=${!kind}
    mkdir -p "$stage/$kind/detail"
    cp "$source/quadtree-index.h" "$stage/$kind/"
    cp "$source/detail/quadtree-index-impl.h" "$stage/$kind/detail/"
    python3 - "$stage/$kind/detail/quadtree-index-impl.h" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
text = p.read_text()
needle = '    static std::uint32_t digit('
assert text.count(needle) == 1
p.write_text(text.replace(needle, '''public:
    auto profileKey(const Box<T>& root, std::uint32_t base, std::uint32_t id, const Box<T>& box) const noexcept
    {
        return makeKey(Cell{root}, base, id, box);
    }

private:
''' + needle))
PY
    "${CXX:-c++}" -std=gnu++20 -O3 -DNDEBUG -I"$stage/$kind" -I"$source/bench" \
        "$here/equivalence.cpp" "$source/bench/bench-memory.cpp" -o "$stage/$kind-probe"
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
