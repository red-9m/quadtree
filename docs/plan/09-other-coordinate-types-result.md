# 09 · Other coordinate types: result

**Closed as is (owner decision, 2026-11-22).** `int64`, `float` and `double` ship as functionally supported coordinate types, with their performance gaps published. They are **not** performance-accepted. The frozen limits are unchanged, and no G1/G2 exception extends beyond `int32`.

## What shipped

| Commit | Change | Evidence |
|---|---|---|
| `df2cfeb` | `int64` uses the integer bulk planner, commit prefetch and ordered fast paths | [candidate](../../bench/results/int64-bulk-plan-2026-11-21/README.md) · [staged recheck](../../bench/results/int64-staged-2026-11-21/README.md) |
| `d52dbe1` | `float`/`double` use the bulk planner, warm flat build, and packed SSE2 cross-item overlap (also in the small-set loop); ordered paths and prefetch apply to every type | [candidate](../../bench/results/float-bulk-plan-2026-11-21/README.md) · [staged recheck](../../bench/results/float-staged-2026-11-21/README.md) |

Effect against the [release run](../../bench/results/release-acceptance-2026-11-21/README.md):
- **`int64`:** `index` 5–34% faster, `index-ordered` 14–32% faster.
- **`float`/`double`:** `index` 24–72% faster.
- **Work counts:** topology, handles, ranks and exact pair sequences are unchanged.
- **CI and `int32`:** hosted Portability passes 9/9 on both commits, and the `int32` release acceptance holds on both ([08 result](08-final-acceptance-result.md)).

## Published gaps (from [final-status.txt](../../bench/results/float-staged-2026-11-21/data/final-status.txt), `d52dbe1`)

| Type | G1 (16) | G2 (20) | Other hard gates |
|---|---|---|---|
| `int64` | 8 pass; 0–4 entries fail at ≤ 3.15× upper | 16 pass, 1 fail (`uniform` 10⁵, 2.13×), 3 unresolved (≈4.0 / 4.03 / 2.03) | G3, G4, G6, G9 and applicable persistent gates pass |
| `float` | 8 pass; 0–4 entries fail or unresolved, ≤ 3.09× upper | 10 pass, 8 fail (4.16–4.48× at 10²–10³, 2.08–2.15× at 10⁴–10⁵), 2 unresolved | all pass |
| `double` | 8 pass; 0–4 entries fail, ≤ 3.21× upper | 10 pass, 8 fail (4.19–4.51× at 10²–10³, 2.13–2.18× at 10⁴–10⁵), 2 unresolved | all pass |

Every failing row is within about 15% of its limit. The G1 0–4-entry rows sit in the same range as the accepted `int32` exception.

## Reopening

A future task can close these rows with more build and pair speed for floats: the gap to `int32` is about 10% at 10⁵ and 10–15% at 10²–10³. For `int64` `uniform` 10⁵, the remaining cost follows the 32-byte box. Alternatively, an owner decision can extend the exceptions. In either case, start from the `d52dbe1` evidence and keep the `int32` acceptance rechecked.
