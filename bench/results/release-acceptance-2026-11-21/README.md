# 08 · Release acceptance run (2026-11-21)

**Release blocked.** On the full matrix of release candidate `a72bc80`, the hard gates fail in two places.

- **G2:** fails on 48 rows. All four coordinate types fail at least one row; `int32` fails only one.
- **G1:** fails for `float` and `double` at 1, 2, 4 and 8 entries, outside the accepted 0–4-entry exception.

Every persistent hard gate passes (G6, G7, G8a–c), as do G3, G4 and G6 on snapshots. Every G9 row passes except one unresolved `int64` row. No row reported a cross-check failure or `NONDETERMINISTIC`. The Task 03 G2/G9 work and the Task 02 exception were measured only on the `int32` slice. For the other coordinate types, this run is their first full release measurement.

## Source, build and environment

- **Source:** `git archive` of `64675e6` ([source-head.txt](source-head.txt)). Its runtime, benchmark and test code is that of release candidate `a72bc80`; later commits change only documentation and results.
- **Build:** Release `-O3 -DNDEBUG`, strict warnings, `QUADTREE_BENCH_NATIVE=OFF`, Ninja. GCC 16.2.1, CMake 4.4.2, pinned Box2D `178495ba…` built Release.
- **Binary:** benchmark `.text` `97d8835e…` ([text.sha256](text.sha256)), byte-identical to the Task 02/03 evidence binary.
- **CTest:** [21/21](tests.log).
- **Machine:** Ryzen 9 5900HS, `performance` platform profile and energy preference, HPET clock source, TSC timer. Processes pinned to CPU 6, each stage started after a quiet-start wait. Power settings restored afterwards. [Environment](data/environment.txt) · [run order](data/run-order.csv) · [thermal/frequency](data/thermal.csv).

## Calibration

Two identical `int32` snapshot processes (release engines, manifest defaults) agree on 292 of 298 rows (98.0% `same`; target ≥ 95%). The manifest's 2% snapshot process term stays valid on this compiler, CPU and OS. [Diff](data/calib-diff.txt) · [calib-a](data/calib-a/) · [calib-b](data/calib-b/). The persistent 3% term was not re-validated.

## Matrix

[run.sh](run.sh) stages, all with manifest defaults: 5 rounds, 0.5–4 s per engine, batch 1024 for small sizes, seed 1.

| Stage | Arguments | Time |
|---|---|---:|
| `snapshot` | all 4 types, all 12 scenes, 0–64 (batched `uniform`/`clustered`) and 10²–10⁵; `index,index-ordered,brute,sweep,grid` | 9.8 min |
| `persistent` | all 4 types, 4 scenes, 10³–10⁵, 10 workloads, `full`/`moved`, 64 steps; `index,index-ordered,brute,sweep,grid,grid-rebuild,b2tree,b2tree-fat` | 116 min |
| `resolve-int32`, `resolve-int64` | 45-round reruns, each in a separate process, of the scenes/sizes left inconclusive | 2.5 min |

**Reconciliation** ([reconciliation.csv](data/reconciliation.csv)): all 6,944 expected engine rows are present.
- Snapshot has 1,280 rows: 4 types × 64 scene/size rows × 5 engines.
- Persistent has 5,664 rows. Box2D engines run only for `int32`/`float`, and `grid-rebuild` only in `full` mode.

**Caps, published as rows:**
- Snapshot: `pile` is capped at 10⁴/10⁵ for every engine, by the 4096-entry rule. `brute` is candidate-capped at 10⁵ on every other scene, and `grid` on `outliers` 10⁵.
- Persistent: 1,113 rows are time-capped (349 of them library rows) and 220 candidate-capped. Time-capped estimates cover a prefix of the 64 steps, as the manifest defines.

## Hard gate results

Final status per row ([gate-rows.csv](data/gate-rows.csv), [gate-summary.csv](data/gate-summary.csv)). Rows inconclusive in the 5-round matrix take their verdict from the 45-round rerun. G1 rows with 0–4 entries and an upper bound ≤ 3.5× count as the [accepted exception](../../../docs/plan/02-g1-small-snapshots-result.md).

| Gate | `int32` | `int64` | `float` | `double` |
|---|---|---|---|---|
| G1 (16 rows each) | 8 pass, 8 exception | 8 pass, 8 exception | 6 pass, 2 exception, **8 blocking** | 6 pass, 2 exception, **8 blocking** |
| G2 (20 rows each) | 18 pass, **1 blocking**, 1 inconclusive | 6 pass, **11 blocking**, 3 inconclusive | 2 pass, **18 blocking** | 2 pass, **18 blocking** |
| G9 (15 rows each) | 15 pass | 14 pass, 1 inconclusive | 15 pass | 15 pass |

