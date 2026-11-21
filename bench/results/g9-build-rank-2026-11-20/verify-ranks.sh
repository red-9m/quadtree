#!/usr/bin/env bash
set -euo pipefail
if (( $# == 0 )); then
    printf 'Usage: %s SOURCE [SOURCE ...]\n' "$0" >&2
    exit 1
fi
here=$(dirname "$(realpath "$0")")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for src in "$@"; do
    label=${src##*/}
    "${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -fno-exceptions -Wall -Wextra -Wpedantic \
        -Wconversion -Wsign-conversion -Wshadow -Werror -I"$src" "$here/build-ranks-test.cpp" -o "$stage/probe"
    printf '%s: ' "$label"
    "$stage/probe"
done
src=${!#}
"${SANITIZER_CXX:-clang++}" -std=c++20 -O1 -g -fno-exceptions -fno-omit-frame-pointer \
    -fsanitize=address,undefined -I"$src" "$here/build-ranks-test.cpp" -o "$stage/sanitized"
printf '%s (ASan/UBSan): ' "${src##*/}"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$stage/sanitized"
