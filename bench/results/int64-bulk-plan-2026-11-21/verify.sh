#!/usr/bin/env bash
# Untimed control/candidate comparison for int32 and int64: topology, handles, rects and exact pair sets.
# usage: verify.sh WORK OUT   (WORK/control, WORK/candidate source trees)
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1") out=$(realpath -m "$2")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
mkdir -p "$out"
flags=(-std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror)
for t in int32 int64; do
    for kind in control candidate; do
        "${CXX:-c++}" "${flags[@]}" -DPROBE_T="std::${t}_t" -I"$w/$kind" -I"$w/$kind/bench" "$here/equivalence.cpp" \
            "$w/$kind/bench/bench-memory.cpp" -o "$stage/eq-$t-$kind"
        "${CXX:-c++}" "${flags[@]}" -fno-exceptions -DPROBE_T="std::${t}_t" -I"$w/$kind" "$here/dense-order.cpp" \
            -o "$stage/dense-$t-$kind"
    done
    {
        for args in extremes "uniform 1000" "uniform 10000" "uniform 100000" "clustered 1000" "clustered 100000" \
            "split-line 1000" "split-line 100000" "split-line-translated 1000" "split-line-translated 100000" \
            "mixed-size 1000" "mixed-size 100000"; do
            for kind in control candidate; do "$stage/eq-$t-$kind" $args > "$stage/$kind.bin"; done
            cmp "$stage/control.bin" "$stage/candidate.bin"
            printf '%s equivalence %s: match %s\n' "$t" "$args" "$(sha256sum < "$stage/control.bin" | cut -c1-16)"
        done
        for kind in control candidate; do "$stage/dense-$t-$kind" > "$stage/$kind.bin"; done
        if cmp -s "$stage/control.bin" "$stage/candidate.bin"; then
            printf '%s dense-order: exact match %s\n' "$t" "$(sha256sum < "$stage/control.bin" | cut -c1-16)"
        else
            printf '%s dense-order: DIFFERENT (%s vs %s bytes)\n' "$t" "$(wc -c < "$stage/control.bin")" "$(wc -c < "$stage/candidate.bin")"
        fi
    } | tee "$out/verify-$t.log"
done
"${SANITIZER_CXX:-clang++}" -std=c++20 -O1 -g -fno-exceptions -fno-omit-frame-pointer -fsanitize=address,undefined \
    -DPROBE_T="std::int64_t" -I"$w/candidate" "$here/dense-order.cpp" -o "$stage/dense-asan"
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$stage/dense-asan" > "$stage/asan.bin"
"$stage/dense-int64-candidate" > "$stage/cand.bin"
cmp "$stage/asan.bin" "$stage/cand.bin" && echo "int64 dense-order: candidate ASan/UBSan stream matches" | tee -a "$out/verify-int64.log"
