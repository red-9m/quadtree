#!/usr/bin/env bash
set -euo pipefail
w=$(realpath "$1") out=$(realpath -m "$2")
root=$(dirname "$(dirname "$(realpath "$0")")")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
mkdir -p "$out"
bash "$root/g2-fused-commit-2026-11-20/verify.sh" "$w" "$out"
for probe in unordered source; do
    if [[ $probe == unordered ]]; then
        test_src="$root/g2-cross-screen-2026-11-20/budget-prefix.cpp"
    else
        test_src="$root/g9-two-scatter-source-radix-2026-11-21/source-order.cpp"
    fi
    for kind in control candidate; do
        "${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -fno-exceptions -fno-access-control \
            -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror \
            -DEXPECT_RADIX=1 -I"$w/$kind" "$test_src" -o "$stage/$kind"
        "$stage/$kind" > "$stage/$kind.bin"
    done
    cmp "$stage/control.bin" "$stage/candidate.bin"
    "${SANITIZER_CXX:-clang++}" -std=c++20 -O1 -g -fno-exceptions -fno-access-control -fno-omit-frame-pointer \
        -fsanitize=address,undefined -DEXPECT_RADIX=1 -I"$w/candidate" "$test_src" -o "$stage/sanitized"
    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$stage/sanitized" > "$stage/sanitized.bin"
    cmp "$stage/control.bin" "$stage/sanitized.bin"
    {
        printf '%s: exact streams match across GCC control/candidate and ASan/UBSan candidate; SHA-256 ' "$probe"
        sha256sum < "$stage/control.bin" | cut -d' ' -f1
    } > "$out/$probe-sanitized.log"
done
