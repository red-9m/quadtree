# 08 · Four-type release validation of `ce92ef5` (2026-11-28)

**`int32`, `int64`, `float` and `double` all meet the [pre-specified rule](resolution-rule.md) on `ce92ef5`.** Every hard snapshot and persistent gate passes outright for all four types in the 5-round and 64-step matrices. No row needed a repeat, and no exception was used. The `int32` release acceptance therefore extends to `ce92ef5`. Task 09's exit criteria hold for `int64`, `float` and `double`.

| Type | G1 (16) | G2 (20) | G9 (15) | G3 / G4 / G6 snapshot | Persistent hard gates |
|---|---|---|---|---|---|
| `int32` | **16 pass** | **20 pass** | **15 pass** | 9 / 8 / 62 pass | G6 216, G7 120, G8a 12, G8b 4, G8c 24 pass |
| `float` | **16 pass** | **20 pass** | **15 pass** | 9 / 8 / 62 pass | G6 216, G7 120, G8a 12, G8b 4, G8c 24 pass |
| `double` | **16 pass** | **20 pass** | **15 pass** | 9 / 8 / 62 pass | G6 216, G8a 12, G8b 4 pass (no Box2D reference) |
| `int64` | **16 pass** | **20 pass** | **15 pass** | 9 / 8 / 62 pass | G6 216, G8a 12, G8b 4 pass (no Box2D reference) |

