#!/usr/bin/env bash
set -euo pipefail
here=$(dirname "$(realpath "$0")")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
control=$1
shift
for src in "$control" "$@"; do
    name=${src##*/}
    "${CXX:-c++}" -std=c++20 -O3 -DNDEBUG -fno-exceptions -Wall -Wextra -Wpedantic \
        -Wconversion -Wsign-conversion -Wshadow -Werror -I"$src" "$here/dense-order.cpp" -o "$stage/probe"
    "$stage/probe" > "$stage/$name.bin"
    cmp "$stage/${control##*/}.bin" "$stage/$name.bin"
    printf '%s: exact dense slots, topology and callback prefixes; %s bytes, SHA-256 ' "$name" "$(wc -c < "$stage/$name.bin")"
    sha256sum < "$stage/$name.bin" | cut -d' ' -f1
    if [[ $src != "$control" ]]; then
        "${SANITIZER_CXX:-clang++}" -std=c++20 -O1 -g -fno-exceptions -fno-omit-frame-pointer \
            -fsanitize=address,undefined -I"$src" "$here/dense-order.cpp" -o "$stage/probe"
        ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$stage/probe" > "$stage/sanitized.bin"
        cmp "$stage/$name.bin" "$stage/sanitized.bin"
        printf '%s: ASan/UBSan stream matches.\n' "$name"
    fi
done
