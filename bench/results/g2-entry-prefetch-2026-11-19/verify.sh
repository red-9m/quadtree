#!/usr/bin/env bash
set -euo pipefail
control=$(realpath "$1") candidate=$(realpath "$2")
here=$(dirname "$(realpath "$0")")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for kind in control candidate; do
    source=${!kind}
    "${CXX:-c++}" -std=gnu++20 -O3 -DNDEBUG -I"$source" -I"$source/bench" \
        "$here/equivalence.cpp" "$source/bench/bench-memory.cpp" -o "$stage/$kind"
done
compare() {
    for kind in control candidate; do
        "$stage/$kind" "$@" > "$stage/$kind.bin"
    done
    cmp "$stage/control.bin" "$stage/candidate.bin"
    printf 'matched %s: ' "$*"
    sha256sum < "$stage/control.bin" | cut -d' ' -f1
}
compare tails
for scene in uniform split-line split-line-translated clustered mixed-size; do
    for n in 10000 100000; do
        compare "$scene" "$n"
    done
done
