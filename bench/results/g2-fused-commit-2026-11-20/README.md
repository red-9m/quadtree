# 03 · Fuse dense entry commit with own-range materialization

**Neither variant advanced.** All unordered build medians increased; neither variant resolved a full-step gain. Ordered steps slowed on every measured row in both blocks. Keep inline counts + bounded lookahead staged; runtime source is unchanged.

## Candidates

On the [staged inline-count + lookahead control](../g2-inline-count-2026-11-20/README.md), the top-down `int32` loader first commits the dense entries, then rereads dense boxes and filters to materialize own ranges. Both candidates instead commit each own range during the existing parent-first topology walk: construct the payload and dense entry, write its reference and accumulate own bounds/filter ORs from captured inputs. They preserve the dense permutation, node IDs, child order, rank insertion, input-order handle mapping and prefetch distances. Node IDs come from a local cursor; publish topology only after constructing all payloads. Preparation and failure boundaries, flat and sorted fallback behavior, and traversal algorithms are unchanged.

- [First variant](candidate.patch): commit the first entry to seed bounds and filters; subsequent entries return their box/filter to the own-range accumulator.
- [Single loop](single-loop.patch): use an integer union identity and one commit loop/site, accumulating into references instead of returning a box/filter pair. The Release benchmark no longer emits a separate `commitBulkItems` function. This tests whether the first variant's slowdown survives simpler code generation; it does not isolate register pressure or cache effects.

## Matched phase screen

Two reversed blocks of nine rounds per variant, `int32`, 100,000 entries, both library engines. The table gives the range across the two blocks, all four scenes, percent versus each variant's matched control. Full-step estimates and build medians are distinct.

| Variant | Unordered build | Ordered build | Ordered full step |
|---|---:|---:|---:|
| First | +6.1–8.6% | +24.0–43.6% | +9.2–15.9% |
| Single loop | +5.5–17.2% | +17.0–63.6% | +5.4–37.2% |

- First variant: all three unordered G2 blocker step intervals include no change in both blocks. Unordered `mixed-size` is 10.6–10.9% slower with both intervals excluding no change.
- Single loop: no unordered gain in either block. In block 2 `uniform` and `split-line-translated` are 4.2% and 10.7% slower with intervals excluding no change. The large regressions in that block also include slower ordered pair medians; unchanged traversal source does not isolate their cause. Ordered build and step regressions are already present in block 1.
- [First before/after steps, phases and intervals](first-pilot/step-change.csv) · [Single loop](single-loop-pilot/step-change.csv). The individual CSVs and compressed raw samples are in the same directories. All 64 library rows match the staged reference on fixture IDs, checksums, pairs, work, allocations and footprint; zero warm allocations ([first](first-pilot/verification.log) / [single loop](single-loop-pilot/verification.log)).
- Neither variant warranted the frozen slice. No new gate verdicts. The [previous matched 15-round control](../g2-contiguous-pairs-2026-11-20/15-control/gates.txt) remains the current staged evidence: G2 9 pass / 1 fail / 5 inconclusive; G9 15/15.

## Correctness and reproduction

Exact dense slots, topology, callback prefixes (including forced sorted fallback), and ASan/UBSan match for both variants ([first](first-checks/dense-order.log) / [single loop](single-loop-checks/dense-order.log)). Ordered and unordered budget/predicate prefixes and full/moved-set identities match; CTest 21/21 and full differential fuzzing pass on each ([first checks](first-checks/) / [single-loop checks](single-loop-checks/)).

Reconstruct the [inline-count + lookahead control](../g2-inline-count-2026-11-20/README.md#correctness-and-reproduction). For each variant, make a separate `WORK` containing `control`, `candidate`, `control-build`, `candidate-build` and `box2d`; copy the control source with symlinks dereferenced before applying the corresponding patch to `candidate`. Verify [control](control-inputs.sha256), [first](candidate-inputs.sha256), and [single-loop](single-loop-inputs.sha256) input hashes. Use GCC 16.2.1 Release, `QUADTREE_BUILD_BENCHMARKS=ON`, `QUADTREE_BUILD_TESTS=ON`, `QUADTREE_BENCH_NATIVE=OFF`, and pinned Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. [Base revision](working-base.txt) · [Executable `.text` hashes](text.sha256).

Run `bash pilot.sh WORK NEW_PILOT_DIR` and `python3 summarize-pilot.py NEW_PILOT_DIR`. The runner pins CPU 6, records run order and environment, and restores all power settings. Run `bash verify.sh WORK CHECK_OUTPUT_DIR` separately from timing.

## Handoff

Keep the control's two-pass commit. Next, isolate the [cross-child bounds screen](../g2-cross-screen-2026-11-20/README.md#candidate) from the exact-eligibility change it was originally tested with. The [narrow profile](../g2-narrow-profile-2026-11-20/README.md#unordered-build-attribution) assigns 33–35% of unordered pair cycles to `crossPairs` node work. Test only child screening on the staged control; retain the original exact test and preserve charge order and callback prefixes. If both phase and step results warrant it, rerun G2/G9 together: the translated control allows only about 3.8% unordered improvement at unchanged ordered time. G1/G2 still block 08.
