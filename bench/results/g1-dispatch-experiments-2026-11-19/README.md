# 02 · Small snapshots — dispatch and storage experiments

**G1 remains open. No library code changes retained.** The live headers and test/benchmark inputs still match the [previous tested source](../g1-flat-commit-2026-11-18/source-inputs.sha256).

## Gate results

| Run | Rounds | Pass | Fail | Inconclusive |
|---|---:|---:|---:|---:|
| [Unchanged headers](before/gates.txt) | 18 | 7 | 7 | 2 |
| [Direct, strict-overlap first](direct/gates.txt) | 18 | 7 | 6 | 3 |
| [Direct, degenerate-box check first](hoist/gates.txt) | 18 | 7 | 6 | 3 |
| [Specialized singleton storage check](single/gates.txt) | 18 | 7 | 6 | 3 |
| [Unchanged headers, repeat](repeat/gates.txt) | 27 | 7 | 8 | 1 |
| [Direct, strict-overlap first, repeat](direct-repeat/gates.txt) | 27 | 8 | 7 | 1 |

The unchanged repeat failed at 0, 1, 2 and 4 entries in both scenes; `clustered` at 8 remained inconclusive. The [comparison CSV](g1-comparison.csv) records absolute ns/scene and the frozen evaluator's ratio intervals and verdicts for all 16 rows of each run. Cross-checks passed on all runs. Pair counts, checksums, fixture hashes, candidate counts, and node visits matched the unchanged run. Each run had 32 library rows and zero warm library allocations. No representative or persistent runs were made.

## What changed in the experiments

The [exact-pair probe](../current-snapshot-2026-11-18/g1-profile/profile-g1.cpp) ran on the unchanged headers at 1, 2, 4 and 8 entries. The [weighted cycle histogram](profile-by-location.csv.gz) shows samples in singleton storage preparation (including the identity compare-and-exchange), and in candidate dispatch, overlap tests and entry construction at four. Source locations are approximate and the [probe's timings](profile.log) are not gate timings.

- [`direct`](direct.patch) elides per-pair budget checks only if the entire flat traversal fits the candidate budget. It checks strict overlap first and uses closed overlap for degenerate contact. The 27-round comparison reduced four-entry steps from 102.5 → 93.3 ns (uniform) and 94.5 → 86.2 ns (clustered), still well outside G1. Meanwhile, the uniform 32-entry step regressed from about 2.27 to 2.51 µs. Not retained.
- [`hoist`](hoist.patch) makes the same budget change but checks the boundary/degeneracy conditions before comparing overlap. The four-entry benefit and 32-entry penalty persisted. Not retained.
- [`single`](single.patch) gives singleton capacity preparation its own compiler instantiation without changing the identity path. Singleton times showed no repeatable improvement. Not retained.

**Next:** stay on 02. Measure descriptor capture/commit separately from direct pair dispatch at two and four entries. The dispatch experiments leave a large replacement cost and lose on larger passing batches. A new candidate should preserve validation, exhaustion handling, and fresh handle issuance and be compared at all 16 sizes before keeping it.

## Reproduction

Use the [previous source inputs](../g1-flat-commit-2026-11-18/README.md#reproduction) at the recorded [HEAD](source-head.txt). Apply at most one experiment patch to that exact source and rebuild Release between variants. [Environment](environment.txt): Ryzen 9 5900HS, GCC 16.2.1, pinned Box2D v3.2.0. Run `bash reproduce.sh /path/to/quadtree-bench /fresh/outdir 18` (or `27` for the repeats). CSV metadata holds the full arguments and effective flags. Every result directory includes the compressed raw samples and run/evaluator exits. For profiling reproduction use the [task 01 probe recipe](../current-snapshot-2026-11-18/README.md#environment-and-reproduction), with `uniform` and sizes `1 2 4 8`.
