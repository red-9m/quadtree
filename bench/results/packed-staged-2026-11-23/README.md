# 09 · Staged packed pair tests (`7cfbebe`): recheck

**Complete.** Commit `7cfbebe` stages the validated [packed-pairs candidate](../packed-pairs-2026-11-22/README.md).
- **CI:** hosted [Portability #10](https://github.com/red-9m/quadtree/actions/runs/45135921885) passes 9/9.
- **`int32`:** release acceptance **holds on `7cfbebe`**. Every snapshot and persistent hard gate passes; G1 0–4 entries are within the accepted exception, and neither G2 exception row is needed.
- **`float`:** G2 19 pass, 0 fail, 1 inconclusive; G9 14 pass, 1 inconclusive. G1 1–4 entries fail.
- **`double`:** G2 15 pass, 1 fail, 4 inconclusive; G9 15 pass. G1 0–4 entries fail.
- **`int64`:** G2 15 pass, 2 fail, 3 inconclusive; G9 15 pass. G1 0–4 entries fail.
- **Persistent:** every applicable hard gate passes for every type.

## Method

- **Build:** `git archive` of `7cfbebe` (later commits change only results and docs), Release CMake build with pinned Box2D `178495ba…`, CTest 21/21. Benchmark `.text` `c01f6323…` equals the validated candidate's ([text.sha256](text.sha256)); each invocation rechecks it (`data/text-check-*.log`, all `OK`).
- **Stages:** [run.sh](run.sh), under the [resolution rule](resolution-rule.md), which was committed before any stage ran.
  - 5-round snapshot matrices for all four types (2026-11-23 10:07–10:18).
  - 64-step persistent matrices for all four (19:01–20:58). The first attempt at `int32-persistent` was killed after 9 minutes (exit 143). Its partial directory is kept as [`int32-persistent-aborted/`](data/int32-persistent-aborted/) and is not evidence; the stage was rerun from scratch.
  - Every hard snapshot row inconclusive in its matrix was rerun in two 45-round processes with identical arguments (21:00–21:11) and decided by their pooled interval ([resolve.py](resolve.py), [final-status.txt](data/final-status.txt)). Arguments per type are in the table below and in each stage's `args.txt`.
- **Environment:** same machine and power settings as earlier runs, CPU 6, quiet starts ≤ 40 °C, power restored after each invocation ([run order](data/run-order.csv), [environment](data/environment.txt), [thermal](data/thermal.csv)). `environment.txt` records the harness revision of the last invocation (`0b762ad`).
- **Exit codes:** every benchmark process exited 0 apart from the aborted stage, so cross-checks and determinism are clean. Nonzero second fields in `exit.txt` are gate-file exits for failing or inconclusive rows.

| Type | Inconclusive matrix rows | `QT_RESOLVE_ARGS` |
|---|---|---|
| `int32` | G1 `clustered`/`uniform` n=0 | `--scenes=uniform,clustered --sizes= --small-sizes=0` |
| `float` | G1 `clustered` n=0; G2 `uniform` 10³; G9 `mixed-size`/`split-line-translated` 10⁵ | `--scenes=uniform,clustered,mixed-size,split-line-translated --sizes=1000,100000 --small-sizes=0` |
| `double` | G1 n=0 (both scenes); G2 `split-line` 10²/10³, `split-line-translated` 10²/10³/10⁵, `uniform` 10³/10⁵; G9 `mixed-size`/`split-line` 10⁵ | `--scenes=uniform,clustered,split-line,split-line-translated,mixed-size --sizes=100,1000,100000 --small-sizes=0` |
| `int64` | G1 `clustered` n=0; G2 `split-line` 10²/10³/10⁴, `uniform` 10³ | `--scenes=uniform,clustered,split-line --sizes=100,1000,10000 --small-sizes=0` |

## Results

Final verdicts after pooling ([final-status.txt](data/final-status.txt)):

| Type | G1 (16) | G2 (20) | G9 (15) | G3/G4/G6 | Persistent hard gates |
|---|---|---|---|---|---|
| `int32` | 8 pass, 8 accepted exception (≤ 3.11 upper) | **20 pass** | 15 pass | pass | G6 216, G7 120, G8a 12, G8b 4, G8c 24: pass |
| `float` | 10 pass, 6 fail (1–4 entries, ≤ 3.26 upper) | 19 pass, 1 inconclusive | 14 pass, 1 inconclusive | pass | G6, G7, G8a, G8b, G8c pass |
| `double` | 8 pass, 8 fail (0–4 entries, ≤ 3.58 upper) | 15 pass, 1 fail, 4 inconclusive | 15 pass | pass | G6, G8a, G8b pass (no Box2D reference) |
| `int64` | 8 pass, 8 fail (0–4 entries, ≤ 3.24 upper) | 15 pass, 2 fail, 3 inconclusive | 15 pass | pass | G6, G8a, G8b pass (no Box2D reference) |

Persistent matrices have soft-gate failures only (G7/G8c soft), as in earlier runs.

**`int32`:** the release acceptance **holds on `7cfbebe`**.
- G2 passes 20/20 in the matrix, so neither G2 exception row is used.
- G1 n=0 pools to 2.08 [2.02–2.13] (`clustered`) and 2.08 [2.03–2.13] (`uniform`). G1 1–4 entries: ≤ 3.11 upper. All are within the ≤ 3.5× exception.
- G3, G4, G6 and G9 pass on every row, as do all persistent hard gates.

**Open non-`int32` rows** (limits 4× at 10²–10³, 2× at 10⁴–10⁵):

| Type | Gate | Row | Ratio [interval] | Verdict |
|---|---|---|---|---|
| `int64` | G2 | `split-line-translated` 10² | 4.29 [4.15–4.43] (matrix) | fail |
| `int64` | G2 | `uniform` 10⁵ | 2.16 [2.07–2.26] (matrix) | fail |
| `int64` | G2 | `split-line` 10² / 10³ | 4.02 [3.94–4.10] / 4.03 [3.95–4.11] | inconclusive |
| `int64` | G2 | `uniform` 10³ | 4.03 [3.95–4.11] | inconclusive |
| `double` | G2 | `uniform` 10³ | 4.23 [4.14–4.32] | fail |
| `double` | G2 | `split-line` 10³ | 3.99 [3.90–4.07] | inconclusive |
| `double` | G2 | `split-line-translated` 10² / 10³ | 3.98 [3.90–4.06] / 4.08 [3.99–4.16] | inconclusive |
| `double` | G2 | `uniform` 10⁵ | 1.97 [1.93–2.01] | inconclusive |
| `float` | G2 | `uniform` 10³ | 4.02 [3.94–4.11] | inconclusive |
| `float` | G9 | `split-line-translated` 10⁵ | 4.03 [3.95–4.12] | inconclusive |

Pooled values are from the two 45-round processes; matrix values keep their 5-round verdict under the rule.

Against the [`d52dbe1` recheck](../float-staged-2026-11-21/README.md#results), G2 fails drop from 8 to 0 for `float` and from 8 to 1 for `double`. `int64` goes from 1 fail to 2. Its pair test is unchanged by this commit (it falls back to `detail::overlaps`). The new `split-line-translated` 10² fail is a 5-round matrix verdict of 4.29; the same row was 4.03 [3.95–4.11] in the candidate and 4.03 unresolved on `d52dbe1`. Under the rule the matrix verdict stands. The `float` G9 row was also inconclusive in the candidate (4.01). Every open row is within about 8% of its limit.

## Next

Owner decision for Task 09:
1. Close 09 again with these gaps published.
2. Extend the G1/G2 (and `float` G9) exceptions to `int64`/`float`/`double`.
3. Continue optimizing: `int64` `uniform` 10⁵ (32-byte box), `double` `uniform` 10³, and the rows within 1–2% of their limits.
