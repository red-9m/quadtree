# 09 · Performance acceptance for `int64`, `float` and `double`

**Goal:** meet G1 and G2 for `int64`, `float` and `double` snapshots, then rerun the full [08](08-final-acceptance.md) matrix for those types. Follows the `int32` release (owner decision, 2026-11-21). Requirements: `QT-NFR-01`–`QT-NFR-03`, `QT-NFR-11`.

**Start from:** the [release acceptance run](../../bench/results/release-acceptance-2026-11-21/README.md), using its [gate rows](../../bench/results/release-acceptance-2026-11-21/data/gate-rows.csv) and [library rows](../../bench/results/release-acceptance-2026-11-21/data/library-rows.csv).

| Gate | Blocking rows |
|---|---|
| G2, `int64` | 11 (`split-line`, `split-line-translated`, `uniform` 10³–10⁵); 3 unresolved |
| G2, `float` / `double` | 18 each, 2.4–11.5× the best baseline |
| G1, `float` / `double` | 1, 2, 4 and 8 entries; the 1–4-entry rows are above the 3.5× exception cap |
| G9, `int64` `mixed-size` 10⁵ | unresolved |

**Read only:** `int32`-specific branches in [the implementation](../../detail/quadtree-index-impl.h): `std::same_as<T, std::int32_t>` (bulk planning, packed cross-item overlap, prefetch) and `!FloatCoordinate<T>` (warm flat build). Also the [Task 03 evidence](../../bench/results/g2-packed-cross-item-confirm-2026-11-21/README.md) on which `int32` paths closed G2.

**Hypothesis (unverified):** the other types take the generic paths that `int32` left behind in Task 03. Confirm with phase medians (`prepare_median_ns` vs `pairs_median_ns`) per type before changing code.

**Work:** generalize or add type-specific fast paths. Keep exact geometry: float signed zero and nonfinite rejection, and `int64` range. Keep all contracts, and keep `int32` code generation unchanged or rechecked. No threshold or fixture changes.

**Exit:** for each type, every G2 row passes; G1 passes or falls within the accepted exception; G9 passes; G0/G3/G4/G6 hold. All of it shown on a full 08 matrix run of the committed source, and `int32` must not regress.
