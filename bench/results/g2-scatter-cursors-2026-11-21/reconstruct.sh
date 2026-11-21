#!/usr/bin/env bash
# Rebuild the staged direct-init source and benchmark in a new workspace.
set -euo pipefail
here=$(dirname "$(realpath "$0")")
repo=$(git -C "$here" rev-parse --show-toplevel)
work=$(realpath -m "$1")
mkdir "$work"
mkdir "$work/candidate"
git -C "$repo" archive 57d30dcc49cff7de5563f46625b385cc145861cb | tar -xf - -C "$work/candidate"
patches=(g2-topdown-build-2026-11-20/candidate.patch
    g2-grouped-child-screen-2026-11-20/child-screen.patch g9-radix-row-sort-2026-11-20/radix-row-sort.patch
    g2-compact-classifier-2026-11-20/candidate.patch g9-build-rank-2026-11-20/candidate.patch
    g2-bulk-streams-2026-11-20/narrow.patch g2-inline-count-2026-11-20/inline.patch
    g2-inline-count-2026-11-20/lookahead.patch g2-topology-cost-2026-11-20/candidate.patch)
for p in "${patches[@]}"; do
    (cd "$work/candidate" && patch -s -p1 --fuzz=0 < "$here/../$p")
done
(cd "$work/candidate" && sha256sum -c --quiet "$here/../g2-topology-cost-2026-11-20/candidate-inputs.sha256")
if [[ -n ${BOX2D_SOURCE:-} ]]; then
    ln -s "$(realpath "$BOX2D_SOURCE")" "$work/box2d"
else
    mkdir "$work/box2d"
    git -C "$work/box2d" init -q
    git -C "$work/box2d" fetch --depth 1 https://github.com/erincatto/box2d.git 178495ba35695aefe9c321e569d2277f06db3b90
    git -C "$work/box2d" checkout -q --detach FETCH_HEAD
fi
[[ $(git -C "$work/box2d" rev-parse HEAD) == 178495ba35695aefe9c321e569d2277f06db3b90 ]]
cmake -S "$work/candidate" -B "$work/candidate-build" -DCMAKE_BUILD_TYPE=Release \
    -DQUADTREE_BUILD_BENCHMARKS=ON -DQUADTREE_BUILD_TESTS=ON -DQUADTREE_BENCH_NATIVE=OFF \
    -DFETCHCONTENT_SOURCE_DIR_BOX2D="$work/box2d"
cmake --build "$work/candidate-build" -j "${JOBS:-8}" --target quadtree-bench