- **G0:** every benchmark and gate-evaluator process exits 0. There is no `MISMATCH`, `NONDETERMINISTIC` or `UNORDERED` result.
- **G6:** zero warm allocations on all 2,224 library rows.
- **Calibration:** `calib-a`/`calib-b` agree on **291/298 rows (97.7%)**, with a geometric mean of 1.001. The target is ≥ 95%.
- **[CTest](ctest.log): 22/22** on the archive build.
- **[Full differential fuzz](fuzz.log): 20/20**, 4 types × 5 scenes, seeds 1–40, 1500 steps. Every seventh seed now exercises the common-filter kernels.
- **Hosted [Portability #24](https://github.com/red-9m/quadtree/actions/runs/46095847492): 9/9** on exactly `ce92ef5` ([ci.json](ci.json)).

## Source, build and environment

- **Source:** `git archive` of `ce92ef580b7582db1afa59528dd710d26b5abe56` ([source-head.txt](source-head.txt)). It adds the [common-filter pair kernels for `float`/`double`](../float-common-pairs-2026-11-27/README.md) to `c13e498`. Workspace and cache values: [build.txt](build.txt).
- **Build:** Ninja Release `-O3 -DNDEBUG`, GCC 16.2.1, CMake 4.4.2, `QUADTREE_BUILD_BENCHMARKS=ON`, `QUADTREE_BENCH_NATIVE=OFF`. Box2D is the v3.2.0 archive pinned by `URL_HASH` (`ffec747b…`), with no local source override.
- **Hashes:** `quadtree-bench` `f1d1694a…`, benchmark `.text` `f4905252…` ([text.sha256](text.sha256)), `detail/quadtree-index-impl.h` `8c653e4f…`. These are byte-identical to the binary measured in the [candidate evidence](../float-common-pairs-2026-11-27/README.md). Each `run.sh` invocation rechecks `.text` and the Box2D archive; all 3 `data/text-check-*.log` are `OK`.
- **Machine:** Ryzen 9 5900HS on AC power, kernel 7.3.0, HPET clock source, `amd-pstate-epp`, governor `powersave`, boost on. Each invocation sets EPP and platform profile to `performance` and restores them afterwards (`power-before.txt` = `power-after.txt`). Stages run pinned to CPU 6 with sibling 7 idle; the monitor runs on CPU 0.
- **Quiet starts:** before each stage, Tctl ≤ 40 °C and host, CPU 6 and CPU 7 ≤ 5% busy. Stages started at 39.5–40.1 °C and peaked at 68.0–72.8 °C.
- **Records:** [environment](data/environment.txt), [run order](data/run-order.csv), [thermal/frequency every 5 s](data/thermal.csv).

## Run

[resolution-rule.md](resolution-rule.md), [run.sh](run.sh) and [matrices.sh](matrices.sh) were committed (`ef2319f`) before any stage ran. They are the release-c13e498 runner with this directory's hashes. Two scripts changed: [summarize.py](summarize.py) now marks non-`int32` hard rows as blocking rather than published, and [resolve.py](resolve.py) changed only its docstring. Arguments are the manifest defaults: 5 rounds, seed 1, 0.5–4 s per engine, batch 1024, candidate cap 2·10⁸, 64 persistent steps.

| Stages | When (11-28) | Time |
|---|---|---:|
| `calib-a`, `calib-b` | 04:02–04:07 | 2 × 2.2 min |
| `int32`/`float`/`double`/`int64-snapshot` | 04:07–04:17 | 2.2–2.3 min each |
| `int32`/`float`/`double`/`int64-persistent` | 04:18–06:14 | 33.1 / 33.6 / 24.6 / 23.8 min |

No stage aborted. The rule required no snapshot or persistent repeats, so there are no `resolve`/`presolve` stages. The host was idle apart from the session shell.

## Reconciliation

All **6,944** expected engine rows are present ([reconciliation.csv](data/reconciliation.csv)).
- **Snapshot:** 1,192 `ok` rows, 48 candidate-capped (`brute` at 10⁵, `grid` on `outliers` 10⁵), and 40 `pile` 10⁴/10⁵ rows capped as a whole. These are the same caps as earlier release runs.
- **Persistent:** 4,340 `ok`, 1,104 time-capped and 220 candidate-capped. Of the 1,728 library rows, 341 are time-capped.

## Margins

Final status: [gate-rows.csv](data/gate-rows.csv), [gate-summary.csv](data/gate-summary.csv), [snapshot](data/final-snapshot-status.txt), [persistent](data/final-persistent-status.txt), [summary](data/summary.txt). The worst row per gate:

| Type | G1 0–4 (limit 2) | G1 8–64 | G2 10²–10³ (limit 4) | G2 10⁴–10⁵ (limit 2) | G9 (limit 4) |
|---|---|---|---|---|---|
| `int32` | `uniform` n=4 1.898 [1.821–1.978] | `clustered` n=8 1.570 [1.518–1.624] | `uniform` 10³ 3.829 [3.712–3.950] | `split-line-translated` 10⁵ 1.893 [1.838–1.948] | `split-line-translated` 10⁵ 3.499 [3.371–3.631] |
| `float` | `uniform` n=4 1.725 [1.668–1.784] | `clustered` n=8 0.625 [0.591–0.661] | `split-line-translated` 10² 3.388 [3.236–3.546] | `split-line-translated` 10⁵ 1.748 [1.687–1.812] | `split-line` 10⁵ 3.699 [3.586–3.815] |
| `double` | `uniform` n=1 1.627 [1.497–1.768] | `clustered` n=8 0.665 [0.646–0.686] | `split-line-translated` 10² 3.569 [3.455–3.687] | `split-line` 10⁵ 1.724 [1.551–1.917] | `split-line-translated` 10⁵ 3.656 [3.539–3.777] |
| `int64` | `uniform` n=4 1.761 [1.711–1.813] | `clustered` n=8 1.230 [1.194–1.267] | `split-line-translated` 10² 3.275 [3.152–3.402] | `uniform` 10⁵ 1.849 [1.786–1.915] | `split-line-translated` 10⁵ 3.514 [3.391–3.641] |

- **`int32` exception rows** pass outright, so the exceptions are not used: G2 `split-line-translated` 10² 3.548 [3.437–3.662] and `uniform` 10³ 3.829 [3.712–3.950].
- **Persistent, worst row:**
  - G7: `int32` 1.281 [1.228–1.337], `float` 1.250 [1.187–1.318] (`uniform` 10³ `static/full`, limit 2).
  - G8a: ≤ 20.6 ns for every type (limit 1 µs).
  - G8b: 3.290–3.372, upper ≤ 3.601 (`mixed-size`, limit 4).
  - G8c: `int32` 1.170 [1.115–1.229], `float` 1.201 [1.129–1.277] (limit 2).
- **Soft gates (vs grid, published):** G7 107–110 pass, 5–10 fail and 2–5 inconclusive per type; G8c 15–17 pass, 3–4 fail and 4–5 inconclusive per type. This is the same pattern as earlier release runs.

### Watch items

- **`int32` G1 n=4:** `uniform` 1.898 [1.821–1.978] and `clustered` 1.845 [1.752–1.944]; both pass.
- **`float clustered n=2` G1:** pass, 1.585 [1.532–1.639]. `results.csv`/`raw.csv` paths are 100/96 characters.
- **`float`/`double` G9 at 10⁵:** worst upper bounds 3.815 (`float split-line`) and 3.777 (`double split-line-translated`).

## Published, not gated

- **G5 pathology** ([g5-pathology.csv](data/g5-pathology.csv)), `index` vs the best timed baseline, worst row per scene:
  - `int32`: `outliers` 8.43×, `long-thin` 5.76×, `tiles` 4.73×.
  - `float`: `outliers` 9.82×, `tiles` 4.12×, `pile` 3.50×.
  - `double`: `outliers` 9.62×, `tiles` 4.57×, `pile` 3.57×.
  - `int64`: `outliers` 7.10×, `pile` 4.38×, `tiles` 4.25×.
- **Library timing, work and memory** ([library-rows.csv](data/library-rows.csv)): no row has a nonzero warm allocation count. The largest cold peaks are 39.7 MB snapshot (`double split-line-translated` 10⁵ `index-ordered`) and 49.7 MB persistent (`double uniform` 10⁵ `churn-10%/full` `index-ordered`), the same as earlier runs.
- **Raw data per stage:** `results.csv`, `raw.csv.gz`, `gates.txt`, `run.log`, `time.log`, `args.txt`, `exit.txt`, `quiet-start.csv`, `path-lengths.txt`.

## Decision

Per [resolution-rule.md](resolution-rule.md):
- **`int32`:** every hard snapshot and persistent gate passes, and all 15 G9 rows pass. **The `int32` release acceptance extends to `ce92ef5`.**
- **`int64`, `float`, `double`:** every hard snapshot gate (G1 16, G2 20, G3, G4, G6, G9 15) and every applicable hard persistent gate passes, with no exception. **Task 09 exit criteria are met; all three types are performance-accepted on `ce92ef5`.**
- **All types:** G0 and G6 pass. Calibration is 97.7% ≥ 95%. CTest 22/22, full fuzz 20/20, hosted Portability 9/9 on the exact SHA.

## Reproduction

1. Extract the source: `git archive ce92ef5 | tar -x -C WORK/src`.
2. Configure and build: `cmake -S WORK/src -B WORK/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUADTREE_BUILD_BENCHMARKS=ON -DQUADTREE_BENCH_NATIVE=OFF`, then `cmake --build WORK/build`. The build downloads Box2D v3.2.0 and verifies its hash.
3. Run `ctest` and `bench/quadtree-index-fuzz` in `WORK/build`.
4. Save the `.text` hash: `objcopy --dump-section .text=WORK/bench.text WORK/build/bench/quadtree-bench WORK/copy`.
5. Run the matrices: `bash matrices.sh WORK data`.
6. Write the summaries with `python3 summarize.py data`. Record hosted CI with `python3 collect-ci.py .`.
