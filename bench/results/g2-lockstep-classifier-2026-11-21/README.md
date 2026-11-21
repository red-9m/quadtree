# 03 · Lockstep classification on two-stream + source radix

**Reject the lockstep classifier.** Unordered build medians rise on all three blockers in both blocks; no full-step interval resolves a gain. No frozen rerun or runtime change.

The [classifier profile](../g2-bulk-classifier-2026-11-21/README.md) locates roughly 30–33% of unordered build cycles in the classifier, especially around geometry and loop control. The [candidate](candidate.patch) takes the exact staged source-radix control and classifies one ID per half per iteration, using the original separate histograms. The helper is explicitly inlined where supported: an untimed compiler check of the initial lambda form emitted two calls per iteration, which would not test in-loop overlap. Short groups stay scalar; an odd trailing ID uses the upper histogram. No change to geometry, prefetch distance and bounds, tag/bucket ranges, prefix checks, stable scatter, or workspace.

## Matched phase screen

Two reversed nine-round blocks of `int32` snapshots on CPU 6, both engines, `uniform`, `split-line`, translated, and `mixed-size` at 10,000 and 100,000. Starts are matched within 1 °C per pair, with quiet preflight. Use the full-step estimate and interval, not the sum of phase medians. Advance to the frozen slice only if both 100,000-entry unordered blockers move faster in both blocks, at least one target full-step gain resolves, and there is no repeatable ordered regression.

| Unordered fixture | Build median, block 1 (ms) | Build median, block 2 (ms) | Full-step ratios, block 1 / 2 |
|---|---:|---:|---:|
| `uniform` 100,000 | 5.496 → 5.614 | 5.456 → 5.638 | 1.000 [0.948–1.055] / 1.023 [0.978–1.071] |
| `split-line` 10,000 | 0.436 → 0.449 | 0.431 → 0.450 | 0.968 [0.882–1.062] / 1.059 [0.965–1.163] |
| `split-line-translated` 100,000 | 6.037 → 6.189 | 6.004 → 6.159 | 1.000 [0.963–1.038] / 1.006 [0.960–1.055] |

- No full-step interval excludes 1 in either engine. The [emitted paired loop](paired-classifier.asm) has both histogram updates under one loop exit, no helper calls, and both bounded prefetches. The whole `planBulk<int32>` body grows from 4,458 to 4,630 bytes. The measurement tests this implementation, not the isolated effect of loop trip count, code size, or gather overlap.
- [All phase and step rows](pilot/step-change.csv) · [Verification](pilot/verification.log) · [Run order](pilot/run-order.csv) · [Raw samples and environment](pilot/). All 64 rows match frozen work, checksums and footprint; zero warm allocations. Power settings restored.

## Verification and reproduction

[Exact permutation and bucket edges](checks/scatter-test.log), [dense slots and topology](checks/dense-order.log), [ordered](checks/ordered-prefix.log) and [unordered](checks/unordered-sanitized.log) prefix streams, [source radix](checks/source-sanitized.log), and [full/moved-set identities](checks/equivalence.log) match. The scatter, dense and relevant query probes pass under ASan/UBSan. [CTest](checks/tests.log) passes 21/21; [full fuzzing](checks/fuzz.log) passes.

Build the [source-radix control](../g9-two-scatter-source-radix-2026-11-21/README.md#correctness-and-reproduction) as `WORK/control` and `WORK/control-build`, with `WORK/box2d` pinned. Copy `WORK/control` (dereference symlinks) and apply [candidate.patch](candidate.patch) to `WORK/candidate`. Use GCC 16.2.1 Release, `QUADTREE_BUILD_BENCHMARKS=ON`, `QUADTREE_BUILD_TESTS=ON`, `QUADTREE_BENCH_NATIVE=OFF`, and `FETCHCONTENT_SOURCE_DIR_BOX2D=WORK/box2d` at `178495ba35695aefe9c321e569d2277f06db3b90`. The runner verifies [control ELF](control-executable.sha256), [candidate inputs](candidate-inputs.sha256) and [`.text`](text.sha256).

Use the [scatter probe](../g2-two-scatter-2026-11-21/verify-scatter.sh) and the [outline report's checks](../g2-radix-outline-2026-11-21/verify.sh) before timing. Run `bash run.sh WORK NEW_PILOT_DIR --pilot` and `python3 summarize-pilot.py NEW_PILOT_DIR`.

## Handoff

**Attribute dense and topology commit on the unchanged control.** Reuse the classifier profile's exact Release captures and symbol twin; split dense construction, commit-loop control/gathers, rank insertion, own-range initialization, child construction, and refit. Use audited instructions where compiler inlining blurs source lines. Do not rerun frozen gates for the rejected lockstep patch or infer that stack histogram latency accounts for the classifier cost.
