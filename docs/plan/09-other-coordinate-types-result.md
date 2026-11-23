# 09 · Other coordinate types: result

**Reopened (owner, 2026-11-22); awaiting an owner decision after the `7cfbebe` recheck.** The [packed-pairs candidate](../../bench/results/packed-pairs-2026-11-22/README.md) is staged as `7cfbebe`, and its [recheck](../../bench/results/packed-staged-2026-11-23/README.md) is complete. The current gaps are [below](#current-gaps-7cfbebe). The earlier closure text describes the state at `d52dbe1`.

**Closed as is (owner decision, 2026-11-22).** `int64`, `float` and `double` ship as functionally supported coordinate types, with their performance gaps published. They are **not** performance-accepted. The frozen limits are unchanged, and no G1/G2 exception extends beyond `int32`.

## What shipped

| Commit | Change | Evidence |
|---|---|---|
| `df2cfeb` | `int64` uses the integer bulk planner, commit prefetch and ordered fast paths | [candidate](../../bench/results/int64-bulk-plan-2026-11-21/README.md) · [staged recheck](../../bench/results/int64-staged-2026-11-21/README.md) |
| `d52dbe1` | `float`/`double` use the bulk planner, warm flat build, and packed SSE2 cross-item overlap (also in the small-set loop); ordered paths and prefetch apply to every type | [candidate](../../bench/results/float-bulk-plan-2026-11-21/README.md) · [staged recheck](../../bench/results/float-staged-2026-11-21/README.md) |
| `7cfbebe` (after reopening) | Packed overlap tests in `selfPairs`, `itemsVsSubtree` and `orderedSelf` for `int32`/`float`/`double`; branch-free float `Box` capture. `int64` keeps `detail::overlaps`. | [candidate](../../bench/results/packed-pairs-2026-11-22/README.md) · [staged recheck](../../bench/results/packed-staged-2026-11-23/README.md) |

Effect against the [release run](../../bench/results/release-acceptance-2026-11-21/README.md):
- **`int64`:** `index` 5–34% faster, `index-ordered` 14–32% faster.
- **`float`/`double`:** `index` 24–72% faster.
- **Work counts:** topology, handles, ranks and exact pair sequences are unchanged.
- **CI and `int32`:** hosted Portability passes 9/9 on both commits, and the `int32` release acceptance holds on both ([08 result](08-final-acceptance-result.md)).

## Current gaps (`7cfbebe`)

From [final-status.txt](../../bench/results/packed-staged-2026-11-23/data/final-status.txt). Hosted Portability #10 passes 9/9, and the `int32` acceptance holds ([08 result](08-final-acceptance-result.md)).

| Type | G1 (16) | G2 (20) | Other hard gates |
|---|---|---|---|
| `int64` | 8 pass; 0–4 entries fail, ≤ 3.24× upper | 15 pass, 2 fail (`split-line-translated` 10² 4.29×, `uniform` 10⁵ 2.16×), 3 unresolved (4.02–4.03 at 10²–10³) | G3, G4, G6, G9 and applicable persistent gates pass |
| `float` | 10 pass; 1–4 entries fail, ≤ 3.26× upper | 19 pass, 1 unresolved (`uniform` 10³ 4.02) | G9 14 pass, 1 unresolved (`split-line-translated` 10⁵ 4.03); the rest pass |
| `double` | 8 pass; 0–4 entries fail, ≤ 3.58× upper | 15 pass, 1 fail (`uniform` 10³ 4.23×), 4 unresolved (3.98–4.08 at 10²–10³; `uniform` 10⁵ 1.97) | all pass |

Every open row is within about 8% of its limit. The `double` G1 n=1 upper bounds (3.52× and 3.58×) exceed the `int32` exception cap of 3.5×; the other types' G1 rows are within it.

## Gaps at closure (from [final-status.txt](../../bench/results/float-staged-2026-11-21/data/final-status.txt), `d52dbe1`)

| Type | G1 (16) | G2 (20) | Other hard gates |
|---|---|---|---|
| `int64` | 8 pass; 0–4 entries fail at ≤ 3.15× upper | 16 pass, 1 fail (`uniform` 10⁵, 2.13×), 3 unresolved (≈4.0 / 4.03 / 2.03) | G3, G4, G6, G9 and applicable persistent gates pass |
| `float` | 8 pass; 0–4 entries fail or unresolved, ≤ 3.09× upper | 10 pass, 8 fail (4.16–4.48× at 10²–10³, 2.08–2.15× at 10⁴–10⁵), 2 unresolved | all pass |
| `double` | 8 pass; 0–4 entries fail, ≤ 3.21× upper | 10 pass, 8 fail (4.19–4.51× at 10²–10³, 2.13–2.18× at 10⁴–10⁵), 2 unresolved | all pass |

Every failing row is within about 15% of its limit. The G1 0–4-entry rows sit in the same range as the accepted `int32` exception.

## Reopening

A future task can close these rows with more build and pair speed for floats: the gap to `int32` is about 10% at 10⁵ and 10–15% at 10²–10³. For `int64` `uniform` 10⁵, the remaining cost follows the 32-byte box. Alternatively, an owner decision can extend the exceptions. In either case, start from the `7cfbebe` evidence above and keep the `int32` acceptance rechecked.
