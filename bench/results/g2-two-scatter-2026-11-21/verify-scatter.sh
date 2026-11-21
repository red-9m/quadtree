#!/usr/bin/env bash
set -euo pipefail
here=$(dirname "$(realpath "$0")")
control=$(realpath "$1") candidate=$(realpath "$2")
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
for variant in control candidate; do
    "${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -fno-exceptions -fno-access-control -Wall -Wextra -Wpedantic \
        -Wconversion -Wsign-conversion -Wshadow -Werror -I"${!variant}" "$here/scatter-test.cpp" -o "$tmp/$variant"
    printf '%s: ' "$variant"
    "$tmp/$variant"
done
"${SANITIZER_CXX:-clang++}" -std=c++20 -O1 -g -fno-exceptions -fno-access-control -fno-omit-frame-pointer \
    -fsanitize=address,undefined -I"$candidate" "$here/scatter-test.cpp" -o "$tmp/sanitized"
printf 'candidate (ASan/UBSan): '
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$tmp/sanitized"
