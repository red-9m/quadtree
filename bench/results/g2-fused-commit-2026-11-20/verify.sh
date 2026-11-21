#!/usr/bin/env bash
set -euo pipefail
w=$(realpath "$1") out=$(realpath -m "$2")
r=$(dirname "$(dirname "$(realpath "$0")")")
mkdir -p "$out"
bash "$r/g2-bulk-streams-2026-11-20/verify-order.sh" "$w/control" "$w/candidate" > "$out/dense-order.log"
bash "$r/g2-cross-screen-2026-11-20/verify-prefix.sh" "$w/control" "$w/candidate" > "$out/unordered-prefix.log"
bash "$r/g2-grouped-child-screen-2026-11-20/verify-prefix.sh" "$w/control" "$w/candidate" > "$out/ordered-prefix.log"
bash "$r/g2-topdown-build-2026-11-20/verify.sh" "$w/control" "$w/candidate" > "$out/equivalence.log"
ctest --test-dir "$w/candidate-build" --output-on-failure > "$out/tests.log" 2>&1
"$w/candidate-build/bench/quadtree-index-fuzz" > "$out/fuzz.log" 2>&1
