# 08 · Release validation of `c13e498` (2026-11-26/27)

**Every hard gate passes for `int32`, with no exception rows needed.** The calibration pair missed its target, so under the [pre-specified rule](resolution-rule.md) `int32` timing sign-off is **withheld pending an owner decision** (see [Decision](#decision)). Correctness, allocation, memory, CTest and portability checks pass for all four coordinate types.

- **`int32` snapshot** (after the rule's repeats): G1 16/16, G2 20/20, G3 9, G4 8, G6 62, G9 15/15 pass. The G1 0–4-entry and G2 small-tree exceptions are not used.
- **`int32` persistent:** G6 216, G7 120, G8a 12, G8b 4, G8c 24, all pass.
- **All types:** every benchmark process exits 0, so there is no `MISMATCH`, `NONDETERMINISTIC` or `UNORDERED` result (G0). Zero warm allocations on all 1,112 G6 rows. Persistent hard gates pass for every type.
- **Calibration:** `calib-a`/`calib-b` agree on **281/298 rows (94.3%)**, below the ≥ 95% target. A supplementary pair run after the matrices, `calib-c`/`calib-d`, agrees on 291/298 (97.7%).
- **CTest** [22/22](ctest.log). **Hosted [Portability #21](https://github.com/red-9m/quadtree/actions/runs/45887625703)** passes 9/9 on exactly `c13e498` ([ci.json](ci.json)).
- **Non-`int32` timing (published, not blocking):** `float` G2 `uniform` 10³ and G9 `split-line-translated` 10⁵ stay inconclusive; `double` G2 `uniform` 10³ and 10⁵ stay inconclusive. No row fails.

## Source, build and environment

- **Source:** `git archive` of `c13e498fffe18736ea0a8ab871eb366cdfa5aa3c` ([source-head.txt](source-head.txt)). Workspace and cache values: [build.txt](build.txt).
- **Build:** Ninja Release `-O3 -DNDEBUG`, GCC 16.2.1, CMake 4.4.2, `QUADTREE_BUILD_BENCHMARKS=ON`, `QUADTREE_BENCH_NATIVE=OFF`. Box2D is the v3.2.0 archive pinned by `URL_HASH` in `bench/CMakeLists.txt` (`ffec747b…`, no local source override).
- **Hashes, matching the [handoff](../../../SESSION-HANDOFF.md#provenance):** `quadtree-bench` `f7d8968a…`, benchmark `.text` `369ce16e…` ([text.sha256](text.sha256)), `detail/quadtree-index-impl.h` `0264f71b…`. Each `run.sh` invocation rechecks `.text` and the Box2D archive; all 8 `data/text-check-*.log` are `OK`.
- **Machine:** Ryzen 9 5900HS on AC power, kernel 7.3.0, HPET clock source, TSC timer, `amd-pstate-epp` active, governor `powersave`, boost on. Each invocation sets EPP and platform profile to `performance` and restores the previous values afterwards (`power-before.txt` = `power-after.txt`). Stages are pinned to CPU 6 with sibling 7 idle; the monitor runs on CPU 0.
- **Quiet starts:** before each stage, Tctl ≤ 40 °C and host, CPU 6 and CPU 7 ≤ 5% busy over 10 s ([quiet-start.py](../g2-two-scatter-repeatability-2026-11-21/quiet-start.py), per-stage `quiet-start.csv`). Every stage started at 31.9–40.0 °C.
- **Records:** [environment](data/environment.txt) (written by the last invocation), [run order](data/run-order.csv), [thermal/frequency every 5 s](data/thermal.csv).

## Run

The [rule](resolution-rule.md), [run.sh](run.sh) and [matrices.sh](matrices.sh) were committed (`13629a3`) before any stage ran. Arguments are the manifest defaults: 5 rounds, seed 1, 0.5–4 s per engine, batch 1024, candidate cap 2·10⁸, 64 persistent steps. Snapshot engines are `index,index-ordered,brute,sweep,grid`. Persistent engines add `grid-rebuild,b2tree,b2tree-fat`. Every stage's exact arguments are in its `args.txt`.

| Stages | When | Time |
|---|---|---:|
| `calib-a`, `calib-b` | 11-26 22:05–22:10 | 2 × 2.2 min |
| `int32`/`float`/`double`/`int64-snapshot` | 22:12–22:22 | 2.1–2.4 min each |
| `int32`/`float`/`double-resolve`, `-resolve-2` (45 rounds) | 22:23–22:30 | 0.6–1.4 min each |
| `int32-persistent-aborted` | 22:30–22:37 | killed by host shutdown |
| `int32`/`float`/`double`/`int64-persistent` | 11-27 01:07–03:04 | 33.1 / 33.6 / 24.5 / 23.7 min |
| `calib-c`, `calib-d` (supplementary) | 03:07–03:12 | 2 × 2.2 min |

- **Aborted stage:** the host shut down at 22:37 (`last -x`) while `int32-persistent` was running. It is kept as [`int32-persistent-aborted/`](data/int32-persistent-aborted/ABORTED.txt) and is not evidence. After the reboot, the persistent half was rerun from scratch with the same arguments (`QT_FROM=persistent` in `matrices.sh`; the snapshot half is unchanged). The snapshot stages ran on the earlier boot, the persistent stages on the later one, with the same kernel, binary and power settings.
- **Repeats:** under the rule, `int64` needed no snapshot repeats, and no type needed persistent repeats (`presolve`).
- **Thermal:** stage maxima were 61–73 °C, except `calib-b`. That stage rose from 69 °C to 83 °C in its last 15 s (22:09:40–22:09:56), and Tctl peaked at 90 °C just after it ended. This coincides with the `outliers` 10⁵ rows that differ between `calib-a` and `calib-b`. Likely cause: other host load. Another agent session was active then (unverified); a single pinned benchmark process reaches ≤ 73 °C.

## Reconciliation

All **6,944** expected engine rows are present ([reconciliation.csv](data/reconciliation.csv)); none are missing.
- **Snapshot:** 1,280 rows = 4 types × 64 scene/size rows × 5 engines. 1,192 `ok`, 48 candidate-capped (`brute` at 10⁵ on 11 scenes, `grid` on `outliers` 10⁵) and 40 `pile` 10⁴/10⁵ rows capped as a whole by the 4096-entry rule. Same caps as earlier release runs.
- **Persistent:** 5,664 rows, with Box2D engines for `int32`/`float` only and `grid-rebuild` only in `full` mode. 4,341 `ok`, 1,103 time-capped and 220 candidate-capped. Of the 1,728 library rows, 341 are time-capped; their estimates cover a prefix of the 64 steps, as the manifest defines.

## Hard gates

Final status after the rule ([gate-rows.csv](data/gate-rows.csv), [gate-summary.csv](data/gate-summary.csv), [final snapshot](data/final-snapshot-status.txt), [final persistent](data/final-persistent-status.txt), [summary](data/summary.txt)).

| Type | G1 (16) | G2 (20) | G9 (15) | G3 / G4 / G6 snapshot | Persistent hard gates |
|---|---|---|---|---|---|
| `int32` | **16 pass** | **20 pass** (2 after repeats) | **15 pass** | 9 / 8 / 62 pass | G6 216, G7 120, G8a 12, G8b 4, G8c 24 pass |
| `float` | 16 pass | 19 pass, 1 inconclusive | 14 pass, 1 inconclusive | pass | G6, G7, G8a, G8b, G8c pass |
| `double` | 16 pass (1 after repeats) | 18 pass, 2 inconclusive | 15 pass | pass | G6, G8a, G8b pass (no Box2D reference) |
| `int64` | 16 pass | 20 pass | 15 pass | pass | G6, G8a, G8b pass (no Box2D reference) |

Persistent soft gates (vs grid): G7 107–110 pass, 6–9 fail and 4 inconclusive per type; G8c 16–17 pass, 2–3 fail and 5–6 inconclusive per type.

### Matrix inconclusives and repeats

Original 5-round verdicts are published as measured; pooled values come from the two 45-round processes ([rule](resolution-rule.md)).

| Type | Gate | Row | Matrix | Repeats | Pooled | Final |
|---|---|---|---|---|---|---|
| `int32` | G2 | `split-line-translated` 10⁵ | 1.710 [1.388–2.106] | 1.894, 1.888 | 1.891 [1.853–1.930] | pass |
| `int32` | G2 | `uniform` 10⁵ | 2.035 [1.938–2.138] | 1.865, 1.864 | 1.864 [1.827–1.902] | pass |
| `float` | G2 | `uniform` 10³ | 3.928 [3.768–4.096] | 4.049, 3.998 | 4.023 [3.939–4.110] | inconclusive |
| `float` | G9 | `mixed-size` 10⁵ | 3.945 [3.823–4.072] | 3.894, 3.913 | 3.903 [3.825–3.984] | pass |
| `float` | G9 | `split-line-translated` 10⁵ | 3.992 [3.875–4.112] | 3.985, 4.077 | 4.031 [3.950–4.113] | inconclusive |
| `double` | G1 | `clustered` n=1 | 2.061 [1.511–2.810] | 2.062, 1.742 | 1.895 [1.822–1.971] | pass |
| `double` | G2 | `uniform` 10³ | 4.080 [3.957–4.206] | 3.991, 3.942 | 3.966 [3.880–4.055] | inconclusive |
| `double` | G2 | `uniform` 10⁵ | 1.969 [1.906–2.033] | 1.989, 1.976 | 1.982 [1.942–2.024] | inconclusive |

During the `int32` matrix's `uniform` 10⁵ row, both `index` and `grid` had elevated p95 (16.1 and 11.6 ms against medians of 13.4 and 6.6 ms). Both repeats returned to the earlier 1.86. The two `double clustered n=1` repeats disagree beyond their intervals (2.06 vs 1.74), which is the documented per-process placement effect at tiny sizes.

### `int32` margins

- **G1:** worst 0–4 entries `uniform` n=4 1.837 [1.780–1.896]; worst 8–64 `clustered` n=8 1.527 [1.483–1.571] (limit 2).
- **G2:** the two exception rows pass outright: `split-line-translated` 10² 3.663 [3.542–3.789] and `uniform` 10³ 3.747 [3.554–3.950] (limit 4). 10⁴–10⁵: worst pooled 1.891 [1.853–1.930] (limit 2).
- **G9:** all 15 rows pass in the matrix; worst `split-line-translated` 10⁵ 3.455 [3.272–3.648], then `split-line` 10⁵ 3.374 and `clustered` 10³ 3.287 (limit 4). The `int32` repeat processes also measured the translated row, at 3.385 [3.290–3.483] and 3.524 [3.424–3.626]. Those values are published but not used.
- **Persistent:** worst G7 1.258 [1.202–1.316] (`uniform` 10³ `static/full`, limit 2); G8a ≤ 20.6 ns (limit 1 µs); G8b 3.365 [3.196–3.543] (`mixed-size`, limit 4); G8c 1.159 [1.095–1.226] (limit 2).

### Watch items from the handoff

- **`int32 uniform n=4`:** matrix verdict **pass**, 1.837 [1.780–1.896]. The candidate matrix had been inconclusive at upper 2.041. In the calibration processes it was inconclusive in `calib-a` (1.651 [1.214–2.244]) and passed in `calib-b`, `calib-c` and `calib-d`.
- **`float clustered n=2`:** matrix **pass**, 1.496 [1.440–1.554]. The output paths are 100/96 characters (`results.csv`/`raw.csv`); the directory is 88. Adapters are unchanged.
- **`int64 split-line-translated n=100000` G2:** **pass**, 1.767 [1.707–1.830] (`7cfbebe`: 1.932 [1.876–1.990]).

## Calibration

- **`calib-a` vs `calib-b`** ([diff](data/calib-diff.txt)): 281 same, 9 faster and 8 slower of 298; geometric mean 0.992. Two groups explain most of the 17:
  - `clustered` n=0 is 0.51–0.55× in all five engines, including `brute`. This is the per-process placement effect at tiny sizes that the manifest documents.
  - `outliers` 10⁵ `index`, `index-ordered` and `sweep` are 1.31–1.56× slower. They ran during the thermal event above.
  - The other 9 rows differ by 3–7%.
- **`calib-c` vs `calib-d`** ([diff](data/calib-diff-cd.txt), supplementary, run after the matrices on a quiet host): 291 same, 3 faster, 4 slower (97.7%); geometric mean 1.004.
- **Gates in the calibration processes:** no hard `FAIL` in any of them. `calib-c` and `calib-d` pass every `int32` snapshot hard gate. `calib-a` and `calib-b` have wide-interval inconclusives only (G1 `uniform` n=2/4; G2 and G4 `split-line*` 10³ in `calib-b`).
- **Persistent:** the 3% persistent process term was not re-validated, as in earlier release runs.

## Published, not gated

- **G5 pathology** ([g5-pathology.csv](data/g5-pathology.csv)): `index` vs the best timed baseline. Worst `int32` row per scene: `outliers` 8.36×, `long-thin` 5.70×, `tiles` 4.81×, `tiles-touch` 4.63×, `points` 3.86×, `pile` 3.35×, `disjoint` 2.42×. Other types fall within 2.2–10.1× on the same scenes.
- **Library timing, work and memory** ([library-rows.csv](data/library-rows.csv)): for every `index`/`index-ordered` row, the estimate, error, median, p95, phase medians, candidates, node visits, pairs, checksum, warm and cold allocations, cold peak and retained bytes. No row has a nonzero warm allocation count. Largest cold peaks: 39.7 MB snapshot (`double split-line-translated` 10⁵ `index-ordered`) and 49.7 MB persistent (`double uniform` 10⁵ `churn-10%/full` `index-ordered`), the same as earlier runs.
- **Raw data per stage:** `results.csv`, `raw.csv.gz` (all samples), `gates.txt`, `run.log`, `time.log`, `args.txt`, `exit.txt` (benchmark and gate-evaluator exits), `quiet-start.csv`, `path-lengths.txt`.

## Decision

**Per the rule:** "Below [95%], timing verdicts are published but `int32` timing sign-off is withheld." `calib-a`/`calib-b` reached 94.3%, so this run does **not** extend the `int32` release sign-off by itself. Everything else the rule requires holds:
- every `int32` hard gate passes, with no exception rows and no open rows;
- G0 and G6 pass for all four types;
- CTest passes 22/22, and hosted Portability passes 9/9 on the exact SHA.

`calib-c`/`calib-d` were not part of the rule. They are published as additional evidence that the 2% snapshot process term holds on this host when it is quiet.

**Owner decision needed**, one of:
1. Accept `calib-c`/`calib-d` (97.7%) as the calibration for this candidate, given the documented thermal event in `calib-b`. The run then signs off `int32` on `c13e498`.
2. Require a fresh calibration pair plus an `int32` snapshot matrix under the same rule before sign-off.

The historical sign-off (`7cfbebe`) is unchanged until that decision.

## Reproduction

1. `git archive c13e498 | tar -x -C WORK/src`. Then run `cmake -S WORK/src -B WORK/build -G Ninja -DCMAKE_BUILD_TYPE=Release -DQUADTREE_BUILD_BENCHMARKS=ON -DQUADTREE_BENCH_NATIVE=OFF` and `cmake --build WORK/build`. The build downloads Box2D v3.2.0 and verifies its hash. Run `ctest` in `WORK/build`.
2. Save the `.text` hash: `objcopy --dump-section .text=WORK/bench.text WORK/build/bench/quadtree-bench WORK/copy`.
3. Run `bash matrices.sh WORK data`. To resume after an aborted persistent stage, set `QT_FROM=persistent`.
4. Supplementary calibration: `bash run.sh WORK data calib-c calib-d`, then `quadtree-bench --diff=data/calib-c/results.csv,data/calib-d/results.csv`.
5. Summaries: `python3 summarize.py data`. Hosted CI record: `python3 collect-ci.py .`.
