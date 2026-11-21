# 03 · Screen disjoint cross-child pairs

**Not retained.** The combined cross-child screen and exact-overlap change gains no G2 pass and slows unordered `mixed-size` at 100,000 entries in both repeats. All 15 candidate G9 rows pass in both repeats. Runtime source is unchanged.

## Candidate

Apply [candidate.patch](candidate.patch) to the [staged top-down + ordered screen + radix build](../g9-radix-row-sort-2026-11-20/README.md), not to the shipped source. Unordered full self traversal screens each row of sibling or child pairs before recursion. It charges only nonempty pairs, batches charges for disjoint bounds, and flushes before each overlapping pair or at the end of the row. If a batch does not fit, it charges the affordable prefix and stops. Filters, predicates and callbacks still run in the original depth-first order.

The same patch changes exact `int32` eligibility to reject disjoint boxes first, then distinguish strict overlap from contact. Contact still uses the default degeneracy rule or `TouchInclusive`. This helper is shared with ordered and other queries; the child-pair screen itself is unordered only. No new storage, workspace requirement, thresholds, fixture changes or adapter changes.

## Frozen snapshot slice

Counts are pass / fail / inconclusive. G0, G3, G4 and G6 pass in all four runs; all 68 library rows per run completed with zero warm allocations.

| Rounds | G2 control → candidate | G9 control → candidate | G1 control → candidate |
|---:|---:|---:|---:|
| 9 | 9 / 4 / 2 → 9 / 4 / 2 | 13 / 0 / 2 → 15 / 0 / 0 | 8 / 8 / 0 → 7 / 6 / 3 |
| 15 | 9 / 5 / 1 → 9 / 5 / 1 | 15 / 0 / 0 → 15 / 0 / 0 | 8 / 8 / 0 → 7 / 6 / 3 |

15-round `index` rows at 100,000 entries, milliseconds. Full-step estimates are separate from phase medians.

| Scene | Step before → after | Build before → after | Pairs before → after | G2 after (limit 2×) |
|---|---:|---:|---:|---:|
| `uniform` | 14.986 → 15.084 | 7.545 → 7.498 | 7.337 → 7.562 | 2.378 [2.311–2.448], fail |
| `split-line` | 15.450 → 15.398 | 8.099 → 7.870 | 7.347 → 7.489 | 2.197 [2.130–2.266], fail |
| `split-line-translated` | 15.203 → 15.302 | 8.318 → 8.135 | 6.910 → 7.078 | 2.317 [2.246–2.390], fail |
| `mixed-size` | 41.271 → 43.365 | 7.398 → 7.146 | 33.727 → 36.106 | 1.295 [1.258–1.333], pass |

- At 100,000, the nine-round split-line unordered steps improved by about 5%, mostly in the build phase even though the patch does not change build source. Neither gain resolved at 15 rounds. All three 100,000-entry failing G2 rows in the table have step change intervals spanning no change at 15 rounds.
- `mixed-size` unordered slowed 4.5–5.1% at 100,000 in both repeats, with intervals excluding no change. Its ordered step slowed 7.7% in the 15-round run (153.404 → 165.197 ms), and its G9 upper bound rose from 3.829× to 3.922×. Both still pass G9. This combined test does not isolate the screen's cost from the eligibility change.
- [G2 per-row phases and work](g2-comparison.csv) · [G9 per-row phases and ratios](g9-comparison.csv) · [Matched before/after intervals](step-change.csv) · [Gate counts](gate-counts.csv) · [9-round control](9-control/gates.txt) / [candidate](9-candidate/gates.txt) · [15-round control](15-control/gates.txt) / [candidate](15-candidate/gates.txt).

## Correctness and reproduction

- [Unordered prefix probe](budget-prefix.log): exact statuses, counters, predicates and visitor streams at finite budgets and early stops; both boundary modes, instrumentation on/off, snapshot and post-update. Both builds pass 101,250 exact geometry checks including integer limits.
- [Ordered prefixes](ordered-prefix.log) and [full/moved-set equivalence](equivalence.log) match. [Slice verification](verification.log) matches uncapped pair counts, checksums, visits, candidates, allocations and footprint. [CTest passed 21/21](tests.log); [full differential fuzzing](fuzz.log) passed for all four coordinate types.

Reconstruct the [radix candidate](../g9-radix-row-sort-2026-11-20/README.md#correctness-and-reproduction) as `WORK/control`, copy it to `WORK/candidate`, and apply `patch -p1 < candidate.patch` in the copy. Verify [control input hashes](control-inputs.sha256) and [candidate input hashes](candidate-inputs.sha256). Build both in Release with GCC 16.2.1 and pinned Box2D `178495ba35695aefe9c321e569d2277f06db3b90`, as in the [top-down run](../g2-topdown-build-2026-11-20/README.md#reproduction). [Executable `.text` hashes](text.sha256).

Run `bash verify-prefix.sh WORK/control WORK/candidate`, the [ordered prefix probe](../g2-grouped-child-screen-2026-11-20/verify-prefix.sh), the [full/moved-set probe](../g2-topdown-build-2026-11-20/verify.sh), and CTest in `WORK/candidate-build`.

Run `bash run.sh WORK NEW_OUTPUT_DIR` on an idle host with matching [environment](environment.txt) and available CPU 6, then `python3 summarize.py NEW_OUTPUT_DIR`. The runner records the [order](run-order.csv) and [restores](restored-mode.txt) the platform profile; set matching CPU energy preferences before the run. Each run directory contains the full snapshot CSV, compressed raw samples, gate output and exit codes.

## Handoff

Return to the staged radix control; do not stack this patch. Profile preparation on the 100,000-entry `uniform` and `split-line-translated` rows: split classification/scatter in `planBulk`, descriptor gathers and dense commit, then topology materialization. Target the largest measured source of traffic; evaluate a compact permutation/tag representation if partition traffic dominates. The 15-round radix control requires about 16–17% lower unordered full-step time on those two rows at unchanged uncertainty. Build savings alone also shrink the G9 denominator; at a 3.854× upper bound on `split-line-translated`, a roughly 4% denominator drop exhausts headroom if ordered time is unchanged. Size an ordered improvement alongside the next unordered change and measure both gates together. G1 and G2 still block release acceptance.
