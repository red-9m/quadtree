#!/usr/bin/env bash
# Build the combined candidate (direct result + flat build dispatch) with -falign-functions=64,
# the same recipe as ../g1-layout-control-2026-11-21/build.sh (whose a64 control is reused).
set -euo pipefail
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1")
frozen=$(realpath "$w/frozen-control-build")
box2d=$(realpath "$w/box2d")
(cd "$w/candidate" && sha256sum -c --quiet "$here/candidate-inputs.sha256")
mkdir -p "$w/candidate-a64/bench"
"${CXX:-c++}" -DQT_BENCH_HAVE_BOX2D -I"$w/candidate" -I"$frozen/bench/Release" -I"$box2d/include" \
  -I"$frozen/_deps/box2d-build/src" -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow \
  -falign-functions=64 "$w/candidate/bench/quadtree-bench.cpp" "$w/candidate/bench/bench-memory.cpp" \
  "$frozen/_deps/box2d-build/src/libbox2d.a" -lm -o "$w/candidate-a64/bench/quadtree-bench"
ln -sfn "$w/candidate-a64" "$w/candidate-build"
