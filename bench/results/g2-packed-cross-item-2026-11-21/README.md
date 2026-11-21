# 03 · Packed exact overlap for direct cross items

**Frozen screen: `uniform` at 100,000 and `split-line` at 10,000 clear G2 twice.** The 100,000-entry translated row clears both G2 and G9 at nine rounds but is inconclusive on both at 15. The later [45-round confirmation](../g2-packed-cross-item-confirm-2026-11-21/README.md) closes both and stages this candidate for the measured slice.

The [item census](../g2-item-eligibility-census-2026-11-21/README.md) finds direct-cross exact calls reject on disjoint bounds 92–96% of the time, and the [Release audit](../g2-exact-eligibility-profile-2026-11-21/README.md) puts 5.0–8.7% of unordered pair cycles in its box setup and geometry-mode dispatch. The [candidate](candidate.patch) starts from the unchanged two-stream + source-radix control and changes only the default-predicate direct own-range `crossPairs` item test.

For `int32` with SSE2, load the boxes safely with `memcpy`, pack minima and the opposite maxima, and reject on any signed greater-than lane. A closed hit with no opposite endpoint equality is accepted. Only a contact needs the boundary mode or degenerate rule; interior points/lines remain accepted. The [Release code](item-gate.asm) skips both on separation; GCC reads the mode before branching on equality for closed hits and reaches degenerate checks only on contact. It inlines the packed gate: unordered `crossPairs<int32>` grows from 1,287 to 1,787 bytes, so the screen measures the whole code-generation change. `pairQualifies` retains the filter and candidate charge, and custom predicates and other callers/types keep the scalar test. No changes to the node gate, data layout, workspace, visits or callbacks.

## Matched phase screen

Two reversed nine-round blocks, CPU 6, quiet starts within 1 °C per comparison. Both engines on `uniform`, both split-line scenes and `mixed-size` at 10,000 and 100,000. Advance to joint frozen gates only if representative full-step evidence warrants it.

| Unordered fixture | Pair median in ms, block 1 / 2 | Full-step ratio, block 1 / 2 |
|---|---:|---:|
| `split-line` 10,000 | 0.715 → 0.677 / 0.723 → 0.689 | 0.9710 [0.9420–1.0009] / 0.9722 [0.9436–1.0016] |
| `uniform` 100,000 | 7.347 → 6.728 / 7.312 → 6.710 | 0.9450 [0.9107–0.9807] / 0.9466 [0.9155–0.9788] |
| `split-line-translated` 100,000 | 6.997 → 6.654 / 7.024 → 6.647 | 0.9705 [0.9423–0.9996] / 0.9723 [0.9378–1.0080] |

- Build medians on the three targets change by at most 0.032 ms. The unordered translated gain resolves in block 1 only; both `split-line` 10,000 intervals narrowly include 1. The 100,000-entry untranslated `split-line` also gains in block 2 only. No representative ordered regression resolves. Ordered `uniform` gains in block 1 only although it does not call the packed helper; that gain is not attributed to executing the new gate. Ordered `mixed-size` ratios are 0.9926 [0.9629–1.0231] and 0.9928 [0.9641–1.0224].
- [Phases and intervals](pilot/step-change.csv) · [Verification](pilot/verification.log) · [Run order](pilot/run-order.csv) · [Raw samples and environment](pilot/). All 64 rows match frozen checksums, work, allocations and footprint; zero warm allocations. Power settings restored.

## Frozen G1/G2/G9 screen

The same pilot ELFs run the unchanged frozen snapshot slice, quiet cold preflight, CPU 6, nine rounds control → candidate, then 15 candidate → control. Starts are within 1 °C per comparison. Counts are pass / fail / inconclusive. G0 cross-checks, G3/G4/G6 pass throughout, with 68 allocation-free warm library rows per run.

