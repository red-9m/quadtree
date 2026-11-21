# 03 · Packed closed bounds at cross-node admission

**Do not advance the packed gate.** Unordered pair medians improve on the blockers, but no unordered full-step gain resolves. Ordered `mixed-size` regresses in both blocks. No frozen rerun or runtime change.

The [traversal profile](../g2-staged-traversal-2026-11-21/README.md) attributes 31–36% of unordered pair cycles to cross-node/loop work. The [census](../g2-cross-admission-census-2026-11-21/README.md) finds disjoint bounds reject 86–88% of charged cross calls, with balanced one-axis rejects. This screens only their overlap test, not an early child screen.

The [patch](candidate.patch) adds a local `crossNodeOverlap`: pack both `int32` lower corners and the opposite upper corners, compare signed lanes, and reject on any strict separation. Equal endpoints and degenerate boxes are still closed-overlapping; no subtraction can overflow. Checked layout and `memcpy` permit unaligned loads without pointer punning. SSE2 is compile-time guarded; other targets/types use the original scalar test. No node layout, recursion, filter, occupancy, charge, callback or exact-item changes. [Release code](bounds-gate.asm) replaces four short-circuit branch sites with two `movdqu`, unpack, `pcmpgtd`, `pmovmskb` and one conditional branch after the unchanged charge. GCC's unordered `crossPairs<int32>` body is 1,287 → 1,286 bytes; machine layout is not held fixed.

## Matched phase screen

Two reversed nine-round blocks, CPU 6, quiet starts within 1 °C inside each comparison. Both engines, `uniform`, both split-line scenes and `mixed-size`, 10,000 and 100,000 `int32` entries. Advance to frozen gates only if the representative unordered full-step result warrants it; component savings aren't a gate pass.

| Unordered fixture | Pair median in ms, block 1 / 2 | Full-step ratio, block 1 / 2 |
|---|---:|---:|
| `split-line` 10,000 | 0.724 → 0.709 / 0.719 → 0.710 | 0.984 [0.954–1.016] / 0.997 [0.967–1.028] |
| `uniform` 100,000 | 7.336 → 7.139 / 7.328 → 7.163 | 0.979 [0.940–1.019] / 0.997 [0.965–1.031] |
| `split-line-translated` 100,000 | 7.016 → 6.924 / 7.026 → 6.874 | 0.995 [0.964–1.027] / 0.976 [0.940–1.013] |

- Ordered `mixed-size` at 100,000 is 4.6% and 3.5% slower (ratios 1.046 [1.014–1.079], 1.035 [1.003–1.069]); pair medians increase in both comparisons. Ordered traversal does not use the candidate gate. Timings do not identify the cause of the ordered regression.
- [All phases and intervals](pilot/step-change.csv) · [Verification](pilot/verification.log) · [Run order](pilot/run-order.csv) · [Raw samples and environment](pilot/). All 64 rows match fixtures, checksums, work, allocation counts and footprint. Power settings restored.

## Checks and reproduction

[Packed and scalar geometry checks](checks/bounds.log) cover 614,656 exhaustive box pairs over signed extremes and 100,000 generated pairs, both orders. GCC checks the SSE2 and forced scalar source paths; Clang ASan/UBSan verifies the packed probe. [Unordered budget/predicate/stop](checks/unordered-sanitized.log), [ordered prefixes](checks/ordered-prefix.log), [source order](checks/source-sanitized.log), [topology/dense](checks/dense-order.log), and [full/moved-set identities](checks/equivalence.log) match; [CTest 21/21](checks/tests.log) and [full differential fuzzing](checks/fuzz.log) pass.

Rebuild the [source-radix candidate](../g9-two-scatter-source-radix-2026-11-21/README.md#correctness-and-reproduction) as `WORK/control` and `WORK/control-build`; copy it to `WORK/candidate` and apply `patch -p1 --fuzz=0 < candidate.patch`. Use GCC 16.2.1 Release, tests/benchmarks enabled, `QUADTREE_BENCH_NATIVE=OFF`, and pinned `WORK/box2d` at `178495ba35695aefe9c321e569d2277f06db3b90`. [Input hashes](candidate-inputs.sha256) · [Exact control ELF](control-executable.sha256) · [`.text` hashes](text.sha256). Run `bash verify-bounds.sh WORK/candidate` and the [shared checks](../g2-radix-outline-2026-11-21/verify.sh); time with `bash run.sh WORK NEW_PILOT_DIR --pilot`, then `python3 summarize-pilot.py NEW_PILOT_DIR`.

## Handoff

**Census exact item eligibility by traversal origin on the unchanged staged control.** Extend the [shadow census](../g2-cross-admission-census-2026-11-21/README.md) for `selfPairs`, direct own-range `crossPairs`, and holder walks. Separate filter rejects, overlap/contact/disjoint outcomes, and degenerate boxes; tie each to the current per-PC exact samples. Count before choosing whether geometry or per-item filtering is worth a new factor. Do not stack the packed gate or reintroduce the earlier grouped child screen on the basis of phase savings. Two-stream + source radix stays staged; G1/G2 still block 08.
