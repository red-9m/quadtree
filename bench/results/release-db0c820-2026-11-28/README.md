# 08 · Four-type release validation of `db0c820` (2026-11-28)

**Under the [pre-specified rule](resolution-rule.md), `int64`, `float` and `double` pass every hard gate on `db0c820`. Task 09 is accepted for them on this commit.** `int32` passes every hard snapshot gate and all but one hard persistent gate. That one row, G8b `mixed-size` n=10⁵ `small-single/moved`, stays open: pooled 3.901 [3.781–4.024], limit 4. Per the rule, the `int32` release acceptance does not extend to `db0c820` and stays on `c13e498`, pending an owner decision ([below](#open-int32-row)).

`db0c820` is `ce92ef5` plus the [fix for ordered `double` queries raising floating-point exceptions](../double-ordered-fp-flags-2026-11-28/README.md). Every `int32` `Index` function in the benchmark binary is instruction-identical between the two. The [`ce92ef5` validation](../release-ce92ef5-2026-11-28/README.md) passed this G8b row at 3.324 [3.163–3.494], and `c13e498` at 3.365.

| Type | G1 (16) | G2 (20) | G9 (15) | G3 / G4 / G6 snapshot | Persistent hard gates |
|---|---|---|---|---|---|
| `int32` | **16 pass** | **20 pass** | **15 pass** | 9 / 8 / 62 pass | G6 216, G7 120, G8a 12, G8c 24 pass; **G8b 3 pass, 1 open** |
| `float` | **16 pass** | **20 pass** | **15 pass** | 9 / 8 / 62 pass | G6 216, G7 120, G8a 12, G8b 4, G8c 24 pass |
| `double` | **16 pass** | **20 pass** | **15 pass** | 9 / 8 / 62 pass | G6 216, G8a 12, G8b 4 pass (no Box2D reference) |
| `int64` | **16 pass** | **20 pass** | **15 pass** | 9 / 8 / 62 pass | G6 216, G8a 12, G8b 4 pass (no Box2D reference) |

- **G0:** every benchmark and gate-evaluator process exits 0.
- **G6:** zero warm allocations on all 2,224 library rows.
- **Calibration:** `calib-a`/`calib-b` agree on **295/298 rows (99.0%)**, with a geometric mean of 0.998.
- **[CTest](ctest.log) 22/22** and **[full differential fuzz](fuzz.log) 20/20** on the archive build.
- **Hosted [Portability #27](https://github.com/red-9m/quadtree/actions/runs/46138155025): 9/9** on exactly `db0c820` ([ci.json](ci.json)).

## Open `int32` row

| Process | G8b `int32 mixed-size` n=10⁵ `small-single/moved` vs n=10³ |
|---|---|
| matrix (`int32-persistent`) | 3.843 [2.903–5.086], inconclusive |
| `int32-presolve` (45 rounds) | 3.863 [3.695–4.039] |
| `int32-presolve-2` (45 rounds) | 3.939 [3.771–4.114] |
| **pooled, per rule** | **3.901 [3.781–4.024], inconclusive: open** |

**Post-hoc diagnostic** ([diag-g8b](diag-g8b/), not part of the rule and not used for its decision). The same `presolve` arguments were run on the three release binaries in rotating order, three processes each, under the same protocol:

| Binary | Three 45-round processes |
|---|---|
| `c13e498` (accepted release) | 3.836 [3.665–4.015], 4.011 [3.841–4.188], 3.820 [3.652–3.996] |
| `ce92ef5` | 3.985 [3.818–4.159], 3.976 [3.807–4.152], 3.836 [3.668–4.013] |
| `db0c820` | 3.887 [3.714–4.069], 3.981 [3.813–4.156], 4.004 [3.833–4.182] |

In this two-row process, the accepted `c13e498` measures the same ratio as `db0c820`, about 3.8–4.0. The full persistent matrices of `c13e498` and `ce92ef5` gave 3.32–3.37. So the repeat processes measure a higher value than the matrix for every binary, not a `db0c820` regression. This G8b row is a per-step ratio of one moved entry at n=10⁵ to n=10³, dominated by memory latency. The `int32` code is unchanged. The rule allows no further repeats, so the row stays open as measured.

## Source, build and environment

- **Source:** `git archive` of `db0c82025d200dabf6de1005fa06180326142af2` ([source-head.txt](source-head.txt)). Workspace and cache values: [build.txt](build.txt).
- **Build:** Ninja Release `-O3 -DNDEBUG`, GCC 16.2.1, CMake 4.4.2, `QUADTREE_BENCH_NATIVE=OFF`, pinned Box2D v3.2.0 archive (`ffec747b…`), with no local source override.
- **Hashes:** `quadtree-bench` `e015f86d…`, benchmark `.text` `257d2628…` ([text.sha256](text.sha256)), runtime header `81b2795b…`. These are byte-identical to the binary measured in the [fix evidence](../double-ordered-fp-flags-2026-11-28/README.md). All 4 `data/text-check-*.log` are `OK`.
- **Machine and protocol:** Ryzen 9 5900HS, HPET clock source, EPP and platform profile `performance`, restored afterwards. Stages run pinned to CPU 6 with sibling 7 idle, with quiet starts at ≤ 40 °C; stages started at 39.4–40.0 °C and peaked at 58.2–73.6 °C. Records: [environment](data/environment.txt), [run order](data/run-order.csv), [thermal](data/thermal.csv).

## Run

[resolution-rule.md](resolution-rule.md), [run.sh](run.sh) and [matrices.sh](matrices.sh) were committed (`8858e87`) before any stage ran. The runner is identical to the [`ce92ef5` validation](../release-ce92ef5-2026-11-28/README.md). Arguments are the manifest defaults.

| Stages | When (11-28) | Time |
|---|---|---:|
| `calib-a`, `calib-b` | 13:07–13:12 | 2 × 2.2 min |
| `int32`/`float`/`double`/`int64-snapshot` | 13:12–13:22 | 2.1–2.3 min each |
| `int32`/`float`/`double`/`int64-persistent` | 13:23–15:19 | 33.1 / 33.7 / 24.4 / 23.9 min |
| `int32-presolve`, `-presolve-2` (rule repeats for G8b) | 15:19–15:20 | 2 × 0.2 min |

No stage aborted and no snapshot repeat was needed. **Reconciliation:** all 6,944 expected engine rows are present, with the same caps as earlier runs ([reconciliation.csv](data/reconciliation.csv)).

## Margins

Final status: [gate-rows.csv](data/gate-rows.csv), [snapshot](data/final-snapshot-status.txt), [persistent](data/final-persistent-status.txt), [summary](data/summary.txt). The worst row per snapshot gate:

| Type | G1 0–4 (limit 2) | G1 8–64 | G2 10²–10³ (limit 4) | G2 10⁴–10⁵ (limit 2) | G9 (limit 4) |
|---|---|---|---|---|---|
| `int32` | `uniform` n=4 1.851 [1.774–1.932] | `clustered` n=8 1.576 [1.531–1.622] | `uniform` 10³ 3.842 [3.718–3.970] | `split-line-translated` 10⁵ 1.895 [1.839–1.952] | `split-line-translated` 10⁵ 3.486 [3.371–3.605] |
| `float` | `uniform` n=4 1.704 [1.651–1.759] | `clustered` n=8 0.624 [0.589–0.661] | `split-line-translated` 10² 3.559 [3.448–3.673] | `split-line-translated` 10⁵ 1.729 [1.680–1.780] | `split-line` 10⁵ 3.693 [3.579–3.811] |
| `double` | `clustered` n=1 1.708 [1.639–1.779] | `clustered` n=8 0.662 [0.641–0.683] | `uniform` 10³ 3.477 [3.355–3.603] | `uniform` 10⁵ 1.841 [1.783–1.900] | `split-line` 10⁵ 3.668 [3.484–3.863] |
| `int64` | `uniform` n=4 1.764 [1.711–1.818] | `clustered` n=8 1.243 [1.206–1.281] | `split-line-translated` 10² 3.497 [3.387–3.611] | `uniform` 10⁵ 1.873 [1.811–1.937] | `split-line-translated` 10⁵ 3.497 [3.374–3.625] |

- **Persistent, worst passing row:**
  - G7: `int32` 1.311 [1.251–1.373], `float` 1.268 [1.214–1.324].
  - G8a: ≤ 20.6 ns for every type.
  - G8b: `float` 3.346 [3.196–3.503], `double` 3.536 [3.335–3.750], `int64` 3.447 [3.268–3.636]. The other three `int32` rows are ≤ 1.669.
  - G8c: `int32` 1.159 [1.102–1.219], `float` 1.206 [1.149–1.266].
- **Soft gates (vs grid, published):** G7 107–110 pass per type; G8c 16 pass per type, with 2–4 fail and 4–6 inconclusive. This is the same pattern as earlier runs.
- **Published only** ([g5-pathology.csv](data/g5-pathology.csv), [library-rows.csv](data/library-rows.csv)): largest cold peaks are 39.7 MB snapshot and 49.7 MB persistent, the same as earlier runs.

## Decision

Per [resolution-rule.md](resolution-rule.md):
- **`int64`, `float`, `double`:** every hard snapshot gate (G1 16, G2 20, G3, G4, G6, G9 15) and every applicable hard persistent gate passes, with no exception. **Task 09 is accepted for all three on `db0c820`.**
- **`int32`:** every hard snapshot gate passes, including all 15 G9 rows. One hard persistent row (G8b above) is still open after the rule's repeats. **The `int32` release acceptance does not extend to `db0c820` under this rule. It stays on `c13e498`; `ce92ef5` also passed every `int32` gate, and its `int32` code is instruction-identical to `db0c820`'s.** Extending it needs an owner decision: accept on the evidence above, or rerun under a new pre-specified rule.
- **All types:** G0 and G6 pass. Calibration is 99.0%. CTest 22/22, full fuzz 20/20, hosted Portability 9/9 on the exact SHA.

## Reproduction

As for [`ce92ef5`](../release-ce92ef5-2026-11-28/README.md#reproduction), with `git archive db0c820`. Diagnostic: `bash diag-g8b/run.sh BIN_DIR OUT`, where `BIN_DIR` holds the three release binaries as `c13e`, `ce92` and `db0c`.
