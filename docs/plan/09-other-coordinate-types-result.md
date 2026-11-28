# 09 · Other coordinate types: result

**Accepted (2026-11-28) on `ce92ef5`.** `int64`, `float` and `double` are performance-accepted. On the [four-type release validation](../../bench/results/release-ce92ef5-2026-11-28/README.md), every type passes G1 16/16, G2 20/20, G9 15/15, G3, G4, G6 and every applicable persistent hard gate. The run needed no repeats and used no exceptions, under a [rule committed before it ran](../../bench/results/release-ce92ef5-2026-11-28/resolution-rule.md). `int32` passes every hard gate on the same run, so its release acceptance holds. The frozen limits are unchanged, and no exception extends beyond `int32`.

History:
- First closed as is on `d52dbe1` (owner, 2026-11-22), then reopened the same day.
- Closed as is again on `7cfbebe` (owner, 2026-11-23), with published gaps (below).
- Reopened for the [common-filter pair kernels](#common-filter-pair-kernels-ce92ef5).

## Common-filter pair kernels (`ce92ef5`)

[Candidate evidence](../../bench/results/float-common-pairs-2026-11-27/README.md) · [release validation](../../bench/results/release-ce92ef5-2026-11-28/README.md)

- **Cause:** `float` and `double` had no common-filter fast path, which `int64` already had.
  - At `c13e498`, unordered `int64` pairs at `uniform` 10³ took 31.8 µs; `float` took 43.7 µs and `double` 44.1 µs.
  - Their ordered pairs used the general recursive walk.
- **Change:**
  - Bulk `float`/`double` builds record the common-filter-bit proof.
  - Unordered full pairs use the `CommonBitPairs` kernel with the exact packed overlap test.
  - Ordered full pairs use `orderedCommonFloat`, a compact ordered walk on float lanes. It is exact for `float`. For `double`, lanes are rounded toward −∞, every lane hit is rechecked exactly, and exact thread-node bounds keep node charges equal to the general walk's.
  - `orderedCommon` becomes `noinline`, so `int32` code generation stays independent of this code.
- **Effect, candidate vs `c13e498`, same machine:**

  | Type | `index` 10²–10³ | `index` 10⁴–10⁵ | `index-ordered` 10²–10⁵ |
  |---|---|---|---|
  | `float` | 0.5–40% faster | 1.4–21% faster | 5–44% faster |
  | `double` | 0–46% faster | 0.4–23% faster | 5–42% faster |
  | `int32` | −1.3% to +1.2% | −1.0% to +2.0% | −5.3% to +4.4% |

  Work counts, pair sequences and checksums are unchanged.
- **Costs** (`pile` is published under G5; neither row is gated):
  - `pile` unordered pairs: 19–28% slower for `float`/`double`, as for `int64`. `pile` ordered pairs: 33–80% faster.
  - `int64` unordered: 1–7% slower at 10²–10⁴ from compiler inlining shifts. All `int64` gates still pass.
- **Rows open at `c13e498`, now passing:**
  - `float` G2 `uniform` 10³ and G9 `split-line-translated` 10⁵.
  - `double` G2 `uniform` 10³ and 10⁵.
  - They pass in the candidate matrix, in two 45-round processes and in the release matrix.

Tightest rows in the release matrix, all passing:
- G9 at 10⁵: `float split-line` 3.699 [3.586–3.815] and `double split-line-translated` 3.656 [3.539–3.777].
- G2: `double split-line-translated` 10² 3.569 [3.455–3.687].

## What shipped before `ce92ef5`

| Commit | Change | Evidence |
|---|---|---|
| `df2cfeb` | `int64` uses the integer bulk planner, commit prefetch and ordered fast paths | [candidate](../../bench/results/int64-bulk-plan-2026-11-21/README.md) · [staged recheck](../../bench/results/int64-staged-2026-11-21/README.md) |
| `d52dbe1` | `float`/`double` use the bulk planner, warm flat build, and packed SSE2 cross-item overlap (also in the small-set loop); ordered paths and prefetch apply to every type | [candidate](../../bench/results/float-bulk-plan-2026-11-21/README.md) · [staged recheck](../../bench/results/float-staged-2026-11-21/README.md) |
| `7cfbebe` (after reopening) | Packed overlap tests in `selfPairs`, `itemsVsSubtree` and `orderedSelf` for `int32`/`float`/`double`; branch-free float `Box` capture. `int64` keeps `detail::overlaps`. | [candidate](../../bench/results/packed-pairs-2026-11-22/README.md) · [staged recheck](../../bench/results/packed-staged-2026-11-23/README.md) |

Effect:
- **`df2cfeb`/`d52dbe1`, against the [release run](../../bench/results/release-acceptance-2026-11-21/README.md):** `int64` `index` 5–34% faster and `index-ordered` 14–32% faster; `float`/`double` `index` 24–72% faster.
- **`7cfbebe`, against `d52dbe1`:** G2 fails drop from 8 to 0 for `float` and from 8 to 1 for `double`; `int64` is not targeted.
- **Work counts:** topology, handles, ranks and exact pair sequences are unchanged.
- **CI and `int32`:** hosted Portability passes 9/9 on all three commits, and the `int32` release acceptance holds on all three ([08 result](08-final-acceptance-result.md)).

## Published gaps (`7cfbebe`)

From [final-status.txt](../../bench/results/packed-staged-2026-11-23/data/final-status.txt). Limits: G1 2× brute force; G2 4× the best baseline at 10²–10³ and 2× at 10⁴–10⁵; G9 4× the unordered step.

| Type | G1 (16) | G2 (20) | Other hard gates |
|---|---|---|---|
| `int64` | 8 pass; 0–4 entries fail, ≤ 3.24× upper | 15 pass, 2 fail (`split-line-translated` 10² 4.29×, `uniform` 10⁵ 2.16×), 3 unresolved (4.02–4.03 at 10²–10³) | G3, G4, G6, G9 and applicable persistent gates pass |
| `float` | 10 pass; 1–4 entries fail, ≤ 3.26× upper | 19 pass, 1 unresolved (`uniform` 10³ 4.02) | G9 14 pass, 1 unresolved (`split-line-translated` 10⁵ 4.03); the rest pass |
| `double` | 8 pass; 0–4 entries fail, ≤ 3.58× upper | 15 pass, 1 fail (`uniform` 10³ 4.23×), 4 unresolved (3.98–4.08 at 10²–10³; `uniform` 10⁵ 1.97) | all pass |

Notes:
- The `int64` `split-line-translated` 10² fail is a 5-round matrix verdict. The same row measured 4.03 [3.95–4.11] in the candidate and was unresolved on `d52dbe1`.
- The `double` G1 n=1 upper bounds (3.52× and 3.58×) exceed the `int32` exception cap of 3.5×; the other types' G1 rows are within it.
- **Causes:** `int64` and `double` boxes are 32 bytes, against 16 for `int32`, and build and pair cost follow that size. The `int64` overlap test is not packed. G1 0–4 entries carry the same fixed rebuild cost as the accepted `int32` exception.

## Gaps at the first closure (`d52dbe1`)

From [final-status.txt](../../bench/results/float-staged-2026-11-21/data/final-status.txt).

| Type | G1 (16) | G2 (20) | Other hard gates |
|---|---|---|---|
| `int64` | 8 pass; 0–4 entries fail at ≤ 3.15× upper | 16 pass, 1 fail (`uniform` 10⁵, 2.13×), 3 unresolved (≈4.0 / 4.03 / 2.03) | G3, G4, G6, G9 and applicable persistent gates pass |
| `float` | 8 pass; 0–4 entries fail or unresolved, ≤ 3.09× upper | 10 pass, 8 fail (4.16–4.48× at 10²–10³, 2.08–2.15× at 10⁴–10⁵), 2 unresolved | all pass |
| `double` | 8 pass; 0–4 entries fail, ≤ 3.21× upper | 10 pass, 8 fail (4.19–4.51× at 10²–10³, 2.13–2.18× at 10⁴–10⁵), 2 unresolved | all pass |

## Reopening options at `7cfbebe` (superseded)

Start from the `7cfbebe` evidence and keep the `int32` acceptance rechecked after any change. Pre-specify each run's resolution rule before it starts. Options, ranked by expected value:
1. **`int64` rebased snapshot path (design change).** When a build's span fits in 32 bits, store offsets from the minimum and reuse the `int32` kernels. It stays exact, but it only helps such inputs, so publish that caveat. Persistent inserts that leave the range need a fallback.
2. **`double` float prefilter (design change).** Keep outward-rounded `float` box copies for traversal and pair filtering, with the exact `double` test on candidates only. Unverified whether the rechecks eat the saving at 10³.
3. **Confirm the `int64` `split-line-translated` 10² fail:** two 45-round processes in a new recheck directory, under a new rule.

Rejected so far: packed node-bound tests (G9 4.0–4.2 at 10⁵), `splitThreshold` 12/16, per-entry inline midpoint, and branch-free scalar `int64` overlap. Alternatively, an owner decision can extend the exceptions.
