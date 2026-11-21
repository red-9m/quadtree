#!/usr/bin/env bash
# Build control and direct-result candidate with one identical extra layout flag set.
# usage: build.sh WORK OUTDIR_SUFFIX [EXTRA_FLAGS...]  -> WORK/{control,candidate}-SUFFIX/bench/quadtree-bench
set -euo pipefail
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1"); suffix=$2; shift 2
frozen=$(realpath "$w/frozen-control-build")
box2d=$(realpath "$w/box2d")
(cd "$w/control" && sha256sum -c --quiet "$here/../g2-packed-cross-item-2026-11-21/candidate-inputs.sha256")
(cd "$w/candidate" && sha256sum -c --quiet "$here/../g1-two-direct-result-2026-11-21/candidate-inputs.sha256")
for kind in control candidate; do
    mkdir -p "$w/$kind-$suffix/bench"
    "${CXX:-c++}" -DQT_BENCH_HAVE_BOX2D -I"$w/$kind" -I"$frozen/bench/Release" -I"$box2d/include" \
      -I"$frozen/_deps/box2d-build/src" -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow \
      "$@" "$w/$kind/bench/quadtree-bench.cpp" "$w/$kind/bench/bench-memory.cpp" \
      "$frozen/_deps/box2d-build/src/libbox2d.a" -lm -o "$w/$kind-$suffix/bench/quadtree-bench" &
done
wait
