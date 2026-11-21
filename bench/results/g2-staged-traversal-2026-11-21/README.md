# 03 · Unordered traversal on two-stream + source radix

**`crossPairs` node and loop work is 31.0–36.0% of sampled pair cycles on the three G2 blockers.** Its exact eligibility adds another 14.3–23.2%. Count actual cross-call and rejection shapes before choosing a change. No new timing, code, or gate result.

## Release attribution

Reused six unordered `cycles:u` captures from the [classifier report](../g2-bulk-classifier-2026-11-21/README.md), two reversed blocks on the exact staged ELF and verified `.text` twin. The nearest inlined or physical traversal caller owns each stack; `walk` reached from `itemsVsSubtree` stays in the holder bucket. Exact includes `pairQualifies` and its inlined tests. Reporting is separate in the CSV; together with the displayed columns it accounts for the classified pair phase. At most one sample (0.025%) lacks a resolved owner. These are sampled cycle shares, not stall counts or removable time.

| Unordered fixture | `selfPairs` node/loop | `selfPairs` exact | `crossPairs` node/loop | `crossPairs` exact | Holder node/walk | Holder exact |
|---|---:|---:|---:|---:|---:|---:|
| `split-line` 10,000 | 7.57–8.35% | 13.89–14.39% | 35.97–36.02% | 14.69–15.68% | 14.64–14.95% | 1.51–1.70% |
| `uniform` 100,000 | 7.82–8.69% | 15.04–15.21% | 30.96–33.08% | 21.91–23.16% | 11.55–11.84% | 1.14–1.30% |
| `split-line-translated` 100,000 | 7.57–8.71% | 14.31–15.03% | 32.82–33.55% | 14.34–14.68% | 12.56–12.90% | 2.55–2.97% |

[Attribution and counts](attribution.csv) · [Hot PCs and inline locations](hot-pcs.csv) · [Verification](verification.log). All six profiles match frozen fixtures, checksums, work and zero warm allocations; attribution reconciles with all six prior pair-phase totals. Sampled cross-node PCs include the occupancy and bounds gate, charge, and child loops. They don't tell how many calls exercise each exit or how a different screen would schedule on this CPU.

To reproduce, recover `CAPTURE_DIR` and the hash-matched `staged.debug` per the [classifier report](../g2-bulk-classifier-2026-11-21/README.md#reproduction). Run `python3 summarize.py CAPTURE_DIR RESULT_DIR`.

## Handoff

**Census cross-node admission on the same topology.** Count sibling versus recursive calls, empty pairs, leaf/internal combinations, both disjoint axes and admitted pairs, plus exact pair work by origin. Reconcile exact output and charged work with the real query. This will distinguish geometry-test opportunities from removing recursive gate/call overhead. Do not restage the [older child-bound screen](../g2-child-bounds-only-2026-11-20/README.md) without a materially different factor.
