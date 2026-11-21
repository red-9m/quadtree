#!/usr/bin/env bash
set -euo pipefail
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1")
tree="$w/candidate"
control=$(realpath "$w/control-build")
box2d=$(realpath "$w/box2d")
mkdir -p "$w/candidate-build/bench"
(cd "$tree" && sha256sum -c --quiet "$here/candidate-inputs.sha256")
"${CXX:-c++}" -DQT_BENCH_HAVE_BOX2D -I"$tree" -I"$control/bench/Release" -I"$box2d/include" \
  -I"$control/_deps/box2d-build/src" -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow \
  "$tree/bench/quadtree-bench.cpp" "$tree/bench/bench-memory.cpp" \
  "$control/_deps/box2d-build/src/libbox2d.a" -lm -o "$w/candidate-build/bench/quadtree-bench"
