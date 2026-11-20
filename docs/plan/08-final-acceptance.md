# 08 · Publish release acceptance evidence

**Goal:** decide release readiness on the exact release candidate. Requires 01–07 completed or explicitly shown unnecessary. Requirements: `QT-NFR-01`–`QT-NFR-05`, `QT-NFR-11`, `QT-FR-17`, `QT-FR-24`, `QT-FR-28`, `QT-FR-31`, `QT-FR-34`.

**Read only:** the manifest's [method](../../bench/MANIFEST.md#method), [frozen limits](../../bench/MANIFEST.md#frozen-acceptance-limits) and [process calibration](../../bench/MANIFEST.md#error-calibration-this-hardware); [scope of previous library measurements](../../bench/results/index-development/README.md#environment-and-method). Those measurements used 16 persistent steps, not the final 64. Prototype verdicts and targeted before/after timings are not release evidence.

**Run:** calibrate on the designated hardware. Use `index` and `index-ordered`, the applicable snapshot baselines, and pinned Release Box2D for persistent comparisons. Cover all supported coordinate types, applicable scenes/sizes/motion rates, full and moved-set modes, and 64-step persistent passes. Include the 10² snapshot rows omitted by 01. Record dense/time/candidate caps explicitly; publish G5 pathology results and soft comparisons, not only hard-gate passes.

**Deliver:** a new results directory with exact source provenance, hardware/compiler/flags, arguments/seeds, raw samples, summaries and `--gate-files` output. Report median/p95, allocations, work and memory, including rebuild peaks. Reconcile expected vs observed rows and explain every omission; a successful evaluator exit cannot prove coverage of rows it never saw.

**Exit:** every applicable hard gate passes on supported evidence, no unresolved hard intervals, and the correctness/allocation/memory and portability checks from 04/06 pass. Otherwise publish the blocking row/check and leave release open. No threshold changes or algorithm fixes in this task.
