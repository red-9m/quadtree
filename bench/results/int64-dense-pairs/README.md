# Task 09: int64 dense-row follow-up

`a58cff8` remains held: its target's pooled release interval ended at 2.016. This is a new candidate, `679a2b2106815ac459f8d2d12a0a7ed9905eb033`. No limit, exception, fixture, reference engine or budget was changed. [Prespecified screen](rule.md), [screen driver](screen.sh).

| Variant | G2 upper bounds: 5 / 45 / 45 rounds | Advance? |
|---|---|---|
| `dense`: specialize contiguous entry loops | 2.079 / 1.974 / 2.017 | no |
| `direct`: dense loops plus direct warm contiguous build commit | 2.010 / 1.976 / 1.971 | no |
| `common`: root filter proof plus batched dense rows | 1.958 / 1.906 / 1.918 | yes |

Only `common` is in the candidate. During bulk capture intersect every entry's category and mask. A nonzero shared bit proves both filter directions for every pair. Store the proof at the root; invalidate it on any insertion or filter change, and default it to false on a new topology. Only the existing narrow-span int64 path uses it. For contiguous unordered rows, charge the exact prefix consumed once per row; visitor stops, finite budgets and saturating counters retain their original semantics. Mixed filters, wide coordinates and non-SSE2 paths keep normal checks. No additional allocation. `int64PairPrefixes()` compares exact callback order, statuses and work counters against the generic predicate traversal, before and after filter changes and inserts.

Target: `uniform`, n=100000. Each of the three processes passed G2/G6/G9 independently. With `common`, pair medians were 5.51–5.56 ms versus 6.20–6.22 ms on the held candidate's 45-round resolution. Same 788,738 candidates, 73,905 pairs, checksum `47126aad363c014b`, and zero warm allocations. Each variant directory includes raw samples, input arguments, gates, binary digest and complete source delta.

**Promotion:** separate full release-protocol `int32`/`int64` matrices, clean archived candidate build, 22 CTests and nine [hosted CI jobs](https://github.com/red-9m/quadtree/actions/runs/45685264321). The official five-round target must also pass without pooling. Previously passing rows (including `a58cff8`'s int64 rows) cannot be dropped. See `data/run-order.csv`, `data/final-snapshot-status.txt` and finally `data/decision.txt`; missing evidence is not a pass. Workspace pointer: `~/.cache/quadtree-common-i64.current`. No automatic merge or int64 type acceptance.
