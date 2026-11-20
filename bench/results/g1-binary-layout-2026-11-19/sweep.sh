#!/usr/bin/env bash
# Relink the already compiled benchmark object; do not recompile its code.
set -eu
export LC_ALL=C
control=$(realpath "$1") direct=$(realpath "$2") out=$3 cpu=$4
mkdir "$out"
out=$(realpath "$out")
here=$(dirname "$(realpath "$0")")
shifts=(0 16 32 48 64 80 96 112)

for kind in control direct; do
  build=${!kind}
  obj="$build/bench/CMakeFiles/quadtree-bench.dir"
  stage="$out/$kind"
  mkdir "$stage"
  objcopy --set-section-alignment .text=16 "$obj/quadtree-bench.cpp.o" "$stage/bench.o"
  objcopy --dump-section ".text=$stage/original-object.text" "$obj/quadtree-bench.cpp.o" "$stage/original-object.copy"
  objcopy --dump-section ".text=$stage/relaxed-object.text" "$stage/bench.o" "$stage/relaxed-object.copy"
  cmp "$stage/original-object.text" "$stage/relaxed-object.text"
  objcopy --dump-section ".text=$stage/release.text" "$build/bench/quadtree-bench" "$stage/release.copy"
  for shift in "${shifts[@]}"; do
    printf '.text\n.p2align 6\n.fill %s, 1, 0x90\n.section .note.GNU-stack,"",@progbits\n' "$shift" > "$stage/pad.s"
    "${CC:-cc}" -c "$stage/pad.s" -o "$stage/pad.o"
    "${CXX:-c++}" -O3 -DNDEBUG "$stage/pad.o" "$stage/bench.o" "$obj/bench-memory.cpp.o" \
      "$build/_deps/box2d-build/src/libbox2d.a" -lm -o "$stage/bench-$shift"
    nm -nSC "$stage/bench-$shift" > "$stage/bench-$shift.nm"
  done
  objcopy --dump-section ".text=$stage/zero.text" "$stage/bench-0" "$stage/zero.copy"
  cmp "$stage/release.text" "$stage/zero.text"
  (cd "$stage"; sha256sum {release,zero,original-object,relaxed-object}.text > text.sha256)
  rm "$stage/"*.copy
done

# Two engines, four fixture rows, original runner and fixtures. Reverse the
# second block; use a nonmonotone order for the third. Never time in parallel.
printf 'replicate,kind,shift,start\n' > "$out/run-order.csv"
for replicate in 1 2 3; do
  case $replicate in
    1) order=(0 16 32 48 64 80 96 112); kinds=(control direct);;
    2) order=(112 96 80 64 48 32 16 0); kinds=(direct control);;
    3) order=(48 96 16 64 0 112 32 80); kinds=(control direct);;
  esac
  for shift in "${order[@]}"; do
    for kind in "${kinds[@]}"; do
      run="$out/$replicate-$kind-$shift"
      mkdir "$run"
      printf '%s,%s,%s,%s\n' "$replicate" "$kind" "$shift" "$(date -Is)" >> "$out/run-order.csv"
      taskset -c "$cpu" "$out/$kind/bench-$shift" --model=snapshot --types=int32 \
        --scenes=uniform,clustered --engines=index,brute --sizes= --small-sizes=0,1 --batch=256 \
        --rounds=67 --min-time=0.025 --max-reps=2077 --csv="$run/snapshot.csv" \
        --raw="$run/snapshot-raw.csv" > "$run/run.log"
      gzip -n "$run/snapshot-raw.csv"
    done
  done
done
python3 "$here/summarize.py" "$out"
