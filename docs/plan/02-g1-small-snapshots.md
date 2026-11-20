# 02 · Close G1 for small snapshots

**Goal:** meet [G1](../../bench/MANIFEST.md#frozen-acceptance-limits) for the `uniform` and `clustered` 0–64 batches. Start with the current 01 verdicts linked in the [task index](../../QUADTREE-UPDATE-PLAN.md). Skip if green; repeat inconclusive measurements before changing code. Requirements: `QT-NFR-11`, `QT-NFR-03`, `QT-FR-13`, `QT-FR-21`.

**Read only:** `LibraryIndex::build` and `LibraryIndex::pairs` in [bench-index.h](../../bench/bench-index.h); `Index::buildRange` and `Index::forEachPair` in [the implementation](../../detail/quadtree-index-impl.h); [build guarantees](../../INDEX.md#build-and-input-ownership). The direct small-set traversal already exists. Historically 0, 1, 2, 4, and 8 failed in both scenes; 16, 32, and 64 passed.

**Work:** measure adapter preparation, index replacement, and direct pair dispatch to locate the cost. Optimize only a measured cost. Keep adapter work, validation, and handle invalidation inside the same contract; the gate times build **plus** pairs. No topology redesign, persistent tuning, or changes to the brute baseline.

**Exit:** publish comparable before/after batched times, absolute ns/scene, intervals, and G1 verdicts for all 16 rows of the 01 slice. Every row must pass, with matching results and zero warm allocations. Rerun build/failure tests if the build path changes.
