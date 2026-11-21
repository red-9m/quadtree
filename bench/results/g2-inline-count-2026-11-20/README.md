# 03 · Inline ordered visit counts and bounded classifier lookahead

**Combined candidate staged; runtime unchanged.** G9 passes 15/15 in both repeats. G2 gains no pass but `split-line-translated` at 100,000 moves from fail to inconclusive in both repeats. At 15 rounds, all three unordered blocker step intervals span no change despite lower build medians.

## Candidates

Both factors start from the [staged narrow-stream build](../g2-bulk-streams-2026-11-20/README.md). Apply [inline.patch](inline.patch) and [lookahead.patch](lookahead.patch) to separate copies for the factor screen, then both to a third copy for the frozen slice.

- **Inline count:** replace both four-bit `std::popcount` calls in `walkOrdered` with a shift and mask into a 16-nibble immediate. Admitted children, visit grouping, saturation and traversal order stay unchanged. The [gate ELF disassembly](code.log) confirms two external `__popcountdi2` calls per control instantiation and none in the inline build, with unchanged target flags.
- **Lookahead:** prefetch the indexed `Box<int32>` 16 positions ahead in `planBulk`, only within the current partition. GCC and Clang use the hint; other compilers keep the original loop. No change to classification, stable order, prefix fallback, allocations or scratch requirements.

## Factor screen

Two reversed blocks of nine rounds at 100,000 `int32` entries. [Matched steps and phases](pilot/step-change.csv) · [Work verification](pilot/verification.log).

- Inline count improves `uniform` ordered steps by 6.7% and 3.6% with both intervals excluding no change. The translated ordered step improves 7.6% in block 2; block 1 does not resolve a gain. `mixed-size` ordered pair medians fall 2.8–3.5%, but its matched full-step intervals include no change.
- Lookahead lowers unordered build medians on `uniform` and the two split-line scenes by 3.7–5.3% in both blocks, or roughly 0.26–0.35 ms. No unordered full-step interval excludes no change. The combined full slice checks whether the phase gains carry over without consuming ordered headroom.

## Frozen snapshot slice — both factors

Counts are pass / fail / inconclusive. G0, G3, G4 and G6 pass throughout; all 68 library rows per run have zero warm allocations.

| Rounds | G2 control → combined | G9 control → combined | G1 control → combined |
|---:|---:|---:|---:|
| 9 | 9 / 2 / 4 → 9 / 1 / 5 | 15 / 0 / 0 → 15 / 0 / 0 | 8 / 6 / 2 → 8 / 8 / 0 |
| 15 | 9 / 2 / 4 → 9 / 1 / 5 | 15 / 0 / 0 → 15 / 0 / 0 | 8 / 6 / 2 → 8 / 8 / 0 |

15-round unordered `index`, 100,000 entries, milliseconds. Full-step estimates and phase medians are separate.

| Scene | Step before → after | Build before → after | Pairs before → after | G2 after (limit 2×) |
|---|---:|---:|---:|---|
| `uniform` | 13.853 → 13.631 | 6.500 → 6.133 | 7.308 → 7.331 | 2.127 [2.060–2.196], fail |
| `split-line` | 14.370 → 14.061 | 6.927 → 6.683 | 7.369 → 7.349 | 1.990 [1.922–2.060], inconclusive |
| `split-line-translated` | 14.026 → 14.084 | 7.036 → 6.760 | 6.944 → 7.090 | 2.002 [1.923–2.085], inconclusive |

- `uniform` and `split-line` ordered full-step gains resolve in both repeats (6.2–6.9% and 6.7–8.9%). At 15 rounds their pair medians fall from 34.401 → 31.750 ms and 40.946 → 37.868 ms. The translated ordered gain resolves only at nine rounds; its G9 upper bounds are 3.813× and 3.923×. `mixed-size` G9 upper bounds are 3.712× and 3.713×.
- The translated unordered G2 verdict improves, but its full-step gain resolves only at nine rounds. Its 15-round ratio also reflects a faster `grid` baseline in the control than the candidate; do not infer a resolved 15-round speedup from the gate ratio. Both G1 empty rows go from inconclusive to fail in both repeats, though neither factor runs on empty snapshots.
- [G2 rows and work](g2-comparison.csv) · [G9 rows and phases](g9-comparison.csv) · [Matched steps and intervals](step-change.csv) · [Gate counts](gate-counts.csv) · [9-round control](9-control/gates.txt) / [candidate](9-candidate/gates.txt) · [15-round control](15-control/gates.txt) / [candidate](15-candidate/gates.txt).

## Correctness and reproduction

- The combined build matches exact [ordered predicate/budget prefixes](ordered-prefix.log) and [long-row streams](dense-prefix.log). Both factors and the combined build match [dense order, topology and callback prefixes](dense-order.log), including under ASan/UBSan. [CTest 21/21](tests.log) and [full differential fuzzing](fuzz.log) pass on the combined build.
- [Slice verification](verification.log) matches fixtures, uncapped work, checksums, allocations and footprint. The factor screen matches the same staged reference on all 48 rows.

Reconstruct the narrow-stream candidate per its [report](../g2-bulk-streams-2026-11-20/README.md#correctness-and-reproduction) as `WORK/control`. Copy it to `WORK/inline`, `WORK/lookahead` and `WORK/candidate`; apply the respective patch or both patches. Verify [control](control-inputs.sha256), [inline](inline-inputs.sha256), [lookahead](lookahead-inputs.sha256) and [combined](candidate-inputs.sha256) source hashes. Build the four `*-build` directories with GCC 16.2.1 Release and pinned Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. Verify [`.text` hashes](text.sha256).

Run `bash check-code.sh WORK` after building. Run `bash run.sh WORK NEW_PILOT_DIR pilot` and `python3 summarize-pilot.py NEW_PILOT_DIR`, then `bash run.sh WORK NEW_OUTPUT_DIR full` and the [G2/G9 summarizer](../g2-cross-screen-2026-11-20/summarize.py). The runner pins CPU 6 and restores [all original power settings](power-after.txt). [Environment](environment.txt) · [Run order](run-order.csv). Run `bash verify.sh WORK CHECK_OUTPUT_DIR` separately from timings.

## Handoff

Use the combined build as the staged control. `uniform` at 100,000 still needs about 9% less full-step time (roughly 1.2 ms) against its 15-round upper bound. Prioritize a larger unordered improvement: test classifying several independent geometry gathers at a time on the narrow stream, keeping tag and scatter order unchanged. Bounded lookahead alone saved only a fraction of the gap; compare against it rather than stacking another prefetch distance blindly.

`split-line-translated` still needs about 4% less unordered time, but its G9 upper bound of 3.923× allows only about 2% unordered improvement at unchanged ordered time. To gain further ordered headroom, test compact child occupancy/bounds access on the common single-overlap path without altering DFS visit charges or callback prefixes. Retest G2/G9 together. G1/G2 still block release acceptance.
