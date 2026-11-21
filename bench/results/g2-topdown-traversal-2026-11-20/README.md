# 03 · Pair traversal on the staged top-down build

**Split-line G9 is a walk problem; `mixed-size` is not just a walk problem.** `walk` accounts for 63–65% of sampled ordered pair cycles on the two split-line scenes. On `mixed-size`, `walk` accounts for 32–33%, rank sorting for 29–30%, and per-entry rank checks/collection for 21%. No runtime source or gate verdict changed.

## Sampled pair-phase attribution

All rows are `int32`, 100,000 entries, using the [staged top-down patch](../g2-topdown-build-2026-11-20/candidate.patch). Ranges are the two independent process profiles. Shares use sampled `cycles:u` periods, not wall time or estimates of removable work.

| Scene and engine | Walk gate + iteration | Rank sorting | Per-entry rank check + collection | `crossPairs` node work | Exact pair eligibility |
|---|---:|---:|---:|---:|---:|
| `split-line`, ordered | 63.5–63.8% | 12.6–13.6% | 10.7–11.4% | — | 5.6% |
| `split-line-translated`, ordered | 63.4–64.6% | 12.7–13.1% | 11.4–11.8% | — | 4.5–4.7% |
| `mixed-size`, ordered | 32.4–32.5% | 29.1–30.1% | 20.7–21.5% | — | 10.7–10.9% |
| `uniform`, unordered | 4.50% | — | — | 31.4% | 39.4–40.5% |
| `split-line-translated`, unordered | 4.3–5.2% | — | — | 32.5–32.6% | 31.5–32.6% |

[Full attribution](attribution.csv) · [Period totals by phase](phase-periods.csv) · [Per-PC and inline-frame histogram](pc-histogram.csv.gz). Source-line attribution in optimized code is approximate; the census below independently verifies work counts.

## Count-only diagnostic

- **Ordered split-line scenes:** 2.97–3.01 million nonempty `walk` visits reject disjoint bounds after charging the node budget (69.5–69.7% of charged visits). Another 279,072–322,677 empty calls do not charge. Both scenes have 99,999 source rows: 79,502 have no more than one partner, only 66 have more than 16, and the longest has 23.
- **Ordered `mixed-size`:** 3.42 million disjoint visits (63.3% of charged `walk` visits), but 12.60 million per-entry rank checks versus 0.95–0.96 million on split-line. Rank rejects 49.9% of those entries before candidate charging. The 1,190 rows longer than 64 hold 47.8% of reported partners; the longest has 5,628. The rank-sort sample bucket includes source and row sorting; the census does not assign cycles to either.
- **Unordered G2:** 86.0% (`uniform`) and 88.4% (`split-line-translated`) of charged `crossPairs` visits reject disjoint bounds. Filter rejects none of these node visits. Exact pair eligibility is at least as large a cost as `crossPairs` node work on `uniform`, and comparable on `split-line-translated`.

[Exact census](census.csv) · [Verification](verification.log). All five census rows reconcile with the benchmark counts and checksums. All ten Release profiles match the staged gate run's fixtures, checksums, candidate counts and node visits; zero warm allocations throughout. The diagnostic binaries were not timed.

## Handoff

- Stage a grouped child bounds screen for the ordered `walk` on the two split-line scenes. Avoid recursive calls for disjoint children, not their existing visit charges: retain DFS charge order, filter counters, stop semantics and ordered row-prefix behavior under finite budgets. Compare exact charged visits and callbacks before timing.
- For `mixed-size`, separate source sort from row sort and test a dense-row change alongside the walk change. A small-row-only sort does not target the long tail. Rank-rejected entries cannot be skipped in a way that changes charged work or incomplete results.
- The worst unordered G2 rows still need roughly 16–18% full-step reduction against the observed upper bounds. Saving only a fraction of `crossPairs`'s roughly one-third of pair cycles is not sized to close that gap; include an eligibility or build improvement. Retest G2 and G9 together: faster unordered steps make G9's denominator smaller. No acceptance pass inferred from profiles.

## Reproduction

Reconstruct the [staged candidate source and Release executable](../g2-topdown-build-2026-11-20/README.md#reproduction). Use CPU 6, the recorded [environment](environment.txt), and performance profile/preferences, restoring them afterward. Run the [profiler](profile.sh) with `CANDIDATE_SOURCE CANDIDATE_BUILD BOX2D_SOURCE NEW_CAPTURE_DIR`. It profiles the same Release executable used for the gate work; the symbolization twin must match the [`.text` hashes](text.sha256) and [layout](text.layout). [Run order](run-order.csv) · [Raw callchains and per-process results](raw-callchains.tar.gz) · [Restored profile](restored-mode.txt).

Run `bash census.sh CANDIDATE_SOURCE > RESULT_DIR/census.csv` and `python3 summarize.py CAPTURE_DIR RELEASE_ELF DEBUG_TWIN RESULT_DIR`. Use the Release DSO path recorded in the archive when analyzing saved captures. The `.text` equality check is for attribution, not a timing result.
