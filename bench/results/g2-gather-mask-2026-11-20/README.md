# 03 · Gather overlap and ordered child metadata

**No patch retained.** Four independent gathers did not beat bounded lookahead. Neither occupancy-mask variant improved the translated ordered row. Single-overlap dispatch passed G9 in both repeats but gained no G2 pass. Runtime unchanged.

## Candidates

All factors start from the [inline-count + lookahead build](../g2-inline-count-2026-11-20/README.md), not shipped source.

- [Four gathers](gather.patch) replaces 16-entry lookahead with four independent indexed box loads and classifications before updating the shared histogram. The scalar tail and stable scatter emit the original tag and dense order.
- [Mask + branchless bounds](mask.patch) caches four child occupancy bits in node padding and refreshes them alongside counts and bounds. The ordered child screen tests all four boxes without short-circuiting and masks empty slots afterward. The fuzzer checks cached occupancy after every mutation.
- [Single-overlap dispatch](single.patch) keeps the occupancy cache but restores short-circuit bounds checks. For a full block with exactly one overlapping child at slot `q`, it charges `q + 1` visits before descending and `3 - q` afterward only if nonzero and the traversal is still active. Other groups keep the original grouped charging. This isolates the follow-up from both four gathers and branchless bounds.

## Factor screen

Two reversed nine-round blocks at 100,000 `int32` entries. [All matched steps and phases](pilot/step-change.csv) · [Work verification](pilot/verification.log).

- Four gathers make unordered build medians worse on `uniform` in both blocks (6.155 → 6.476 ms and 6.178 → 6.490 ms) and on both split-line scenes. No target unordered full-step interval shows a gain. `planBulk` grows from 3,017 to 4,561 bytes in the measured ELF ([symbol sizes](symbol-sizes.csv)); the run does not isolate whether code size contributed. Keep bounded lookahead.
- Mask + branchless bounds raises translated ordered pair medians from 37.549 → 39.587 ms and 37.804 → 39.532 ms. Full-step intervals span no change on that scene; ordered `uniform` and `split-line` regress in block 1. Do not keep the branchless screen.

## Frozen snapshot slice — single-overlap dispatch

Counts are pass / fail / inconclusive. G0, G3, G4 and G6 pass throughout; all 68 library rows per run complete with zero warm allocations.

| Rounds | G2 control → single | G9 control → single | G1 control → single |
|---:|---:|---:|---:|
| 9 | 9 / 2 / 4 → 9 / 3 / 3 | 15 / 0 / 0 → 15 / 0 / 0 | 8 / 6 / 2 → 8 / 8 / 0 |
| 15 | 9 / 3 / 3 → 9 / 2 / 4 | 15 / 0 / 0 → 15 / 0 / 0 | 8 / 7 / 1 → 8 / 6 / 2 |

15-round `index-ordered`, 100,000 entries, milliseconds. Full-step estimates and phase medians are separate.

| Scene | Step before → after | Build before → after | Pairs before → after | G9 before → after (limit 4×) |
|---|---:|---:|---:|---|
| `uniform` | 43.810 → 45.027 | 11.956 → 11.981 | 31.632 → 32.741 | 3.265 [3.170–3.362] → 3.271 [3.170–3.377] |
| `split-line` | 50.300 → 50.944 | 12.429 → 12.470 | 37.600 → 38.417 | 3.588 [3.480–3.700] → 3.590 [3.486–3.698] |
| `split-line-translated` | 50.248 → 51.212 | 12.313 → 12.449 | 38.068 → 38.282 | 3.685 [3.574–3.800] → 3.718 [3.605–3.835] |
| `mixed-size` | 144.648 → 145.082 | 12.179 → 11.755 | 132.438 → 132.642 | 3.627 [3.523–3.734] → 3.600 [3.497–3.706] |

- The translated ordered step shows no resolved gain in either repeat. At 15 rounds all four intervals span no change; at nine rounds ordered `uniform` is 3.8% slower (ratio 1.038 [1.003–1.074]). The specialized dispatch did not recover headroom.
- All three 100,000 unordered blocker step intervals include no change in both repeats. `split-line` is inconclusive for control in both repeats, fails in the nine-round candidate, and is inconclusive again at 15 rounds. Both `uniform` and `split-line-translated` fail at 100,000 in the matched control and candidate runs. This repeat variation does not reinstate any G2 pass.
- [G2 per-row phases and work](g2-comparison.csv) · [G9 per-row phases and ratios](g9-comparison.csv) · [Matched steps and intervals](step-change.csv) · [Gate counts](gate-counts.csv) · [9-round control](9-control/gates.txt) / [candidate](9-candidate/gates.txt) · [15-round control](15-control/gates.txt) / [candidate](15-candidate/gates.txt).

## Correctness and reproduction

- [Exact single-child budget probe](single-prefix.log) checks full → partial → full occupancy through removals and insertions, every node limit through 76, and early stops with instrumentation on/off. [Ordered prefixes](ordered-prefix.log), [long rows](dense-prefix.log), and [dense order and fallback under ASan/UBSan](dense-order.log) match.
- [CTest 21/21](tests.log) and [full differential fuzzing](fuzz.log) pass, including cached occupancy checks after mutations. [Full-slice verification](verification.log) matches uncapped results, work, allocations and footprint.

Reconstruct the [combined inline-count candidate](../g2-inline-count-2026-11-20/README.md#correctness-and-reproduction) as `WORK/control`. Copy it to `WORK/gather`, `WORK/mask` and `WORK/single` and apply the respective patch. Verify [control](control-inputs.sha256), [gather](gather-inputs.sha256), [mask](mask-inputs.sha256) and [single](single-inputs.sha256) hashes. Build the `*-build` directories with GCC 16.2.1 Release and pinned Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. [ELF `.text` hashes](text.sha256).

Run `bash run.sh WORK NEW_PILOT_DIR pilot`, then `python3 summarize-pilot.py NEW_PILOT_DIR`. Point `WORK/candidate-build` to `WORK/single-build` for `bash run.sh WORK NEW_OUTPUT_DIR full`; use the [G2/G9 summarizer](../g2-cross-screen-2026-11-20/summarize.py) on that directory. The runner pins CPU 6 and restores [power settings](power-after.txt). [Environment](environment.txt) · [Run order](run-order.csv). Run `bash verify.sh WORK CHECK_OUTPUT_DIR` separately from timings.

## Handoff

Keep inline counts + bounded lookahead as the staged control. Stop tuning these scalar classifier and child-dispatch variants; they showed no full-step gain. Target unordered pair eligibility in contiguous own-entry ranges: measure coverage of those ranges, then test hoisting source metadata and grouping exact geometry tests without changing filter decisions, candidate charges or callback prefixes. Keep the linked and finite-budget paths as controls. The older [overlap-first experiment](../g2-cross-screen-2026-11-20/README.md) combined eligibility with child screening and was rejected; isolate eligibility this time.

The matched 15-round control needs about 9% less `uniform` full-step time at 100,000, roughly 1.2 ms or 17% of its pair phase if build is unchanged. `split-line-translated` needs about 7% less unordered time; its ordered upper bound of 3.800× allows only 5% unordered savings at unchanged ordered time. Rerun G2/G9 together before retaining a pair change. G1/G2 still block release acceptance.
