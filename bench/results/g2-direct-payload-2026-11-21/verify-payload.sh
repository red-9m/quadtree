#!/usr/bin/env bash
set -euo pipefail
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for kind in control candidate; do
    "${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -fno-exceptions -Wall -Wextra -Wpedantic -Wconversion \
        -Wsign-conversion -Wshadow -Werror -I"$w/$kind" "$here/payload-paths.cpp" -o "$stage/$kind"
    "$stage/$kind" > "$stage/$kind.bin"
done
cmp "$stage/control.bin" "$stage/candidate.bin"
"${SANITIZER_CXX:-clang++}" -std=c++20 -O1 -g -fno-exceptions -fno-omit-frame-pointer -fsanitize=address,undefined \
    -I"$w/candidate" "$here/payload-paths.cpp" -o "$stage/sanitized"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$stage/sanitized" > "$stage/sanitized.bin"
cmp "$stage/control.bin" "$stage/sanitized.bin"
printf '10 payload fixtures: exact handles, dense ids and pair streams, no warmed allocations; rollback verified. GCC/ASan/UBSan SHA-256 '
sha256sum < "$stage/control.bin" | cut -d' ' -f1