| Rounds | G1 control → packed | G2 control → packed | G9 control → packed |
|---:|---:|---:|---:|
| 9 | 8 / 6 / 2 → 8 / 6 / 2 | 12 / 0 / 3 → 15 / 0 / 0 | 15 / 0 / 0 → 15 / 0 / 0 |
| 15 | 8 / 7 / 1 → 8 / 6 / 2 | 12 / 0 / 3 → 14 / 0 / 1 | 15 / 0 / 0 → 14 / 0 / 1 |

15-round `index` phase medians and full-step estimates, in ms. They are separate statistics.

| Fixture | Build median | Pair median | Full step | Packed G2 |
|---|---:|---:|---:|---:|
| `split-line` 10,000 | 0.448 → 0.437 | 0.733 → 0.680 | 1.182 → 1.123 | 1.891 [1.837–1.948], pass |
| `uniform` 100,000 | 5.359 → 5.357 | 7.312 → 6.705 | 12.709 → 12.087 | 1.887 [1.833–1.943], pass |
| `split-line-translated` 100,000 | 5.946 → 5.990 | 6.937 → 6.637 | 12.985 → 12.813 | 1.900 [1.805–2.001], inconclusive |

- Unordered full-step gains resolve in both pairs for `uniform` (4.2–4.9%) and `split-line` at 10,000 (3.8–4.9%). Translated saves 0.30–0.34 ms in pair medians; the full-step gain resolves only at nine rounds. No hard G2 failures. Candidate G1 retains six failures.
- Translated ordered full step is 47.331 → 47.052 ms and 47.330 → 49.303 ms, neither resolved. Its G9 upper bounds are 3.883× and 4.030×; the latter includes an elevated round error. `mixed-size` G9 passes twice. An inconclusive G2 or G9 interval is not a pass.
- [G2 phases and work](frozen/g2-comparison.csv) · [G9 phases and ratios](frozen/g9-comparison.csv) · [Matched steps and intervals](frozen/step-change.csv) · [Gate counts](frozen/gate-counts.csv) · [Verification](frozen/verification.log). Matched work, checksums, allocations and footprint agree. Raw samples, [run order](frozen/run-order.csv) and environment are in [frozen/](frozen/). Exact reproduction is `bash run.sh WORK NEW_OUTPUT_DIR` and the [joint summarizer](../g2-cross-screen-2026-11-20/summarize.py).

## Checks and reproduction

[Exact geometry](checks/bounds.log) matches for 614,656 exhaustive signed-extreme box pairs and 100,000 generated pairs, both boundary modes, both orientations, packed and forced scalar, including ASan/UBSan. [Exact unordered budget/predicate/stop](checks/unordered-sanitized.log), [ordered prefixes](checks/ordered-prefix.log), [dense order](checks/dense-order.log), [source radix](checks/source-sanitized.log) and [full/moved-set streams](checks/equivalence.log) match. [CTest](checks/tests.log) 21/21; [full differential fuzz](checks/fuzz.log) passes.

Rebuild [two-stream + source radix](../g9-two-scatter-source-radix-2026-11-21/README.md#correctness-and-reproduction) as `WORK/control` and `WORK/control-build`. Copy the source (dereference symlinks) and apply `patch -p1 --fuzz=0 < candidate.patch` to `WORK/candidate`. GCC 16.2.1 Release, benchmarks/tests on, `QUADTREE_BENCH_NATIVE=OFF`, and pinned `WORK/box2d` at `178495ba35695aefe9c321e569d2277f06db3b90`. Verify [inputs](candidate-inputs.sha256), [control ELF](control-executable.sha256), and [`.text` hashes](text.sha256). Run `bash verify-bounds.sh WORK/candidate` and the [shared checks](../g2-radix-outline-2026-11-21/verify.sh), then `bash run.sh WORK NEW_PILOT_DIR --pilot` and `python3 summarize-pilot.py NEW_PILOT_DIR`.

## Handoff

**Audit the translated uncertainty before confirmation.** Reconstruct the reported 15-round intervals from the raw samples for `index`, `grid` and `index-ordered` at 100,000. Separate round dispersion from the calibrated process term and check whether more rounds can plausibly resolve both gates without code changes. Do not drop rounds, change limits or promote based on points alone. G1/G2 still block 08.
