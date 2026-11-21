# 03 · Grouped child bounds screen

**Staged, not retained.** Both 100,000-entry split-line G9 rows pass in the 15-round repeat, but remain inconclusive at nine rounds. `mixed-size` still fails G9 in both repeats. G2 still fails; runtime source is unchanged.

## Candidate

Apply [child-screen.patch](child-screen.patch) after the [top-down bulk patch](../g2-topdown-build-2026-11-20/candidate.patch). The control is the staged top-down build, not the shipped source. Only nonflat, full ordered `int32` self queries use the new walk. The parent classifies its four children, skips recursion into disjoint bounds and charges their visits in groups before the next overlapping child (or at the end). Empty children remain uncharged. Filtering and callbacks retain depth-first order. A partially affordable group consumes the remaining node budget and returns `BudgetExhausted`; the unfinished ordered row is discarded. No workspace, topology, moved-set path or unordered algorithm changed.

## Frozen snapshot slice

Counts are pass / fail / inconclusive. G0, G3, G4 and G6 pass in all four runs. Both binaries ran the full 204-row slice at each repeat; all 68 library rows per run completed with zero warm allocations.

| Rounds | G2 control → screened | G1 control → screened | G9 control → screened |
|---:|---:|---:|---:|
| 9 | 9 / 4 / 2 → 9 / 4 / 2 | 8 / 6 / 2 → 8 / 7 / 1 | 12 / 2 / 1 → 12 / 1 / 2 |
| 15 | 9 / 3 / 3 → 9 / 4 / 2 | 8 / 6 / 2 → 9 / 6 / 1 | 12 / 1 / 2 → 14 / 1 / 0 |

15-round repeat, `index-ordered`, 100,000 entries, milliseconds per snapshot. Step estimates are separate from build and pair medians.

| Scene | Step before → after | Build before → after | Pairs before → after | G9 before → after (limit 4×) |
|---|---:|---:|---:|---:|
| `split-line` | 62.883 → 60.778 | 16.091 → 16.637 | 46.236 → 43.262 | 4.072 [3.943–4.207] → 3.794 [3.621–3.975], pass |
| `split-line-translated` | 67.025 → 57.967 | 17.184 → 16.230 | 49.722 → 40.559 | 4.185 [3.958–4.425] → 3.737 [3.538–3.947], pass |
| `mixed-size` | 191.064 → 173.929 | 16.437 → 15.679 | 171.358 → 156.739 | 4.461 [4.290–4.638] → 4.196 [4.061–4.337], fail |

- At nine rounds, all three matched ordered full-step intervals include no change. The 15-round full-step gain resolves on `split-line-translated` and `mixed-size`, not on `split-line`. `mixed-size` needs roughly another 8% ordered step reduction at the observed upper bound if the unordered denominator and uncertainty stay fixed.
- At 15 rounds the worst G2 upper bounds are 2.466× (`uniform`) and 2.446× (`split-line-translated`), both at 100,000 versus a 2× limit. The ordered-only patch does not target unordered work. Unordered measurements also moved: `uniform` at 100,000 slowed 7.5% in the 15-round repeat, so the G9 ratio alone cannot isolate the ordered gain. G1's extra nine-round failure is `clustered` at zero entries; it was not reproduced at 15 rounds.
- [All row verdicts and counts](g2-comparison.csv) · [Matched phase and step estimates](step-change.csv) · [9-round control](9-control/gates.txt) / [screened](9-candidate/gates.txt) · [15-round control](15-control/gates.txt) / [screened](15-candidate/gates.txt).

## Correctness

- [Budget-prefix probe](budget-prefix.log): byte-identical statuses, work counters, predicate calls and visitor pairs across node/candidate limits and early stops, with both boundary modes and instrumentation on/off. Built with `-fno-exceptions`.
- [Exact full/moved comparison](equivalence.log) matches the staged control on 15 fixtures and extreme coordinates. [Slice verification](verification.log) matches all uncapped work, allocations and footprint. [CTest: 21/21](tests.log).

## Reproduction and handoff

Reconstruct the top-down control and copy it; apply `patch -p1` with `child-screen.patch` only to the copy. Verify [input hashes](control-inputs.sha256) / [screened hashes](candidate-inputs.sha256) and [`.text` hashes](text.sha256). Build both Release with GCC 16.2.1 and the same pinned Box2D as the [top-down repeat](../g2-topdown-build-2026-11-20/README.md#reproduction). Run `bash verify-prefix.sh CONTROL_SOURCE SCREENED_SOURCE` and the earlier exact probe and CTest. Run the frozen full-slice runner on CPU 6 in [recorded order](run-order.csv), using the [environment](environment.txt) and [restoring the profile](restored-mode.txt) afterward. Raw samples are in each run directory.

**Next:** separate source-rank sort from dense-row sort on `mixed-size` with the screen applied. The prior [traversal profile](../g2-topdown-traversal-2026-11-20/README.md#count-only-diagnostic) found 47.8% of pairs in rows longer than 64. Use the breakdown to choose a row-sort candidate rather than retuning short rows. G2 still needs its own unordered improvement; retest G2 and G9 together before retaining anything.
