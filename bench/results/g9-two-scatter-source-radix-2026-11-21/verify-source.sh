#!/usr/bin/env bash
set -euo pipefail
here=$(dirname "$(realpath "$0")")
control=$(realpath "$1") candidate=$(realpath "$2")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
for kind in control candidate; do
    flag=0
    [[ $kind != candidate ]] || flag=1
    "${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -fno-exceptions -fno-access-control -Wall -Wextra -Wpedantic \
        -Wconversion -Wsign-conversion -Wshadow -Werror -DEXPECT_RADIX="$flag" -I"${!kind}" \
        "$here/source-order.cpp" -o "$stage/$kind"
    "$stage/$kind" > "$stage/$kind.bin"
done
cmp "$stage/control.bin" "$stage/candidate.bin"
"${SANITIZER_CXX:-clang++}" -std=c++20 -O1 -g -fno-exceptions -fno-access-control -fno-omit-frame-pointer \
    -fsanitize=address,undefined -DEXPECT_RADIX=1 -I"$candidate" "$here/source-order.cpp" -o "$stage/sanitized"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$stage/sanitized" > "$stage/sanitized.bin"
cmp "$stage/control.bin" "$stage/sanitized.bin"
printf 'Full-width source orders, predicate/visitor streams, statuses and counters match under GCC and ASan/UBSan: '
sha256sum < "$stage/control.bin" | cut -d' ' -f1
printf '4095/4096/4097; full, zero and each independently undersized scratch buffer; finite budgets, stop and reuse; no query allocations.\n'
