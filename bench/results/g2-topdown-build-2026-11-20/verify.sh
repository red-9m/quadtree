#!/usr/bin/env bash
# Compare topology, handles and exact pair identities; never time these binaries.
set -euo pipefail
export LC_ALL=C
control=$(realpath "$1") candidate=$(realpath "$2")
here=$(dirname "$(realpath "$0")")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for kind in control candidate; do
    source=${!kind}
    "${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror \
        -I"$source" -I"$source/bench" "$here/equivalence.cpp" "$source/bench/bench-memory.cpp" -o "$stage/$kind-probe"
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
