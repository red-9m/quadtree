# 03 · Compact radix validation of build ranks

**Staged, not retained.** The combined compact-geometry + rank candidate restores all 15 G9 passes in both repeats. G2 loses the compact-only `uniform` 10,000 pass and still has five failures and one inconclusive row at 15 rounds. Runtime source is unchanged.

## Candidate

Capture just the full 64-bit ranks, not 24-byte spatial records. Small sets sort the packed values by comparison; at 256 ranks or more, eight histograms drive stable LSD radix passes, skipping constant bytes. Adjacent equal ranks still return `DuplicateRank` after geometry validation and before any payload move, index mutation or output write.

The two rank arrays fit in the existing workspace's temporary spatial-key allocation. A byte array owns that storage; nonallocating placement array new starts the appropriate typed array lifetime for each phase. Spatial planning, shrink and the ordered partner radix sorter restore the spatial-key scratch before writing it. There are no additional allocations, reservation units or retained bytes. Rank insertion, ordered traversal and output order are unchanged.

Apply [rank.patch](rank.patch) to the [staged radix control](../g9-radix-row-sort-2026-11-20/README.md) for the isolated pilot. Apply [candidate.patch](candidate.patch) to the [compact classifier candidate](../g2-compact-classifier-2026-11-20/README.md) for the frozen slice. Neither patch is for the shipped source.

## Isolated radix-control pilot

Nine rounds, 100,000 `int32` entries. Ordered build medians fell from 15.193 → 12.537 ms (`uniform`), 15.724 → 12.955 ms (`split-line`), 15.810 → 13.205 ms (`split-line-translated`) and 15.229 → 14.054 ms (`mixed-size`). None of the matched ordered full-step intervals excluded no change: pairs medians were higher on all four rows. This pilot isolates the change in source, not the timed phases, and is not a gate run. All 16 engine rows match the staged reference's fixtures, results, work, allocations and footprint. [Pilot steps and phases](pilot/step-change.csv) · [Verification](pilot/verification.log).

## Frozen snapshot slice with compact geometry

Counts are pass / fail / inconclusive. G0, G3, G4 and G6 pass throughout; all 68 library rows per run have zero warm allocations. This compares compact geometry alone with compact geometry plus rank validation.

| Rounds | G2 control → candidate | G9 control → candidate | G1 control → candidate |
|---:|---:|---:|---:|
| 9 | 10 / 2 / 3 → 9 / 4 / 2 | 14 / 0 / 1 → 15 / 0 / 0 | 8 / 6 / 2 → 8 / 7 / 1 |
| 15 | 10 / 3 / 2 → 9 / 5 / 1 | 14 / 0 / 1 → 15 / 0 / 0 | 8 / 6 / 2 → 8 / 6 / 2 |

15-round `index-ordered`, 100,000 entries, milliseconds. Full-step estimates and phase medians are distinct.

| Scene | Step before → after | Build before → after | Pairs before → after | G9 before → after (limit 4×) |
|---|---:|---:|---:|---|
| `uniform` | 50.187 → 47.927 | 15.980 → 13.415 | 33.888 → 34.078 | 3.469 [3.359–3.582] → 3.245 [3.147–3.347] |
| `split-line` | 56.741 → 54.718 | 16.127 → 13.923 | 40.567 → 40.676 | 3.838 [3.726–3.953] → 3.587 [3.479–3.699] |
| `split-line-translated` | 60.575 → 54.775 | 16.515 → 13.791 | 41.380 → 40.772 | 4.115 [3.796–4.462] → 3.609 [3.483–3.739] |
| `mixed-size` | 155.537 → 155.691 | 16.409 → 14.279 | 138.820 → 137.415 | 3.763 [3.651–3.879] → 3.727 [3.503–3.965] |

