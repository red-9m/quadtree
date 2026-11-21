#!/usr/bin/env bash
# Compare long-row and fallback results; never time these binaries.
set -euo pipefail
control=$(realpath "$1") candidate=$(realpath "$2")
here=$(dirname "$(realpath "$0")")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for kind in control candidate; do
    src=${!kind}
    "${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -fno-exceptions -Wall -Wextra -Wpedantic \
        -Wconversion -Wsign-conversion -Wshadow -Werror -I"$src" "$here/dense-prefix.cpp" -o "$stage/$kind"
    "$stage/$kind" > "$stage/$kind.bin"
done
cmp "$stage/control.bin" "$stage/candidate.bin"
printf 'Exact long-row streams match: %s bytes, SHA-256 ' "$(wc -c < "$stage/control.bin")"
sha256sum < "$stage/control.bin" | cut -d' ' -f1
