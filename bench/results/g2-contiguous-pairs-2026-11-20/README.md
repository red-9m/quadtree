# 03 · Batched eligibility for contiguous unordered pair ranges

**Neither candidate advanced.** Four-lane grouping slows all three 100,000-entry G2 blockers at 15 rounds. Scalar source hoisting plus closed-first rejection does not recover a gain. Keep inline counts + bounded lookahead staged; runtime unchanged.

## Coverage census

An untimed copy counts exact candidate tests by caller and target range length. On the measured snapshots no target entries use linked lists. For unordered `int32` at 100,000 entries, complete four-entry groups in same-node and cross-node own ranges account for 61.7% of `uniform` candidates, 57.0% of `split-line`, 55.5% of `split-line-translated`, and 18.3% of `mixed-size`. Excludes scalar tails and holder-vs-subtree walks. [Counts](census.csv) · [Reconciliation with the gate rows](census-verification.log).

## Candidate

Apply [candidate.patch](candidate.patch) to the [staged inline-count + lookahead build](../g2-inline-count-2026-11-20/README.md). Only unordered full self traversal changes. It hoists the source box, filter and degeneracy check for linear `int32` own-entry rows with the default predicate. Where four candidates fit the remaining budget, it classifies four targets together. A no-report group charges the compatible candidates and filter rejections in bulk. A group with reports commits charges and callbacks in original order, stopping at the original callback boundary. Pure tests may be computed for later lanes before a visitor stops; those lanes are not charged or reported. Short tails, insufficient budget, linked targets, custom predicates and holder-vs-subtree walks use the original scalar test. No ordered implementation, topology, reservation or benchmark fixture changed.

The [scalar probe](scalar.patch) keeps contiguous traversal, hoisted source metadata, and closed-first rejection, but evaluates and charges one candidate at a time. This isolates the cost of four-lane grouping and avoids speculative exact evaluations after a visitor would stop. It runs only the targeted phase screen, not the full gate.

## Frozen snapshot slice — four-lane candidate

Counts are pass / fail / inconclusive. G0, G3, G4 and G6 pass throughout; all 68 library rows per run have zero warm allocations.

| Rounds | G2 control → candidate | G9 control → candidate | G1 control → candidate |
|---:|---:|---:|---:|
| 9 | 9 / 1 / 5 → 9 / 6 / 0 | 15 / 0 / 0 → 15 / 0 / 0 | 8 / 6 / 2 → 8 / 6 / 2 |
| 15 | 9 / 1 / 5 → 9 / 3 / 3 | 15 / 0 / 0 → 15 / 0 / 0 | 8 / 6 / 2 → 8 / 6 / 2 |

15-round `index`, 100,000 entries, milliseconds. Full-step estimates and phase medians are distinct.

| Scene | Step before → after | Build before → after | Pairs before → after | G2 after (limit 2×) |
|---|---:|---:|---:|---|
| `uniform` | 13.587 → 14.339 | 6.142 → 6.143 | 7.265 → 8.093 | 2.222 [2.148–2.298], fail |
| `split-line` | 14.174 → 14.901 | 6.639 → 6.690 | 7.362 → 8.123 | 2.073 [1.995–2.154], inconclusive |
| `split-line-translated` | 13.789 → 14.451 | 6.688 → 6.702 | 6.941 → 7.670 | 2.201 [2.126–2.279], fail |

- All three unordered steps are 4.8–5.5% slower at 15 rounds with intervals excluding no change; `clustered` is 9.9% slower. The change does not win despite covering a majority of exact candidates on these blockers.
- All G9 rows pass. A slower unordered denominator makes that ratio easier, so G9 does not rescue the candidate. The full slice matches uncapped results, work, allocations and footprint.
- [G2 rows and work](g2-comparison.csv) · [G9 rows and phases](g9-comparison.csv) · [Matched steps and intervals](step-change.csv) · [Gate counts](gate-counts.csv) · [9-round control](9-control/gates.txt) / [candidate](9-candidate/gates.txt) · [15-round control](15-control/gates.txt) / [candidate](15-candidate/gates.txt).

## Scalar phase screen

Two reversed nine-round blocks at 100,000 `int32` entries. None of the unordered blocker step intervals shows a gain. On `uniform`, pair medians increase from 7.391 → 7.781 ms and 7.414 → 7.770 ms; a 5.2% full-step slowdown resolves only in block 1. Both split-line pair medians also increase in both blocks. This rules out advancing the tested scalar variant, not source hoisting in isolation. No gate verdict for scalar. [Steps and phases](pilot/step-change.csv) · [Work verification](pilot/verification.log).

## Verification and reproduction

- The [targeted range prefix](range-prefix.log) matches control, four-lane, and scalar streams for integer extremes, contacts, degenerates, mixed filters, linked fallback after removal, every candidate budget through 137, and early stops with instrumentation on/off. The broader [four-lane](unordered-prefix.log) and [scalar prefix](scalar-prefix.log) probes also match. These compare observable prefixes, not the speculative lane evaluations.
- The four-lane build matches [dense order under ASan/UBSan](dense-order.log) and [full/moved-set pairs](equivalence.log). [CTest 21/21](tests.log) and [full differential fuzzing](fuzz.log) pass.

Reconstruct the [inline-count + lookahead candidate](../g2-inline-count-2026-11-20/README.md#correctness-and-reproduction) as `WORK/control`; copy it to `WORK/candidate` and apply `candidate.patch`. Verify [control](control-inputs.sha256) and [candidate](candidate-inputs.sha256) hashes. Build both `*-build` directories with GCC 16.2.1 Release and pinned Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. [ELF `.text` hashes](text.sha256).

For the scalar screen, apply [scalar.patch](scalar.patch) to a clean control copy at `WORK/scalar` ([hashes](scalar-inputs.sha256)), build `WORK/scalar-build`, then run `bash pilot.sh WORK NEW_PILOT_DIR` and `python3 summarize-pilot.py NEW_PILOT_DIR`.

Run `bash census.sh WORK/control > census.csv` and `python3 verify-census.py` in the result directory. Run `bash verify.sh WORK CHECK_OUTPUT_DIR` separately from timing. Use the [frozen slice runner](../g2-inline-count-2026-11-20/run.sh) with `WORK NEW_OUTPUT_DIR full`, then the [G2/G9 summarizer](../g2-cross-screen-2026-11-20/summarize.py). CPU 6 is pinned; all original [power settings](power-after.txt) are restored. [Environment](environment.txt) · [Run order](run-order.csv).

## Handoff

Return to the staged control. Do not retune lane width or closed-first scalar tests without new evidence. Try fusing top-down dense entry commit with own-range materialization: write references and accumulate bounds/filter summaries while the normalized box and descriptor are already live, instead of rereading dense arrays in a second pass. Preserve dense order, parent/child order, rank insertion and precommit failure behavior. Screen that factor separately; the [narrow build profile](../g2-narrow-profile-2026-11-20/README.md#unordered-build-attribution) puts dense and topology commit together at 29–32% of sampled unordered build cycles, not all of it removable.

The 15-round control still needs about 10% less `uniform` full-step time and 7% less translated time to clear G2 upper bounds. The translated G9 upper bound of 3.848× allows only 3.8% unordered improvement if ordered time stays fixed. Measure both engines' phase changes and rerun G2/G9 together. G1/G2 still block release acceptance.
