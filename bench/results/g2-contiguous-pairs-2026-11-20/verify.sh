#!/usr/bin/env bash
set -euo pipefail
w=$(realpath "$1") out=$(realpath -m "$2")
here=$(dirname "$(realpath "$0")")
r=$(dirname "$here")
mkdir -p "$out"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for k in control candidate scalar; do
    "${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -fno-exceptions -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion \
        -Wshadow -Werror -I"$w/$k" "$here/range-prefix.cpp" -o "$stage/$k"
    "$stage/$k" > "$stage/$k.txt"
done
cmp "$stage/control.txt" "$stage/candidate.txt"
cmp "$stage/control.txt" "$stage/scalar.txt"
printf 'Exact range prefix: %s bytes, SHA-256 ' "$(wc -c < "$stage/control.txt")" > "$out/range-prefix.log"
sha256sum < "$stage/control.txt" | cut -d' ' -f1 >> "$out/range-prefix.log"
bash "$r/g2-cross-screen-2026-11-20/verify-prefix.sh" "$w/control" "$w/candidate" > "$out/unordered-prefix.log"
bash "$r/g2-cross-screen-2026-11-20/verify-prefix.sh" "$w/control" "$w/scalar" > "$out/scalar-prefix.log"
bash "$r/g2-bulk-streams-2026-11-20/verify-order.sh" "$w/control" "$w/candidate" > "$out/dense-order.log"
bash "$r/g2-topdown-build-2026-11-20/verify.sh" "$w/control" "$w/candidate" > "$out/equivalence.log"
ctest --test-dir "$w/candidate-build" --output-on-failure > "$out/tests.log" 2>&1
"$w/candidate-build/bench/quadtree-index-fuzz" > "$out/fuzz.log" 2>&1