- `split-line-translated` passes G9 in both candidate runs (upper bounds 3.814× and 3.739×); its ordered full step improves 5.2% and 9.6%, with both intervals excluding no change. The 15-round compact control has a wide ordered interval, so do not assign the entire gain to rank validation. `mixed-size` ordered full-step intervals include no change in both repeats.
- `uniform` at 10,000 goes from pass to inconclusive in both G2 repeats. Its 15-round unordered step moves 1.129 → 1.149 ms, with unchanged build median (0.491 ms), higher pair median (0.632 → 0.658 ms), and a full-step interval spanning no change. Unordered builds in the slice have no ranks. The G2 movement is in the observed timing, not a change to unordered candidate work; compiled layout or system variation is not isolated here.
- [G2 per-row phases and work](g2-comparison.csv) · [G9 per-row phases and ratios](g9-comparison.csv) · [Matched full steps and intervals](step-change.csv) · [Gate counts](gate-counts.csv) · [9-round control](9-control/gates.txt) / [candidate](9-candidate/gates.txt) · [15-round control](15-control/gates.txt) / [candidate](15-candidate/gates.txt).

## Correctness and reproduction

- All four builds pass the [109-case rank probe](rank-correctness.log): full-width and sparse ranks, 255/256/257 boundaries, duplicate and geometry status precedence, transactional output/handle rollback, consume-without-moving on failure, and scratch reuse. The combined build also passes the probe under ASan/UBSan.
- [CTest 21/21](tests.log), [full differential fuzzing](fuzz.log), [full/moved-set equivalence](equivalence.log), and [exact ordered dense-row budget prefixes](dense-prefix.log) pass. [Full-slice verification](verification.log) matches all uncapped work, checksums, allocations and footprint.

Reconstruct `WORK/radix` as the [radix candidate](../g9-radix-row-sort-2026-11-20/README.md#correctness-and-reproduction). Copy it to `WORK/rank` and apply `rank.patch`. Into another copy, apply the [compact classifier patch](../g2-compact-classifier-2026-11-20/candidate.patch) to produce `WORK/control`; copy that to `WORK/candidate` and apply `candidate.patch`. Verify the [radix](radix-inputs.sha256), [rank-only](rank-inputs.sha256), [compact](control-inputs.sha256) and [combined](candidate-inputs.sha256) source hashes. Build the four `*-build` directories with GCC 16.2.1 Release and pinned Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. Verify [`.text` hashes](text.sha256).

Run `bash verify-ranks.sh WORK/radix WORK/rank WORK/control WORK/candidate` and CTest in `WORK/candidate-build`. Run `bash pilot.sh WORK NEW_PILOT_DIR` and `python3 summarize-pilot.py NEW_PILOT_DIR`. For the full snapshot slice use the [frozen runner](../g2-cross-screen-2026-11-20/run.sh) with `WORK NEW_OUTPUT_DIR`, then the [G2/G9 summarizer](../g2-cross-screen-2026-11-20/summarize.py) on that directory. The full-slice runner pins CPU 6 and records the [environment](environment.txt), [run order](run-order.csv) and [restored profile](restored-mode.txt). Raw samples are in each run directory.

## Handoff

Keep compact geometry + rank validation staged as the combined control, not shipped. Target the remaining G2 gap with a classifier/scatter change rather than another rank-validation pass: test precomputed center/span input and narrower stable permutation/tag streams as separate factors. Preserve the prefix fallback, stable dense mapping and prepared allocation contract. The 15-round combined G2 upper bounds on `uniform` and `split-line-translated` at 100,000 are 2.363× and 2.384×, requiring about 15–16% lower unordered full steps at unchanged uncertainty.

Retest G9 in the same run. At a 3.739× upper bound the translated row tolerates about a 6.5% unordered drop if ordered time stays fixed; clearing its G2 gap would need about another 10% ordered gain under the same uncertainty. `mixed-size` has less than 1% headroom in the 15-round repeat, so measure its ordered pair phase before trying a common build change there. G1/G2 still block release acceptance.
