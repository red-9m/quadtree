#!/usr/bin/env bash
set -euo pipefail
here=$(dirname "$(realpath "$0")")
w=$(realpath "$1")
for kind in empty singleton; do
  (
    tree="$w/$kind/candidate"
    control=$(realpath "$w/$kind/control-build")
    box2d=$(realpath "$w/$kind/box2d")
    out="$w/$kind/candidate-build/bench/quadtree-bench"
    (cd "$tree" && sha256sum -c --quiet "$here/$kind/candidate-inputs.sha256")
    "${CXX:-c++}" -DQT_BENCH_HAVE_BOX2D -I"$tree" -I"$control/bench/Release" -I"$box2d/include" \
      -I"$control/_deps/box2d-build/src" -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow \
      "$tree/bench/quadtree-bench.cpp" "$tree/bench/bench-memory.cpp" \
      "$control/_deps/box2d-build/src/libbox2d.a" -lm -o "$out"
  ) &
  pids+=("$!")
done
rc=0
for pid in "${pids[@]}"; do wait "$pid" || rc=1; done
exit "$rc"
