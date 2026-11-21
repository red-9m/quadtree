# 03 · Exact item eligibility census on two-stream + source radix

**Disjoint geometry dominates direct cross-node candidates; contact and degeneracy are rare.** Across 4,441,192 exact calls in four fixtures, no item filter rejects and only 11 calls have a degenerate operand. Keep both semantics; the workload does not exercise their uncommon cases.

The [shadow traversal](../g2-cross-admission-census-2026-11-21/census.cpp) has an untimed `--item` mode. For each origin it stratifies by degenerate operands, then counts filter rejection or intersection outcomes (X-only, Y-only, both-axis disjoint, closed contact with zero extent, positive extent). Strictly interior point/line hits can satisfy strict endpoint comparisons even though their intersection has zero extent; the census uses intersection extent and independently checks the Default acceptance rule, counters and exact callback stream. Report and work totals match the previous frozen rows.

| Fixture | Disjoint: self / cross / holder | Closed contact: self / cross / holder | Degenerate calls |
|---|---:|---:|---:|
| `split-line` 10,000 | 79.19 / 92.24 / 67.81% | 1.14 / 0.72 / 2.71% | 11 |
| `uniform` 100,000 | 74.67 / 96.40 / 67.48% | 1.99 / 0.47 / 4.10% | 0 |
| `split-line-translated` 100,000 | 73.52 / 92.18 / 53.23% | 1.24 / 0.66 / 2.86% | 0 |
| `mixed-size` 100,000 | 71.82 / 94.37 / 7.87% | 1.89 / 0.42 / 0.32% | 0 |

- All 11 degenerate calls are disjoint (five self, six cross, at 10,000 `split-line`). The zero filter-reject count is a property of these fixtures, not justification to bypass the filter. `mixed-size` holder work is a hit-heavy guard.
- [Outcomes by axis and degeneracy](item-outcomes.csv) · [Summary](summary.csv) · [Exact work](work.csv) · [Verification](verification.log). Timing is unchanged.
- Reproduce with `bash ../g2-cross-admission-census-2026-11-21/run.sh STAGED_SOURCE NEW_OUTPUT_DIR --item` and `python3 summarize.py NEW_OUTPUT_DIR`.

## Handoff

**Audit the exact eligibility instructions on the captured Release control.** Use the census alongside the current [pair attribution](../g2-staged-traversal-2026-11-21/README.md); separate filter/candidate charging, geometry-mode dispatch, and endpoint comparisons. Keep `mixed-size` holder work in mind; cross-branch frequency is not a cycle-cost estimate.
