# 03 · Two-level stable tags

**Not retained.** Neither engine showed a resolved full-step gain on any measured row. `uniform` unordered builds slowed in both blocks, and ordered full steps regressed in both. Runtime unchanged; keep inline counts + bounded lookahead staged.

## Candidate

The [candidate patch](candidate.patch) changes only `int32` top-down bulk planning and its scratch view on the staged inline-count + lookahead control. On even depths, compute the current tag and the next child tag from the same box spans and centers. Use the exact `Split::child` cell and the same integer fit and center tests at both levels. Pack both tags into one byte; scatter by the current tag, writing the predicted tag at the child's output index in a second stream. On odd depths, histogram and scatter read that stream, avoiding the box gather and center/span work in the ordinary classifier. Keep the bounded lookahead on even depths.

The odd pass still checks the original stop conditions. Single-quadrant prefix checks still use exact box geometry; capacity and sorted fallbacks are unchanged. This eliminates neither stable scatter nor the need to classify for children that subsequently terminate. Two ID arrays and two tag arrays occupy 10 bytes per input in the existing temporary key reservation (previously 9); no new storage, reservation units or allocations. Dense slots, topology, rank work and both traversals are unchanged. The [traffic census](../g2-staged-traffic-2026-11-20/README.md#stable-routing-census) modeled 45–48% fewer indexed box gathers, not a speedup. The benchmark's `planBulk` body grows from 3,017 to 5,012 bytes ([symbol sizes](symbol-sizes.csv)); the screen does not isolate code size, routing arithmetic or extra tag stores.

## Matched phase screen

Two reversed nine-round blocks, `int32` at 100,000, both library engines. Unchanged fixtures and benchmark adapter. The table reports milliseconds for build and pairs, and percentage change in estimated full step. Each pair of values gives block 1 / block 2.

| Scene / engine | Build, block 1 / 2 | Pairs, block 1 / 2 | Full-step change, block 1 / 2 |
|---|---:|---:|---:|
| `uniform` / `index` | 6.522 → 7.089 / 6.414 → 7.519 | 7.453 → 7.556 / 7.537 → 7.537 | +3.5% / +4.5% |
| `split-line-translated` / `index` | 7.576 → 7.853 / 7.065 → 7.284 | 7.182 → 7.136 / 7.110 → 7.191 | +0.1% / +2.8% |
| `uniform` / `index-ordered` | 12.782 → 13.492 / 13.169 → 13.954 | 34.206 → 42.580 / 35.200 → 36.308 | +15.4% / +5.8% |
| `split-line-translated` / `index-ordered` | 13.871 → 13.673 / 13.691 → 13.856 | 46.380 → 45.288 / 40.894 → 44.074 | −1.7% / +3.3% |

- Unordered `uniform` build medians rise 8.7% and 17.2%; translated builds rise 3.7% and 3.1%. Neither full-step interval excludes no change for those rows.
- Ordered `uniform` full-step ratios are 1.154 [1.059–1.257] and 1.058 [1.013–1.105]. Its pair medians also rise despite unchanged traversal source; this experiment does not identify the cause of the pair-phase change.
- All other intervals span no change, including both engines on `split-line` and `mixed-size`. [All phases and intervals](pilot/step-change.csv) · [32 rows match fixture hashes, checksums, work, allocations and footprint](pilot/verification.log). Zero warm allocations.
- No frozen-slice rerun. The [last matched 15-round control](../g2-contiguous-pairs-2026-11-20/15-control/gates.txt) remains G2 9 pass / 1 fail / 5 inconclusive; G9 15/15.

## Correctness and reproduction

Exact [dense slots, topology and callback prefixes](checks/dense-order.log) match with exceptions disabled, including signed extremes, one-axis cells, coincident entries, depth limits and forced sorted fallback. ASan/UBSan reproduces the same stream. Both [unordered](checks/unordered-prefix.log) and [ordered](checks/ordered-prefix.log) budget/predicate prefixes and [full/moved-set pair identities](checks/equivalence.log) match. [CTest 21/21](checks/tests.log) and [full differential fuzzing](checks/fuzz.log) pass.

Reconstruct `WORK/control`, `WORK/control-build` and `WORK/box2d` per the [staged control instructions](../g2-child-bounds-only-2026-11-20/README.md#checks-and-reproduction). Copy the source (dereference symlinks) to `WORK/candidate` and apply `patch -p1 < candidate.patch`. Verify the [control](../g2-child-bounds-only-2026-11-20/control-inputs.sha256) and [candidate](candidate-inputs.sha256) sources. Build `WORK/candidate-build` with the same GCC 16.2.1 Release flags, `QUADTREE_BENCH_NATIVE=OFF`, and pinned Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. Verify [ELF `.text` hashes](text.sha256).

Run the fusion report's [functional checks](../g2-fused-commit-2026-11-20/verify.sh) with `WORK CHECK_OUTPUT_DIR`, then its [pilot runner](../g2-fused-commit-2026-11-20/pilot.sh) with `WORK NEW_PILOT_DIR` and [summarizer](../g2-fused-commit-2026-11-20/summarize-pilot.py) with that pilot directory. The pilot pins CPU 6 and restores platform and CPU power settings. [Environment](pilot/environment.txt) · [Run order](pilot/run-order.csv) · [Raw samples and phase CSVs](pilot/).

## Handoff

Do not stack two-level tags or use their modeled traffic reduction as a performance result. On the staged control, measure the **topology materialization** portion of build: separate default node writes, dense reference writes and own-range summaries, and subtree refit. The [staged loader profile](../g2-staged-traffic-2026-11-20/README.md#staged-release-profile) puts this portion at 16–17% of unordered build cycles (roughly 1.0–1.1 ms at 100,000), below the full 1.38 ms `uniform` gap even if all of it were removable. Screen a topology-only change only if the breakdown shows avoidable work; keep dense entry commit separate rather than retrying [commit fusion](../g2-fused-commit-2026-11-20/README.md). Keep exact node order, summaries, and initial mutation/free-block state. Translated G9 still allows only 3.8% unordered savings at fixed ordered time; measure both engines before advancing any change. G1/G2 still block 08.
