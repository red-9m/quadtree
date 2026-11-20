# 01 · Measure the current snapshot baseline

**Goal:** establish which G1/G2 rows still need work on the current headers. No code changes, tuning, threshold changes, or persistent runs. Requirements: `QT-NFR-01`, `QT-NFR-03`, `QT-NFR-11`.

**Read:** [frozen G1/G2 limits and verdict rules](../../bench/MANIFEST.md#frozen-acceptance-limits), [historical snapshot invocation](../../bench/results/index-development/README.md#reproduce), [historical per-row verdicts](../../bench/results/index-development/gates.txt). The later targeted timings are not substitutes for this run.

From the repository root on the designated Ryzen 9 5900HS, use the historical Release toolchain/settings. Write to a fresh output directory; do not overwrite previous results.

```sh
build=$(mktemp -d)
out=$(mktemp -d)
cmake -S . -B "$build" -DCMAKE_BUILD_TYPE=Release -DQUADTREE_BUILD_BENCHMARKS=ON
cmake --build "$build" --target quadtree-bench
"$build/bench/quadtree-bench" --model=snapshot --types=int32 \
  --scenes=uniform,clustered,split-line,split-line-translated,mixed-size,outliers \
  --engines=index,index-ordered,qt-loose-sort,sweep,grid,brute \
  --sizes=1000,10000,100000 --small-sizes=0,1,2,4,8,16,32,64 \
  --batch=256 --rounds=3 --min-time=0.03 --min-reps=3 --max-reps=12 \
  --csv="$out/snapshot.csv" --raw="$out/snapshot-raw.csv"
"$build/bench/quadtree-bench" --gate-files="$out/snapshot.csv" > "$out/gates.txt"
```

**Deliver:** publish the CSV, raw samples, gate report, arguments, and environment together under a new `bench/results/` directory. Record `git rev-parse HEAD` and the tested source delta: `git diff HEAD --binary` does not include untracked headers or harness files, so preserve their contents too (or use an exact committed source revision). Label runs on different hardware/toolchains separately; do not use them to close frozen timing gates without calibration.

**Exit:** account for all 16 historical G1 and 15 historical G2 rows as pass/fail/inconclusive. A hard failure (evaluator exit 3) or inconclusive interval (exit 4) is a valid baseline; a missing row or cross-check failure is not. Link the results in the index. Send only nonpassing rows to 02/03; skip a task if all its measured rows pass. Unmeasured sizes/types remain for 08.
