#!/usr/bin/env bash
set -euo pipefail
here=$(dirname "$(realpath "$0")")
src=$(realpath "$1")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
common=(-std=c++20 -fno-exceptions -fno-access-control -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -I"$src")
for mode in packed scalar; do
    flags=(); [[ $mode != scalar ]] || flags=(-U__SSE2__)
    "${CXX:-c++}" "${common[@]}" -O3 -DNDEBUG "${flags[@]}" "$here/bounds.cpp" -o "$stage/probe"
    printf '%s: ' "$mode"
    "$stage/probe"
done
"${SANITIZER_CXX:-clang++}" "${common[@]}" -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined "$here/bounds.cpp" -o "$stage/probe"
printf 'ASan/UBSan: '
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$stage/probe"
