# 03 · Radix-sort long ordered partner rows

**Staged: G9 passes all 15 rows in both repeats.** The `mixed-size` 100,000-entry ordered step is 10.9–18.1% faster than the screened control. G1 and G2 still fail; no runtime patch was retained.

## Candidate

Apply [radix-row-sort.patch](radix-row-sort.patch) on the [grouped-screen build](../g2-grouped-child-screen-2026-11-20/README.md), not on the rejected cached-introsort patch. Full ordered `int32` self queries with at least 128 partners cache the full 64-bit ranks and dense IDs, collect all eight byte histograms in one pass, skip constant bytes, then stably scatter by byte from least to most significant. The sorter uses both inactive build buffers, so it requires existing `WorkspaceReservation::buildEntries` capacity for the row; otherwise the original allocation-free index sort runs. There are no new buffers, reservation requirements, or changes to source sort, short-row sort, traversal, or moved-set queries.

## Profiles before gate timing

Three `cycles:u` captures per binary used the same `mixed-size` fixture and `.text`-matched symbol twins. Total partner-row sort share fell from 28.31–28.98% to 19.42–20.60% of sampled pair cycles, including radix packing, histogram setup, scatter and write-back. Source sorting remains 3.50–3.98%. This attributes a lower share to the row sort; the independent full-step measurements below establish the gain. Profile timings are not gate timings.

[Attribution](attribution.csv) · [Row-sort PC periods](row-pc-periods.csv) · [Raw callchains](raw-callchains.tar.gz) · [Profile verification](profile-verification.log).

## Frozen snapshot slice

Counts are pass / fail / inconclusive. G0, G3, G4 and G6 pass throughout. All 68 library rows per run finished with zero warm allocations.

| Rounds | G2 screened → radix | G1 screened → radix | G9 screened → radix |
|---:|---:|---:|---:|
| 9 | 9 / 5 / 1 → 9 / 4 / 2 | 8 / 6 / 2 → 8 / 8 / 0 | 14 / 1 / 0 → 15 / 0 / 0 |
| 15 | 9 / 6 / 0 → 9 / 5 / 1 | 8 / 7 / 1 → 8 / 7 / 1 | 14 / 1 / 0 → 15 / 0 / 0 |

`mixed-size`, `index-ordered`, 100,000 entries. Times in ms; build and pair medians are separate from the full-step estimates.

| Rounds | Step before → after | Build before → after | Pairs before → after | G9 before → after (limit 4×) |
|---:|---:|---:|---:|---:|
| 9 | 171.793 → 153.114 | 15.485 → 15.217 | 156.287 → 136.010 | 4.186 [4.068–4.307] → 3.708 [3.588–3.833] |
| 15 | 191.943 → 157.124 | 17.103 → 15.175 | 174.200 → 142.260 | 4.365 [4.072–4.680] → 3.723 [3.612–3.838] |

- `mixed-size` ordered full-step gain resolves in both repeats (candidate/control 0.891 [0.862–0.921] and 0.819 [0.773–0.867]); the matched unordered intervals include no change. The larger repeat also shows build-phase movement, so do not attribute its entire gain to sorting.
- G2 gains no pass. At 100,000, the radix candidate's G2 upper bounds are 2.415× (`uniform`), 2.295× (`split-line`), and 2.428× (`split-line-translated`), versus 2×. G1 empty-row outcomes vary; both are failures in the nine-round candidate run but that is not reproduced at 15 rounds.
- [All G2 rows, counts and phases](g2-comparison.csv) · [Matched full steps and phases](step-change.csv) · [9-round screened](9-control/gates.txt) / [radix](9-candidate/gates.txt) · [15-round screened](15-control/gates.txt) / [radix](15-candidate/gates.txt).

## Correctness and reproduction

The [dense probe](dense-prefix.log) matches exact statuses, predicates, visitor pairs and work with full, partial and zero build scratch, full-width ranks, finite budgets and early stops. [Finite-prefix](budget-prefix.log) and [full/moved-set](equivalence.log) probes match too. [Full-slice verification](verification.log) matches uncapped work, allocations and footprint; [CTest passed 21/21](tests.log).

Reconstruct the screened source and copy it; apply `radix-row-sort.patch` only to the copy. Use [input hashes](control-inputs.sha256), [radix input hashes](candidate-inputs.sha256), [`.text` hashes](text.sha256), and the [screening experiment's](../g2-grouped-child-screen-2026-11-20/README.md#reproduction-and-handoff) GCC 16.2.1 Release and pinned Box2D. Run the [dense probe](../g9-cached-row-sort-2026-11-20/verify-dense.sh), earlier prefix and full/moved probes, and CTest. Run `bash profile.sh WORK NEW_CAPTURE_DIR` with `control`, `candidate`, their `-build` directories and `box2d` under `WORK`, then `python3 summarize-profile.py CAPTURE_DIR RESULT_DIR`. For archived captures, retain the recorded DSO paths in `*.elf-path.txt` and supply the matching debug twins. See [profile run order](profile-order.csv), [profile hashes](profile-text.sha256), [timing run order](run-order.csv), [environment](environment.txt) and [restored profile](restored-mode.txt). Frozen timing uses CPU 6 and the full-slice runner; each run directory contains raw samples.

## Handoff

Keep radix and screening staged as the ordered control. Return to unordered G2: `uniform` and `split-line-translated` at 100,000 need about 17–18% less full-step time to clear their observed upper bounds at unchanged uncertainty. Screen disjoint child pairs in `crossPairs` while preserving visit charges, and pair this with an exact-eligibility or build-traffic improvement sized to the remaining gap. Retest G2 and G9 together: at an upper bound of 3.838×, a roughly 4% drop in the `mixed-size` unordered denominator consumes the current G9 headroom unless ordered time also drops. Release acceptance remains blocked.
