#!/usr/bin/env bash
set -euo pipefail
here=$(dirname "$(realpath "$0")")
control=$(realpath "$1") candidate=$(realpath "$2")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for variant in control candidate; do
    "${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -fno-exceptions -fno-access-control -Wall -Wextra -Wpedantic \
        -Wconversion -Wsign-conversion -Wshadow -Werror -I"${!variant}" "$here/home-lookahead-test.cpp" -o "$stage/$variant"
    "$stage/$variant" > "$stage/$variant.bin"
done
cmp "$stage/control.bin" "$stage/candidate.bin"
"${SANITIZER_CXX:-clang++}" -std=c++20 -O1 -g -fno-exceptions -fno-access-control -fno-omit-frame-pointer \
    -fsanitize=address,undefined -I"$candidate" "$here/home-lookahead-test.cpp" -o "$stage/sanitized"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$stage/sanitized" > "$stage/sanitized.bin"
cmp "$stage/control.bin" "$stage/sanitized.bin"
printf 'Exact hash/tree state, diagnostics, dense slots and ordered callback streams match (GCC and ASan/UBSan): '
sha256sum < "$stage/control.bin" | cut -d' ' -f1
printf 'Nine fixtures: short/tail paths, warm epoch reset, collision conversion during commit, tree-mode rebuild and duplicate rollback.\n'
