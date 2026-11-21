#!/usr/bin/env bash
set -euo pipefail
w=$(realpath "$1") out=$(realpath -m "$2")
here=$(dirname "$(realpath "$0")")
r=$(dirname "$here")
mkdir -p "$out"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for k in control mask single; do
    "${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -fno-exceptions -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion \
        -Wshadow -Werror -I"$w/$k" "$here/single-prefix.cpp" -o "$stage/$k"
    "$stage/$k" > "$stage/$k.txt"
    cmp "$stage/control.txt" "$stage/$k.txt"
    printf '%s: exact full/partial child charges, predicates and stops; SHA-256 ' "$k"
    sha256sum < "$stage/$k.txt" | cut -d' ' -f1
done > "$out/single-prefix.log"
bash "$r/g2-grouped-child-screen-2026-11-20/verify-prefix.sh" "$w/control" "$w/single" > "$out/ordered-prefix.log"
bash "$r/g9-cached-row-sort-2026-11-20/verify-dense.sh" "$w/control" "$w/single" > "$out/dense-prefix.log"
bash "$r/g2-bulk-streams-2026-11-20/verify-order.sh" "$w/control" "$w/gather" "$w/mask" "$w/single" > "$out/dense-order.log"
ctest --test-dir "$w/single-build" --output-on-failure > "$out/tests.log" 2>&1
"$w/single-build/bench/quadtree-index-fuzz" > "$out/fuzz.log" 2>&1
