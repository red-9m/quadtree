#!/usr/bin/env bash
# Untimed control/candidate comparison: topology, handles, rects and exact pair sets for all coordinate types,
# exact dense-order streams for integer types, and the budget contract for float/double (plain and ASan/UBSan).
# usage: verify.sh WORK OUT   (WORK/control, WORK/candidate source trees)
set -euo pipefail
export LC_ALL=C
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1") out=$(realpath -m "$2")
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
mkdir -p "$out"
flags=(-std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror)
cases=(extremes "uniform 1000" "uniform 10000" "uniform 100000" "clustered 1000" "clustered 100000"
    "split-line 1000" "split-line 100000" "split-line-translated 1000" "split-line-translated 100000"
    "mixed-size 1000" "mixed-size 100000")
for t in int32 int64 float double; do
    case $t in int*) type=std::${t}_t ;; *) type=$t ;; esac
    log=$out/verify-$t.log
    : > "$log"
    for kind in control candidate; do
        "${CXX:-c++}" "${flags[@]}" -DPROBE_T="$type" -I"$w/$kind" -I"$w/$kind/bench" "$here/equivalence.cpp" \
            "$w/$kind/bench/bench-memory.cpp" -o "$stage/eq-$kind"
    done
    for args in "${cases[@]}"; do
        # shellcheck disable=SC2086
        for kind in control candidate; do "$stage/eq-$kind" $args > "$stage/$kind.bin"; done
        cmp "$stage/control.bin" "$stage/candidate.bin"
        printf '%s equivalence %s: match %s\n' "$t" "$args" "$(sha256sum < "$stage/control.bin" | cut -c1-16)" | tee -a "$log"
    done
    if [[ $t == int* ]]; then
        # The dense-order probe builds integer extremes; floats are covered by the equivalence probe.
        for kind in control candidate; do
            "${CXX:-c++}" "${flags[@]}" -fno-exceptions -DPROBE_T="$type" -I"$w/$kind" "$here/dense-order.cpp" -o "$stage/dense"
            "$stage/dense" > "$stage/$kind.bin"
        done
        if cmp -s "$stage/control.bin" "$stage/candidate.bin"; then
            printf '%s dense-order: exact match %s\n' "$t" "$(sha256sum < "$stage/control.bin" | cut -c1-16)" | tee -a "$log"
        else
            printf '%s dense-order: DIFFERENT\n' "$t" | tee -a "$log"
        fi
    fi
done
: > "$out/budget-contract.log"
for t in float double; do
    "${CXX:-c++}" "${flags[@]}" -DPROBE_T=$t -I"$w/candidate" -I"$w/candidate/bench" "$here/budget-contract.cpp" \
        "$w/candidate/bench/bench-memory.cpp" -o "$stage/bc"
    "${SANITIZER_CXX:-clang++}" -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -DPROBE_T=$t \
        -I"$w/candidate" -I"$w/candidate/bench" "$here/budget-contract.cpp" "$w/candidate/bench/bench-memory.cpp" -o "$stage/bc-asan"
    { printf '%s: ' "$t"; "$stage/bc"; printf '%s ASan/UBSan: ' "$t"; "$stage/bc-asan"; } | tee -a "$out/budget-contract.log"
done
