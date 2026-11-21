# 08 · Publish release acceptance evidence

**Goal:** decide release readiness on the exact release candidate. Requires 01–07 completed or explicitly shown unnecessary. Requirements: `QT-NFR-01`–`QT-NFR-05`, `QT-NFR-11`, `QT-FR-17`, `QT-FR-24`, `QT-FR-28`, `QT-FR-31`, `QT-FR-34`.

**Read only:** the manifest's [method](../../bench/MANIFEST.md#method), [frozen limits](../../bench/MANIFEST.md#frozen-acceptance-limits) and [process calibration](../../bench/MANIFEST.md#error-calibration-this-hardware); [scope of previous library measurements](../../bench/results/index-development/README.md#environment-and-method). Those measurements used 16 persistent steps, not the final 64. Prototype verdicts and targeted before/after timings are not release evidence.

**Run:** calibrate on the designated hardware. Use `index` and `index-ordered`, the applicable snapshot baselines, and pinned Release Box2D for persistent comparisons. Cover all supported coordinate types, applicable scenes/sizes/motion rates, full and moved-set modes, and 64-step persistent passes. Include the 10² snapshot rows omitted by 01. Record dense/time/candidate caps explicitly; publish G5 pathology results and soft comparisons, not only hard-gate passes.

**Deliver:** a new results directory with exact source provenance, hardware/compiler/flags, arguments/seeds, raw samples, summaries and `--gate-files` output. Report median/p95, allocations, work and memory, including rebuild peaks. Reconcile expected vs observed rows and explain every omission; a successful evaluator exit cannot prove coverage of rows it never saw.

**Exit:** every applicable hard gate passes on supported evidence, no unresolved hard intervals, and the correctness/allocation/memory and portability checks from 04/06 pass. Otherwise publish the blocking row/check and leave release open. No threshold changes or algorithm fixes in this task.

**Release scope (owner decision, 2026-11-21):** performance sign-off covers `int32` only. `int64`, `float` and `double` ship as functionally supported, and their timing gates are published but not release-blocking; [Task 09](09-other-coordinate-types.md) owns them. Their correctness (G0), allocation (G6) and portability checks still block. The [full four-type run](../../bench/results/release-acceptance-2026-11-21/README.md) is the baseline.

**Accepted exception (owner decision, 2026-11-21):** G1 rows of 0–4 entries do not block release if their ratio interval's upper bound is ≤ 3.5× brute. Publish each such row as `FAIL (accepted exception)` with absolute ns/scene and its interval. Above 3.5×, or on any other gate (G0, G6, memory), it blocks as usual. All 8–64-entry G1 rows must pass. Scope and evidence: [02 result](02-g1-small-snapshots-result.md).
