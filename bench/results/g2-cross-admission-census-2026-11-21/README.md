# 03 · Cross-node admission census on two-stream + source radix

**About 86% of cross calls are recursive, and 86–88% of charged cross pairs reject on disjoint bounds.** Neither axis dominates the one-axis rejects. A node-only packed overlap test targets the common gate without changing recursion or charges. No timing or gate rerun.

The [untimed shadow traversal](census.cpp) reads the actual staged index built by the benchmark adapter, replays its unordered traversal with the library's exact eligibility and callback orientation, checks the real query stream pair-by-pair, and reconciles charged visits, candidates and filter/report counters. Four fixtures match the frozen staged rows. This is a census of unlimited work, not a latency estimate. Budget ordering will be tested in any candidate.

| Fixture | Cross calls | Recursive | Empty, % of calls | Disjoint, % of charged | X-only / Y-only / both, % of disjoint | Leaf/leaf, % of disjoint | Cross own exact, % of candidates |
|---|---:|---:|---:|---:|---:|---:|---:|
| `split-line` 10,000 | 41,784 | 85.81% | 5.94% | 86.86% | 27.39 / 27.73 / 44.88 | 54.93% | 62.52% |
| `uniform` 100,000 | 346,812 | 85.98% | 1.34% | 85.98% | 28.41 / 28.30 / 43.29 | 55.61% | 69.91% |
| `split-line-translated` 100,000 | 382,758 | 85.55% | 7.94% | 88.35% | 26.39 / 26.12 / 47.49 | 53.35% | 56.67% |
| `mixed-size` 100,000 | 341,868 | 86.07% | 1.33% | 86.13% | 28.25 / 28.19 / 43.56 | 56.25% | 20.51% |

No admitted overlap reached the cross-node filter rejection branch on these fixtures. Do not remove it: nondefault filters remain part of the contract. The far larger recursive share argues against a sibling-only variant. The 43–47% both-axis share means this census does not establish that evaluating both axes beats an early exit; test it.

[Summary](summary.csv) · [Shape/origin counts](cross-shapes.csv) (origin `0` sibling, `1` recursive; shape = number of internal nodes) · [Work reconciliation](work.csv) · [Verification](verification.log). Run `bash run.sh STAGED_SOURCE NEW_OUTPUT_DIR` and `python3 summarize.py NEW_OUTPUT_DIR`; no modification to the library source is needed.

## Handoff

**Screen a packed signed `int32` closed-bounds test only at `crossPairs` admission.** On SSE2, pack the two lower corners and the opposite two upper corners, compare all four signed lanes and reject if any minimum exceeds its matching maximum. Preserve touch-inclusive node screening and no subtraction at extremes. Keep the portable scalar fallback, occupancy and charge order, filters, recursion, callback prefixes, and exact item eligibility. Verify all extremes/contacts, finite budgets and emitted Release instructions, then time both engines at both blocker sizes. This is not the old grouped child screen; it neither changes call count nor batches charges. G1/G2 still block 08.
