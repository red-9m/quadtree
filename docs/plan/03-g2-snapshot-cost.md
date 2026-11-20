# 03 · Close G2 for representative snapshots

**Goal:** meet [G2](../../bench/MANIFEST.md#frozen-acceptance-limits) on the current 01 slice. Start from the verdicts linked in the [task index](../../QUADTREE-UPDATE-PLAN.md); skip if green. Requirements: `QT-FR-13`, `QT-FR-16`–`QT-FR-18`, `QT-NFR-01`–`QT-NFR-03`.

**Read only:** `prepare_median_ns`, `pairs_median_ns`, candidates and node visits in the current snapshot CSV; [storage and complexity](../../INDEX.md#storage-and-complexity); `Index::buildRange`, `Index::countNodes`, `Index::selfPairs` and `Index::crossPairs` in [the implementation](../../detail/quadtree-index-impl.h).

**Work:** separate preparation from traversal before choosing a change. Historically `uniform`, `split-line`, and `split-line-translated` failed at 10⁴ and 10⁵; more rounds are needed for intervals crossing a limit. Compare the same fixture, baselines and full step, not isolated kernel timings. No changes to thresholds, workload mix, or ordered/moved-set semantics.

**Exit:** publish before/after step and phase times, work counts, allocations and per-row verdicts. All G2 rows in the slice pass, including previously inconclusive ones; check G0, G1, G3, G4, G6 and G9 on the combined change. Full type/size coverage belongs to 08.
