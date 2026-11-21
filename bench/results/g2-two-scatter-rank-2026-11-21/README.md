# 03 · Rank home-bucket lookahead on two-stream scatter

**Do not promote rank lookahead.** It cuts translated ordered build time, but neither frozen run clears translated G2 or G9 at 100,000, and ordered `uniform` does not recover. The unchanged two-stream control also lost G9 passes on retest. Keep two-stream scatter staged; no runtime change or release acceptance.

The matched control is the exact staged [two-stream scatter executable](../g2-two-scatter-2026-11-21/README.md). The candidate adds only the existing [rank lookahead patch](../g9-home-lookahead-2026-11-20/candidate.patch): a 32-slot home-bucket ring in eligible ranked `int32` bulk commits. No outlining patch, new prefetch distance, or fixture change. The [Release disassembly](rank-home-instructions.asm) confirms `prefetcht0`, the bounded ring refill and probing from the queued home without remixing it.

## Matched phase screen

Two reversed nine-round blocks at 100,000 `int32` entries, both library engines. Build medians are in ms/snapshot; full-step changes use the benchmark estimates.

| `index-ordered` scene | Build, block 1 / 2 | Full-step change, block 1 / 2 |
|---|---:|---:|
| `uniform` | 11.170 → 10.450 / 11.501 → 10.488 | +1.5% / +1.0% |
| `split-line` | 13.144 → 10.802 / 12.500 → 11.668 | −0.6% / +0.9% |
| `split-line-translated` | 11.785 → 11.060 / 12.374 → 10.662 | +2.8% / −2.6% |
| `mixed-size` | 11.254 → 11.070 / 12.425 → 11.225 | +4.2% / +3.1% |

- Translated build saves 0.72–1.71 ms, but neither full-step interval resolves a gain. Ordered `uniform` pair medians rise 1.39–1.87 ms, more than the build saves; neither full-step interval excludes no change. Ordered `mixed-size` regresses in block 1 (ratio 1.042 [1.007–1.079]); block 2 is inconclusive.
- Unordered `uniform` is 4.2–4.4% faster by the full-step estimates (ratios 0.957 [0.923–0.993] and 0.956 [0.915–0.9999]); pair medians fall 0.41–0.45 ms. The rank ring does not execute on these unranked builds. Despite unchanged query source, the [emitted unordered `selfPairs`](self-pair-code.txt) shrinks from 1,956 to 1,310 bytes and gains two calls to `pairQualifies`. This repeats the code-generation difference from the [direct-init ablation](../g9-rank-hint-ablation-2026-11-20/README.md), not an isolated prefetch or outlining effect.
- [All phases and intervals](pilot/step-change.csv) · [Verification](pilot/verification.log): all 32 rows match fixtures, checksums, work, allocations and footprint; zero warm allocations.

## Frozen snapshot slice

Counts are pass / fail / inconclusive. G0 cross-checks and G3/G4/G6 pass throughout; each run has 68 warm allocation-free library rows.

| Rounds | G2 control → lookahead | G9 control → lookahead | G1 control → lookahead |
|---:|---:|---:|---:|
| 9 | 10 / 0 / 5 → 13 / 0 / 2 | 12 / 0 / 3 → 12 / 0 / 3 | 8 / 6 / 2 → 8 / 7 / 1 |
| 15 | 11 / 0 / 4 → 12 / 0 / 3 | 12 / 0 / 3 → 13 / 0 / 2 | 9 / 7 / 0 → 8 / 6 / 2 |

15-round `index-ordered`, 100,000 entries, control → lookahead, ms/snapshot. The full-step estimate is separate from the phase medians.

| Scene | Build median | Pair median | Full step | G9 candidate (≤4×) |
|---|---:|---:|---:|---|
| `uniform` | 13.313 → 11.972 | 37.831 → 39.061 | 50.988 → 50.929 | 3.672 [3.468–3.887], pass |
| `split-line` | 13.483 → 13.776 | 44.750 → 58.519 | 58.146 → 69.825 | 4.352 [3.669–5.162], inconclusive |
| `split-line-translated` | 13.537 → 11.513 | 44.602 → 39.711 | 58.263 → 53.499 | 4.086 [3.799–4.395], inconclusive |
| `mixed-size` | 13.241 → 10.125 | 153.758 → 140.228 | 168.207 → 151.087 | 3.783 [3.633–3.939], pass |

