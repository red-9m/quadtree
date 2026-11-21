#!/usr/bin/env bash
# Run at the repository root. Use a Release build with pinned Box2D.
set -eu
export LC_ALL=C
build=$(realpath "$1") box2d=$(realpath "$2") out=$3 cpu=$4
mkdir "$out"
out=$(realpath "$out")
exe="$build/bench/quadtree-bench"

# Sample Release, then symbolize against a -g twin only if its code matches.
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
sha256sum "$out/release.text" "$out/debug.text" > "$out/text.sha256"
rm "$out/"{release,debug}{.text,.copy,.layout}

for round in 1 2; do
  if [[ -n ${QT_PROFILE_QUIET:-} ]]; then
    for f in /sys/class/hwmon/hwmon*/name; do
      if [[ $(<"$f") == k10temp ]]; then temp="${f%/name}/temp1_input"; break; fi
    done
    taskset -c 0 python3 "$QT_PROFILE_QUIET" "${temp:?Missing k10temp}" "$out/quiet-$round.csv" >/dev/null
  fi
  if [[ $round == 1 ]]; then
    scenes=(uniform clustered); sizes=(0 1 2 4); engines=(index brute); frequency=4999
  else
    scenes=(clustered uniform); sizes=(4 2 1 0); engines=(brute index); frequency=5477
  fi
  for scene in "${scenes[@]}"; do
    for n in "${sizes[@]}"; do
      for engine in "${engines[@]}"; do
        p="$out/$round-$scene-$n-$engine"
        taskset -c "$cpu" perf record -q -e cycles:u -F "$frequency" -o "$p.data" -- "$exe" \
          --model=snapshot --types=int32 --scenes="$scene" --engines="$engine" --sizes= --small-sizes="$n" \
          --batch=256 --rounds=9 --min-time=0.6 --max-time=0.9 --max-reps=400000 --csv="$p.csv" > "$p.log"
        perf script -i "$p.data" -F period,ip,dso,dsoff > "$p.samples"
      done
    done
  done
done
python3 "$(dirname "$0")/summarize.py" "$out" "$exe"

# Optional AMD Zen 3 store-to-load interlock cross-check.
if [[ ${5:-} == stli ]]; then
  for engine in index brute; do
    for n in 0 1; do
      p="$out/stli-sample-uniform-$n-$engine"
      taskset -c "$cpu" perf record -q -e ls_bad_status2.stli_other:u -c 8191 -o "$p.data" -- "$exe" \
        --model=snapshot --types=int32 --scenes=uniform --engines="$engine" --sizes= --small-sizes="$n" \
        --batch=256 --rounds=9 --min-time=0.25 --max-time=0.5 --max-reps=400000 --csv="$p.csv" > "$p.log"
      perf script -i "$p.data" -F period,ip,dso,dsoff > "$p.events"
    done
  done
fi
