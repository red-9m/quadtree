# 09 · Staged `float`/`double` paths (`d52dbe1`): recheck

**Staged and verified.** Commit `d52dbe1` applies the validated [`float`/`double` patch](../float-bulk-plan-2026-11-21/README.md).
- **CI:** hosted [Portability #7](https://github.com/red-9m/quadtree/actions/runs/44968233000) passes 9/9.
- **`int32`:** release acceptance holds. Every hard gate passes, with the accepted exceptions.
- **`float`/`double`:** G2 has 10 pass, 8 fail, 2 unresolved each. G1 8–64 entries pass. G3, G4, G6, G9 and every applicable persistent gate pass.
- **`int64`:** unchanged from its staging: G2 1 fail, 3 unresolved.

## Method

- **Build:** `git archive` of `d52dbe1` (later commits change only results), Release CMake build with pinned Box2D, CTest 21/21. Benchmark `.text` `9040f7fc…` equals the validated candidate's ([text.sha256](text.sha256)).
- **Stages:** [run.sh](run.sh), in the order given in the [resolution rule](resolution-rule.md), which was committed before the run.
  - 5-round snapshot matrices for all four types, then 64-step persistent matrices for all four.
  - Every hard snapshot row inconclusive in its matrix was rerun in two 45-round processes with identical arguments and decided by their pooled interval ([resolve.py](resolve.py), [final-status.txt](data/final-status.txt)).
- **Environment:** same machine and power settings as earlier runs, CPU 6, quiet starts ≤ 40 °C, power restored ([run order](data/run-order.csv), [environment](data/environment.txt)).
- **Exit codes:** 0 for every run. Cross-checks and determinism are clean.

## Results

| Type | G1 (16) | G2 (20) | G9 | G3/G4/G6 | Persistent hard gates |
|---|---|---|---|---|---|
| `int32` | 8 pass, 8 accepted exception | **20 pass** | 15 pass | pass | G6 216, G7 120, G8a 12, G8b 4, G8c 24: pass |
| `int64` | 8 pass, 8 fail (0–4 entries, ≤ 3.15 upper) | 16 pass, 1 fail, 3 inconclusive | 15 pass | pass | G6, G8a, G8b pass (no Box2D reference) |
| `float` | 8 pass, 6 fail + 2 inconclusive (0–4 entries, ≤ 3.09 upper) | 10 pass, 8 fail, 2 inconclusive | 15 pass | pass | G6, G7, G8a, G8b, G8c pass |
| `double` | 8 pass, 8 fail (0–4 entries, ≤ 3.21 upper) | 10 pass, 8 fail, 2 inconclusive | 15 pass | pass | G6, G8a, G8b pass |

Notes on the `int32` result:
- `split-line-translated` 10² now passes outright: pooled 3.820 [3.744–3.898], limit 4.
- `uniform` 10³ passes in the 5-round matrix.
- `split-line-translated` 10⁵ passes in the matrix.
- The G1 0–4-entry rows are within the ≤ 3.5× exception (upper ≤ 3.19).

**The `int32` release acceptance holds on `d52dbe1`**, with no G2 exception row needed in this run.

`float`/`double` G2 failures:
- `split-line` and `split-line-translated` fail at 10²–10⁴: 4.16–4.48× against 4, and 2.13–2.16× against 2.
- `uniform` fails at 10³: 4.41× / 4.51×.
- `double` also fails `uniform` and `split-line-translated` at 10⁵ (2.15×, 2.18×); `float` fails `split-line-translated` 10⁵ (2.08×).
- Unresolved after pooling: `split-line` 10⁵ (≈2.0) for both types; `float` `uniform` 10⁵ (1.97 [1.93–2.01]); `double` `split-line` 10² (4.01 [3.93–4.09]).

`int64`: `uniform` 10⁵ fails at 2.13×. Unresolved after pooling: `uniform` 10³ (4.00), `split-line-translated` 10² (4.03) and 10⁵ (2.03).

## Next

Every remaining `int64`/`float`/`double` G2 row is within about 15% of its limit. G1 for these three types sits in the same range as the `int32` exception, but no exception covers them. Options:
1. More build/pair speed for floats. The gap to `int32` is about 10% at 10⁵ and about 10–15% at 10²–10³.
2. Extend the G1/G2 exceptions to the other types.
3. Accept the current state for Task 09 and ship `int64`/`float`/`double` as functionally supported, with published gaps.