- **Translated build savings survive:** 0.95 ms at nine rounds and 2.02 ms at 15. Ordered full-step ratios are 0.881 [0.792–0.980] and 0.918 [0.858–0.983], versus no resolved pilot gain. Unordered also speeds up by 9.9% and 7.7% in these frozen comparisons. The G9 upper bounds remain 4.140× and 4.395×. The ordered pair phase also moves; the full-step gains do not isolate the rank hint.
- **Ordered `uniform` does not recover:** full-step intervals span no change in both frozen runs and both pilot blocks. The 15-round build saves 1.34 ms but pairs cost 1.23 ms more. At 100,000, `uniform` and translated each remain G2 inconclusive in both repeats; `uniform` at 10,000 is the only G2 row that moves from inconclusive to pass in both. At 100,000, `split-line` ordered swings from −13.2% at nine rounds to +20.1% at 15; both intervals exclude no change in opposite directions.
- **Control drift:** the previous two-stream runs passed G9 15/15; the same control executable passes 12/15 twice here. The benchmark's `--diff` against the prior runs at matching round counts shows only 171/196 (87.2%) and 147/196 (75.0%) timed rows within combined error, below the [95% calibration target](../../MANIFEST.md#error-calibration-this-hardware). The identical 15-round control's ordered `split-line` and translated steps are 15.0% and 15.4% slower. Retain all samples; do not attribute these shifts to the patch or treat either run as acceptance.
- [G2 phases and work](frozen/g2-comparison.csv) · [G9 phases and ratios](frozen/g9-comparison.csv) · [Matched steps and intervals](frozen/step-change.csv) · [Gate counts](frozen/gate-counts.csv) · [Verification](frozen/verification.log) · Identical-control diffs: [9 rounds](frozen/9-identical-control-diff.txt), [15 rounds](frozen/15-identical-control-diff.txt). Raw samples, per-row gate output, environment and run order are in [frozen/](frozen/).

## Correctness and reproduction

The [nine-fixture exact rank probe](checks/home-test.log) matches hash/tree state, diagnostics, slots, and ordered callback streams under GCC and ASan/UBSan, including warm epoch wrap, in-flight collision conversion, tree-mode rebuild and duplicate rollback. The [109-case rank probe](checks/rank-correctness.log) and [36-case scatter probe](checks/scatter-test.log) pass, including under ASan/UBSan. [Dense slots and topology](checks/dense-order.log), exact [unordered](checks/unordered-prefix.log) and [ordered](checks/ordered-prefix.log) budget/predicate/stop streams, and [full/moved-set identities](checks/equivalence.log) match. [CTest 21/21](checks/tests.log) and [full differential fuzzing](checks/fuzz.log) pass.

Build the [two-stream candidate](../g2-two-scatter-2026-11-21/README.md#correctness-and-reproduction) and link its source and build as `WORK/control` and `WORK/control-build`, with its pinned Box2D checkout as `WORK/box2d`. Dereference-copy the control source to `WORK/candidate` and apply only the [rank patch](../g9-home-lookahead-2026-11-20/candidate.patch) there with `patch -p1 --fuzz=0 --no-backup-if-mismatch`. Verify [control](../g2-two-scatter-2026-11-21/candidate-inputs.sha256) and [candidate](candidate-inputs.sha256) input hashes. Build `WORK/candidate-build` with GCC 16.2.1 Release, `QUADTREE_BUILD_BENCHMARKS=ON`, `QUADTREE_BUILD_TESTS=ON`, `QUADTREE_BENCH_NATIVE=OFF`, and `FETCHCONTENT_SOURCE_DIR_BOX2D=WORK/box2d` at `178495ba35695aefe9c321e569d2277f06db3b90`. Verify the [`.text` hashes](text.sha256).

Run the [home](../g9-home-lookahead-2026-11-20/verify-home.sh), [rank](../g9-build-rank-2026-11-20/verify-ranks.sh), and [scatter](../g2-two-scatter-2026-11-21/verify-scatter.sh) probes with `WORK/control WORK/candidate`; run [functional checks](../g2-fused-commit-2026-11-20/verify.sh) with `WORK NEW_CHECK_DIR`. For the screen, run the [pilot runner](../g2-fused-commit-2026-11-20/pilot.sh) with `WORK NEW_PILOT_DIR` and its [summarizer](../g2-fused-commit-2026-11-20/summarize-pilot.py) on that directory. For the frozen slice, run the [full runner](../g2-inline-count-2026-11-20/run.sh) with `WORK NEW_OUTPUT_DIR full`, then the [G2/G9 summarizer](../g2-cross-screen-2026-11-20/summarize.py) on that directory. Both runners pin CPU 6 and restore power settings. [Pilot environment](pilot/environment.txt) · [Run order](pilot/run-order.csv) · [Raw pilot runs](pilot/).

## Handoff

**Re-establish repeatability on the staged two-stream control before another code change.** On an idle host, repeat the identical frozen 15-round control invocation in independent processes, with the recorded power settings and CPU affinity. Use `quadtree-bench --diff=OLD,NEW` to check the calibration target (≥95% of comparable rows within error), then inspect G2/G9 together on the new runs. If coverage is still below target, report timing instability before using the results to rank candidates. Do not change thresholds, stack the outline patch, or retune rank distance. G1/G2 still block 08; this retest also leaves G9 unresolved.
