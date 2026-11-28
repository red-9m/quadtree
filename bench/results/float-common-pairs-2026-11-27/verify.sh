#!/usr/bin/env bash
# bash verify.sh SRC: Clang index-test in default, scalar (-U__SSE2__ -fno-exceptions) and ASan+UBSan builds.
set -euo pipefail
src=$(realpath "$1")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
cxx=${CXX:-clang++}
common=(-std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -pthread -I"$src")
build() {
    local name=$1; shift
    "$cxx" "${common[@]}" "$@" -c "$src/tests/index-test.cpp" -o "$stage/$name-a.o"
    "$cxx" "${common[@]}" "$@" -DQUADTREE_TEST_DEBUG_HEADER -c "$src/tests/index-test-tu2.cpp" -o "$stage/$name-b.o"
    "$cxx" "${common[@]}" "$@" "$stage/$name-a.o" "$stage/$name-b.o" -o "$stage/$name"
    printf 'Clang index-test (%s)\n' "$name"
    "$stage/$name" | tail -1
}
"$cxx" --version | head -1
build default -O2 -DNDEBUG
build scalar -O2 -DNDEBUG -U__SSE2__ -fno-exceptions
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    build sanitizer -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined
echo ALL VERIFIED
