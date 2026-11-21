#!/usr/bin/env bash
set -euo pipefail
w=$(realpath "$1") out=$(realpath -m "$2")
r=$(dirname "$(dirname "$(realpath "$0")")")
build=$(realpath "$w/candidate-build")
mkdir -p "$out"
bash "$r/g2-grouped-child-screen-2026-11-20/verify-prefix.sh" "$w/control" "$w/candidate" > "$out/ordered-prefix.log"
bash "$r/g9-cached-row-sort-2026-11-20/verify-dense.sh" "$w/control" "$w/candidate" > "$out/dense-prefix.log"
bash "$r/g2-bulk-streams-2026-11-20/verify-order.sh" "$w/control" "$w/inline" "$w/lookahead" "$w/candidate" > "$out/dense-order.log"
cmake --build "$build" -j7 > "$out/build.log" 2>&1
ctest --test-dir "$build" --output-on-failure > "$out/tests.log" 2>&1
"$build/bench/quadtree-index-fuzz" > "$out/fuzz.log" 2>&1
