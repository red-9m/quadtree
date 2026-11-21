#!/usr/bin/env bash
set -euo pipefail
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for kind in control candidate; do
    "${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -fno-exceptions -fno-access-control -Wall -Wextra -Wpedantic -Wconversion \
        -Wsign-conversion -Wshadow -Werror -I"$w/$kind" "$here/leaf-state.cpp" -o "$stage/$kind"
    "$stage/$kind" > "$stage/$kind.bin"
done
cmp "$stage/control.bin" "$stage/candidate.bin"
"${SANITIZER_CXX:-clang++}" -std=c++20 -O1 -g -fno-exceptions -fno-access-control -fno-omit-frame-pointer -fsanitize=address,undefined \
    -I"$w/candidate" "$here/leaf-state.cpp" -o "$stage/sanitized"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$stage/sanitized" > "$stage/sanitized.bin"
cmp "$stage/control.bin" "$stage/sanitized.bin"
printf 'Exact node fields, references and pair streams before/after mutation and rebuild, including empty and retry leaves. GCC/ASan/UBSan SHA-256 '
sha256sum < "$stage/control.bin" | cut -d' ' -f1
