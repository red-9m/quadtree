# 04 · Close runtime verification gaps

**Goal:** turn [§10 verification](archive/QUADTREE-UPDATE-PLAN.md#10-verification-and-acceptance) into a coverage record: requirement/scenario → named test, fixture or missing check. Independent of timing work. No new feature, legacy patch, CI setup, or full performance run.

**Start with existing checks, not a new oracle:** `numeric`, `randomized`, `budgets`, `crossAndSelections`, `cacheAndChurn`, `hardCapacity`, `failureInjection`, and `concurrency` in [index-test.cpp](../../tests/index-test.cpp); [entry-store tests](../../tests/entry-store-test.cpp); `checkTree` and `checkResults` in [index-fuzz.cpp](../../bench/index-fuzz.cpp). [CTest registrations](../../tests/CMakeLists.txt) include mixed-TU, death and no-exception variants; [benchmark registrations](../../bench/CMakeLists.txt) include fuzz and mutation regressions.

- Numeric/identity: compare identities, multiplicity and rank sequences, not counts; retain both independent `QT-FIX-10` probes (`QT-FR-02`, `QT-FR-04`, `QT-FR-07`).
- Traversal: check exact stop/budget prefixes, moved-cache replacement, cross/selected sets, and measured candidate/node work for homogeneous vs interleaved masks (`QT-FR-24`, `QT-FR-26`, `QT-FR-28`, `QT-FR-31`, `QT-FR-32`, `QT-FR-34`).
- Resources: map injected failures to allocation paths, pre-commit state/handles/output, and warmed capacity/peak measurements. Both build modes currently give the strong guarantee; do not manufacture a destructive failure (`QT-FR-13`, `QT-NFR-02`–`QT-NFR-05`).

**Exit:** publish the coverage record with reproducible test names/commands and seeds for regressions. Add tests or measurements only for uncovered obligations. Broken runtime behavior gets a focused regression and fix; platform-only and final timing evidence is assigned explicitly to 06/08, not marked passed.
