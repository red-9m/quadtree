# 03 · Compact split-classifier input

**Staged, not retained.** Compact geometry makes `uniform` at 10,000 pass G2 in both repeats; all three 100,000-entry G2 failures remain in the 15-round run. G9 on `split-line-translated` at 100,000 becomes inconclusive in both repeats. Runtime source is unchanged.

## Candidate

Apply [candidate.patch](candidate.patch) to the [staged radix build](../g9-radix-row-sort-2026-11-20/README.md). It captures normalized `Box<T>` values in a separate typed workspace array rather than interleaving them with the filter, rank and payload pointer. The `int32` top-down classifier gathers from 16-byte boxes instead of 56-byte records. Dense commit prefetches both arrays on the existing unranked `int32` path. The prefix fallback, stable permutations, tag representation, topology, ordered/moved queries and duplicate-rank handling are unchanged. Other coordinate types use the split capture layout but retain their existing sorted build.

The total array bytes and reservation units are unchanged on the measured LP64 host. `Workspace::reserve` prepares both arrays before committing, so nonempty builds add one cold allocation, not a warm allocation. No extra geometry copy is written during capture. The allocation-failure tests enumerate the new request and check rollback.

## Frozen snapshot slice

Counts are pass / fail / inconclusive. G0, G3, G4 and G6 pass throughout. Each run has 68 completed library rows with zero warm allocations.

| Rounds | G2 control → candidate | G9 control → candidate | G1 control → candidate |
|---:|---:|---:|---:|
| 9 | 9 / 5 / 1 → 10 / 2 / 3 | 15 / 0 / 0 → 13 / 0 / 2 | 8 / 6 / 2 → 8 / 8 / 0 |
| 15 | 9 / 5 / 1 → 10 / 3 / 2 | 15 / 0 / 0 → 14 / 0 / 1 | 8 / 6 / 2 → 8 / 8 / 0 |

15-round `index` rows, milliseconds. Full-step estimates are separate from phase medians.

| Scene | n | Step before → after | Build before → after | Pairs before → after | G2 after (limit 2×) |
|---|---:|---:|---:|---:|---|
| `uniform` | 10,000 | 1.185 → 1.124 | 0.523 → 0.487 | 0.659 → 0.629 | 1.915 [1.853–1.978], pass |
| `uniform` | 100,000 | 15.236 → 14.251 | 7.637 → 7.292 | 7.361 → 6.926 | 2.252 [2.187–2.319], fail |
| `split-line` | 100,000 | 15.529 → 14.910 | 8.162 → 7.765 | 7.401 → 7.067 | 2.144 [2.076–2.214], fail |
| `split-line-translated` | 100,000 | 15.271 → 14.511 | 8.143 → 7.796 | 6.969 → 6.677 | 2.197 [2.130–2.267], fail |

- `uniform` at 10,000 is 5.1% faster in both repeats with intervals excluding no change. At 100,000, the three rows improve 4.0–6.5% at 15 rounds, but only `split-line` resolves a gain at nine rounds. Pair medians also moved, so the full-step gains cannot be attributed entirely to compact classification.
- For `split-line-translated` at 100,000, the 15-round ordered full step is 57.864 → 56.838 ms (change interval spans no change); the unordered improvement pushes the G9 interval from 3.789× [3.662–3.921] to 3.917× [3.791–4.048]. The nine-round candidate has wide intervals on this row and `mixed-size`; `mixed-size` passes again at 15 rounds.
- G1's two empty rows go from inconclusive to failure in both repeats. The compact geometry array is unused for empty snapshots; these runs do not attribute those timing changes to the geometry layout.
- [G2 rows, phases and work](g2-comparison.csv) · [G9 rows and phases](g9-comparison.csv) · [Matched step intervals](step-change.csv) · [Allocations and footprint](allocations.csv) · [Gate counts](gate-counts.csv) · [9-round control](9-control/gates.txt) / [candidate](9-candidate/gates.txt) · [15-round control](15-control/gates.txt) / [candidate](15-candidate/gates.txt).

## Correctness and reproduction

- [Exact full/moved-set comparison](equivalence.log) matches topology, handles and pair identities on 15 fixtures plus extreme and coincident boxes. [Unordered](unordered-prefix.log) and [ordered](ordered-prefix.log) callback/work prefixes match under finite budgets and early stops.
- [Slice verification](verification.log) matches uncapped fixtures, results, work and memory footprint. One extra cold allocation on every nonempty library row; zero warm allocations. [CTest: 21/21](tests.log), including allocation rollback and exception-disabled builds. [Full differential fuzzing](fuzz.log) passes all four coordinate types.

Reconstruct the [radix candidate](../g9-radix-row-sort-2026-11-20/README.md#correctness-and-reproduction) as `WORK/control`, copy it to `WORK/candidate`, and apply `patch -p1 < candidate.patch` in the copy. Verify [control inputs](control-inputs.sha256) and [candidate inputs](candidate-inputs.sha256). Build both Release with GCC 16.2.1 and pinned Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. [Executable `.text` hashes](text.sha256).

Run the [full/moved-set comparison](../g2-topdown-build-2026-11-20/verify.sh), [unordered budget probe](../g2-cross-screen-2026-11-20/verify-prefix.sh), [ordered budget probe](../g2-grouped-child-screen-2026-11-20/verify-prefix.sh), CTest, and `WORK/candidate-build/bench/quadtree-index-fuzz`.

Run the [frozen slice runner](../g2-cross-screen-2026-11-20/run.sh) with `WORK NEW_OUTPUT_DIR` on an idle host matching the [environment](environment.txt). It pins CPU 6, records the [run order](run-order.csv) and [restores](restored-mode.txt) the platform profile. Run `python3 summarize.py NEW_OUTPUT_DIR`. Each run directory contains gate output, full snapshot CSV and compressed raw samples.

## Handoff

Keep the compact patch staged as the G9 stress case, not as a runtime change. Prototype a faster ranked-build duplicate check on the radix control first: sort compact full-width ranks in prepared scratch instead of comparing and swapping 24-byte spatial keys. Preserve duplicate status and precommit rollback. The [build profile](../g2-build-traffic-2026-11-20/README.md#attribution) puts rank validation at 28–29% of ordered build cycles, separate from rank insertion and query sorting.

Retest it with the compact candidate before another unordered change. At the 15-round candidate upper bounds, `uniform` and `split-line-translated` need another ~14% and ~12% unordered full-step gain if uncertainty stays fixed. Cutting the latter's unordered step by 12% would require roughly 13% lower ordered time to clear G9. Rank validation alone is too small to promise that gain; size any follow-up rank-insert or ordered-traversal work against this budget. G1/G2 still block release acceptance.
