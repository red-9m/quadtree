# 03 · Two independent stable-scatter streams

**Staged, not shipped.** Two-stream scatter gains two repeatable G2 passes and leaves three inconclusive rows in both runs. G9 passes 15/15 in both runs, but translated headroom is nearly zero. The ordered `uniform` full step regresses at 15 rounds. Runtime unchanged; this is a staged control, not acceptance.

The [patch](candidate.patch) starts from direct child initialization alone. For `int32` bulk partitions of at least 32 IDs, classify each contiguous half into its own histogram using the existing geometry and bounded prefetch. Combine the histograms to get the same split and tag ranges. A tag's second cursor starts after all its lower-half IDs; interleave one placement from each half without changing either half's order. Write a possible odd tail from the upper half. Short partitions use single-cursor scatter. Prefix checks, scratch capacity and fallback, own-range copy, dense commit and topology rules are unchanged. No rank or pair outline patch.

The [Release disassembly](scatter-instructions.asm) confirms independent cursor updates at `rsp+0x160` and `rsp+0x180`, and two destination writes per loop iteration. GCC's emitted `planBulk<int32>` body grows from 3,017 to 4,458 bytes, including the two classification halves. This screen measures the whole change, not the independent effect of code size or the scatter kernel.

## Matched phase screen

Two reversed nine-round blocks at 100,000 `int32` entries, both library engines. Fixtures and adapter unchanged.

| `index` scene | Build, block 1 / 2 (ms) | Full-step change, block 1 / 2 |
|---|---:|---:|
| `uniform` | 6.155 → 5.539 / 5.969 → 5.457 | −2.2% / −2.8% |
| `split-line` | 6.433 → 5.966 / 6.337 → 5.946 | −4.1% / −1.8% |
| `split-line-translated` | 6.500 → 6.060 / 6.386 → 5.937 | −3.7% / −3.1% |
| `mixed-size` | 5.930 → 5.430 / 5.872 → 5.684 | +0.2% / 0.0% |

- Unordered build medians improve in both blocks. The two split-line full-step gains resolve in block 1 only; all ordered full-step intervals include no change. In the translated ordered row, build saves 0.32 and 0.48 ms; the combined point estimates change +0.0% and −1.5%. This is enough to warrant a frozen G2/G9 screen, not a gate verdict.
- [All phases and intervals](pilot/step-change.csv) · [Verification](pilot/verification.log): all 32 rows match fixture hashes, pair checksums, work, allocations and footprint; zero warm allocations.

## Frozen snapshot slice

Counts are pass / fail / inconclusive. G0 cross-checks and G3/G4/G6 pass throughout; each run has 68 warm allocation-free library rows.

| Rounds | G2 control → two streams | G9 control → two streams | G1 control → two streams |
|---:|---:|---:|---:|
| 9 | 9 / 1 / 5 → 12 / 0 / 3 | 15 / 0 / 0 → 15 / 0 / 0 | 8 / 6 / 2 → 8 / 6 / 2 |
| 15 | 10 / 1 / 4 → 12 / 0 / 3 | 15 / 0 / 0 → 15 / 0 / 0 | 8 / 6 / 2 → 8 / 7 / 1 |

15-round `index`, 100,000 entries, control → candidate, ms/snapshot. Full-step estimates and phase medians are separate.

| Scene | Build median | Pair median | Full step | G2 candidate (≤2×) |
|---|---:|---:|---:|---|
| `uniform` | 5.919 → 5.893 | 7.392 → 7.647 | 13.412 → 13.475 | 2.001 [1.931–2.074], inconclusive |
| `split-line` | 6.466 → 5.963 | 7.402 → 7.403 | 13.949 → 13.523 | 1.886 [1.815–1.959], pass |
| `split-line-translated` | 6.517 → 5.997 | 7.016 → 6.994 | 13.598 → 13.028 | 1.948 [1.871–2.027], inconclusive |
| `mixed-size` | 5.828 → 5.440 | 34.307 → 34.168 | 40.249 → 39.845 | 1.159 [1.126–1.194], pass |

