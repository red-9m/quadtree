#!/usr/bin/env bash
# Run at the repository root with the retained source and a Release build.
set -eu
export LC_ALL=C
build=$(realpath "$1") box2d=$(realpath "$2") out=$3 cpu=$4
mkdir "$out"
out=$(realpath "$out")
here=$(dirname "$(realpath "$0")")
exe="$build/bench/quadtree-bench"

# Profile the Release executable, not the symbol twin.
"${CXX:-c++}" -DQT_BENCH_HAVE_BOX2D -I"$PWD" -I"$build/bench/Release" \
  -I"$box2d/include" -I"$build/_deps/box2d-build/src" -O3 -DNDEBUG -g \
  -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow \
  "$PWD/bench/quadtree-bench.cpp" "$PWD/bench/bench-memory.cpp" \
  "$build/_deps/box2d-build/src/libbox2d.a" -lm -o "$out/quadtree-bench"
for kind in release debug; do
  binary=$exe
  if [[ $kind == debug ]]; then binary="$out/quadtree-bench"; fi
  objcopy --dump-section ".text=$out/$kind.text" "$binary" "$out/$kind.copy"
  objdump -h "$binary" | awk '$2 == ".text" {print $3, $4, $6}' > "$out/$kind.layout"
done
cmp "$out/release.text" "$out/debug.text"
cmp "$out/release.layout" "$out/debug.layout"
(cd "$out" && sha256sum release.text debug.text > text.sha256)

# Gate timing is independent of the sampling runs and keeps all six engines.
taskset -c "$cpu" bash "$here/../g1-flat-input-2026-11-19/reproduce.sh" "$exe" "$out/99-control" 99

record() {
  local p="$out/$1-$4-$5-$6"
  taskset -c "$cpu" perf record -q -e "$2" "$3" "$7" -o "$p.data" -- "$exe" \
    --model=snapshot --types=int32 --scenes="$4" --engines="$6" --sizes= --small-sizes="$5" \
    --batch=256 --rounds=9 --min-time=0.6 --max-time=0.9 --max-reps=400000 --csv="$p.csv" > "$p.log"
  perf script -i "$p.data" -F period,ip,dso,dsoff > "$p.samples"
}
for scene in uniform clustered; do
  for n in 1 2; do record cycles1 cycles:u -F "$scene" "$n" index 4999; done
done
for scene in clustered uniform; do
  for n in 2 1; do record cycles2 cycles:u -F "$scene" "$n" index 5477; done
done
for scene in uniform clustered; do
  for n in 1 2; do record instructions instructions:u -F "$scene" "$n" index 5477; done
done
for scene in clustered uniform; do
  for n in 2 1; do
    for engine in index brute; do record stli ls_bad_status2.stli_other:u -c "$scene" "$n" "$engine" 2047; done
  done
done
python3 "$here/summarize.py" "$out" "$exe"