Other gates:
- **G3, G4:** 36 and 32 snapshot rows, all pass.
- **G6:** 248 snapshot and 864 persistent rows, all pass. No library row allocates in a warm step.
- **G7:** 240 hard rows pass.
- **G8a/G8b/G8c:** 48/16/48 hard rows, all pass.
- **Types without a hard reference:** hard G7/G8c need a Box2D reference, so `int64`/`double` have only soft comparisons there.

### Blocking and unresolved rows

- **G1, `float`/`double`:**
  - 1, 2 and 4 entries run at 4.2–5.4× brute in both scenes, above the exception cap of 3.5×.
  - 8 entries run at 2.13–2.27×; 8 is outside the exception's 0–4 scope.
  - Integer types stay inside the exception (`int32` ≤ 3.07×, `int64` ≤ 3.19× upper bound).
- **G2, `float`/`double`:** `uniform`, `split-line`, `split-line-translated` and `mixed-size` fail at every size; `clustered` fails at 10³–10⁵. Ratios reach 10–11.5× at 10³ (limit 4) and 4.6–5.1× at 10⁵ (limit 2). Only `clustered` 10² and `mixed-size` 10⁵ pass.
- **G2, `int64`:**
  - Blocking: `split-line` and `split-line-translated` at every size (2.8–5.1×), and `uniform` 10³–10⁵ (2.7–4.8×).
  - Unresolved after 45 rounds: `uniform` 10², `mixed-size` 10² and `clustered` 10⁵.
- **G2, `int32`:**
  - Blocking: `split-line-translated` 10², 4.18× [4.06–4.30] at 45 rounds, against the best baseline `sweep`. Task 01 omitted 10² rows, so Task 03 never measured them.
  - Unresolved: `uniform` 10³, 3.93× [3.82–4.04] at 45 rounds, with the interval dominated by the 2% process term.
- **G9, `int64` `mixed-size` 10⁵:** 3.99× [3.87–4.10] at 45 rounds, unresolved.

## Published, not gated

- **Soft gates (persistent, vs grid):** G7 413 pass / 46 fail / 21 inconclusive; G8c 64 / 12 / 20.
- **G5 pathology** ([g5-pathology.csv](data/g5-pathology.csv)): `index` vs the best timed baseline, worst row per scene: `points` 27.6×, `outliers` 17.7×, `long-thin` 12.6×, `disjoint` 9.9×, `tiles` 9.1×, `tiles-touch` 8.0×, `pile` 4.2×.
- **Library timing, work and memory** ([library-rows.csv](data/library-rows.csv)): estimate, error, median, p95, phase medians, candidates, node visits, pairs, warm/cold allocations, cold peak and retained bytes for every `index`/`index-ordered` row. Largest cold peak: 39.7 MB snapshot, 49.7 MB persistent.
- **Raw data per stage:** `results.csv`, `raw.csv.gz` (all samples), `gates.txt`, `run.log`, `time.log`, `args.txt`.

## Reproduction

Starting from a `git archive` of the source in `WORK/src`, with `WORK/box2d` at the pinned commit:

1. Configure and build: `cmake -S WORK/src -B WORK/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUADTREE_BUILD_BENCHMARKS=ON -DQUADTREE_BUILD_TESTS=ON -DQUADTREE_BENCH_BOX2D=ON -DQUADTREE_BENCH_NATIVE=OFF -DFETCHCONTENT_SOURCE_DIR_BOX2D=WORK/box2d -DFETCHCONTENT_FULLY_DISCONNECTED=ON`, then `cmake --build WORK/build`.
2. Run the stages: `bash run.sh WORK data` (calibration, snapshot, persistent), then `bash run.sh WORK data resolve-int32 resolve-int64`.
3. Calibration diff: `quadtree-bench --diff=data/calib-a/results.csv,data/calib-b/results.csv`.
4. Summaries: `python3 summarize.py data`.

## Handoff

**08 stays open; release is blocked.** Per the brief, there are no fixes or threshold changes in this task. Releasing needs an owner decision on scope, one of:

1. Declare `int32` the only supported type for this release. Its only remaining blockers are G2 `split-line-translated` 10² (blocking) and `uniform` 10³ (unresolved).
2. Open a new optimization task covering G2 and small-snapshot G1 for `int64`/`float`/`double`, plus `int32` 10².
3. Both: ship `int32` first and follow with the other types.