- `split-line` at 100,000 and translated at 10,000 move from inconclusive to pass in both repeats. The frozen unordered full-step gains resolve for `split-line` at nine rounds (−5.3%) and translated at 15 (−4.2%). Translated 100,000 moves from fail to inconclusive at 15. `uniform` at 100,000 remains inconclusive despite the pilot build gain; its nine-round build falls 0.43 ms but its 15-round build only 0.03 ms.
- Translated ordered build at 100,000 saves 0.80 ms at nine rounds and 0.52 ms at 15; its full-step intervals still span no change. The candidate G9 upper bounds on that row are 3.981× and 3.995×. At 15 rounds, ordered `uniform` full step increases 4.0%, ratio 1.040 [1.002–1.079]; both build and pairs are higher. The nine-round ordered interval includes no change. Query source did not change; the timing effect is not isolated.
- [G2 phases and work](frozen/g2-comparison.csv) · [G9 phases and ratios](frozen/g9-comparison.csv) · [Matched full steps and intervals](frozen/step-change.csv) · [Gate counts](frozen/gate-counts.csv) · [Verification](frozen/verification.log). Raw samples, gate output and run order are in [frozen/](frozen/).

## Correctness and reproduction

The [scatter probe](checks/scatter-test.log) checks exact stable permutations, bucket edges, and canaries in 36 cases: 31/32/33 and 63/64/65 IDs, all five tags, different half histograms, odd tails and both output parities. The candidate passes it under ASan/UBSan. [Dense slots, topology and exact callback prefixes](checks/dense-order.log) match, including under ASan/UBSan, along with the [unordered](checks/unordered-prefix.log) and [ordered](checks/ordered-prefix.log) budget/predicate/stop streams and [full/moved-set identities](checks/equivalence.log). [CTest 21/21](checks/tests.log) and [full differential fuzzing](checks/fuzz.log) pass.

Use the cursor report's [reconstruction script](../g2-scatter-cursors-2026-11-21/reconstruct.sh) to create the staged source and benchmark. Link that source and build as `WORK/control` and `WORK/control-build`, with the pinned checkout as `WORK/box2d`. Copy the staged source (dereference symlinks) to `WORK/candidate` and apply `patch -p1 < candidate.patch`. Verify [control](../g2-topology-cost-2026-11-20/candidate-inputs.sha256) and [candidate](candidate-inputs.sha256) input hashes. Build GCC 16.2.1 Release with `QUADTREE_BUILD_BENCHMARKS=ON`, `QUADTREE_BUILD_TESTS=ON`, `QUADTREE_BENCH_NATIVE=OFF` and pinned Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. Verify the [`.text` hashes](text.sha256).

Run `bash verify-scatter.sh WORK/control WORK/candidate` and the fusion report's [functional checks](../g2-fused-commit-2026-11-20/verify.sh). Use its [pilot runner](../g2-fused-commit-2026-11-20/pilot.sh) and [summarizer](../g2-fused-commit-2026-11-20/summarize-pilot.py). The pilot pins CPU 6 and restores power settings. [Environment](pilot/environment.txt) · [Run order](pilot/run-order.csv) · [Raw samples](pilot/). For the frozen snapshot slice, run the [full runner](../g2-inline-count-2026-11-20/run.sh) with `WORK NEW_OUTPUT_DIR full`, then the [G2/G9 summarizer](../g2-cross-screen-2026-11-20/summarize.py) on that output directory.

## Handoff

**Screen the existing rank home-bucket lookahead on top of two-stream scatter.** Use the two-stream executable as the matched control; add only the [rank lookahead patch](../g9-home-lookahead-2026-11-20/candidate.patch), not the unordered outline or a new prefetch distance. The [same-layout ablation](../g9-rank-hint-ablation-2026-11-20/README.md) isolated about 0.47 ms of translated ordered build savings from the hint and ~0.68 ms for its full build change versus direct init, but not an ordered full-step gain. Test whether that saving survives on the new control and whether ordered `uniform` recovers. Keep the exact rank hash/tree and epoch tests, both traversal engines, and the joint frozen G2/G9 check before advancing any combination.

At unchanged relative uncertainty, the 15-round candidate needs about **0.48 ms (3.6%)** less unordered `uniform` time and **0.17 ms (1.3%)** less translated time. Translated G9's upper bound of 3.995× allows only **0.125%** less unordered time if ordered time stays fixed; clearing translated G2 would need roughly **0.61 ms (1.2%)** less ordered time too. Do not infer a pass from the point estimate. G1/G2 still block 08.
