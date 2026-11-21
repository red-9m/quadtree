# 03 · Remaining costs on the narrow-stream build

**First ordered target: remove the out-of-line four-bit popcount.** Its helper and PLT stub account for 11.2–11.3% of sampled ordered pair cycles on `split-line-translated`. On unordered builds, classification and scatter still take 56–58% of sampled build cycles including partition overhead. No runtime or gate verdict changed.

## Unordered build attribution

Two independent captures, `int32`, 100,000 entries. Shares are percent of sampled build cycles, not removable time.

| Scene | Classify | Scatter | Partition total | Dense commit | Topology commit |
|---|---:|---:|---:|---:|---:|
| `uniform` | 29.0–31.7 | 21.1–22.4 | 57.2–57.7 | 13.7 | 15.1–15.2 |
| `split-line-translated` | 30.7–32.2 | 19.9–20.5 | 56.1–58.4 | 14.5–16.3 | 15.3 |

- Four indexed `Box<int32>` edge loads in `planBulk` still account for 66–76% of sampled build loads with a known source outside L1. The preceding compact ID load accounts for less than 1%; scatter accounts for 0.3–3.2%. Dense commit accounts for 19–27%, including loads attributed to the prefetch path. These are sampled operations, not cache transactions, bytes or latency; multiple edge loads can touch one cache line.
- The unprofiled 15-round build medians are 6.454 ms (`uniform`) and 7.059 ms (`split-line-translated`). A roughly 1.4 ms full-step gap on each corresponds to about one-third of the partition phase if the cycle shares approximate wall-time shares. Eliminating scatter alone would not close either gap at those shares; reducing geometry gather stalls or a separate pair-phase cost needs to be part of the change. Unordered `crossPairs` node work (33–35% of pair cycles) and exact eligibility (32–39%) remain the largest pair buckets.

[Build attribution](build-attribution.csv) · [IBS data sources](ibs-data-sources.csv) · [Indexed-load PCs](focus-loads.csv) · [Instruction windows](hot-instructions.asm).

## Ordered walk attribution

Percent of sampled pair cycles; dispatch includes the helper, which is also shown separately.

| Scene | Walk total | Occupancy/bounds | Dispatch/charges | Of which: popcount helper + PLT | Source sort | Row sort | Rank check/collect |
|---|---:|---:|---:|---:|---:|---:|---:|
| `split-line-translated` | 62.2–63.6 | 30.8–31.6 | 23.9–24.1 | 11.2–11.3 | 13.6 | 2.2–2.3 | 8.4–9.1 |
| `mixed-size` | 35.4–36.2 | 11.6–11.9 | 11.4–12.1 | 5.0–5.4 | 4.1–4.2 | 18.1–18.2 | 21.5–21.9 |

The portable `-O3` executable lowers `std::popcount` at both charge sites to `__popcountdi2`. The count-only [census](census.csv) derives 1,974,636 charge calls on the translated fixture and 2,730,657 on `mixed-size`: one per admitted child plus one when occupied siblings remain at the end of the group. All four child slots are occupied in 84.9% and 98.3% of groups, respectively. Exactly one child overlaps in 89.0% of translated groups versus 49.1% on `mixed-size`.

**Repeated source work:** Within eight consecutive rank-ordered sources, 22.4% of translated group visits and 23.7% of `mixed-size` group visits repeat a parent already seen in that batch. At depths 0–2 those shares are 67.1% and 58.6%. This estimates reuse of child metadata only: geometry tests and budget charges remain source-specific. Full containment of the parent bounds never occurs for translated groups, versus 8.8% for `mixed-size`; containment alone does not target the translated row. The census does not time batching and does not reorder callbacks.

[Pair attribution](pair-attribution.csv) · [Cycle periods](cycle-periods.csv) · [Per-PC breakdown](sites.csv.gz) · [Raw callchains and run logs](raw-callchains.tar.gz).

## Verification and reproduction

All 12 captures match the [15-round narrow gate rows](../g2-bulk-streams-2026-11-20/15-candidate/snapshot.csv) on fixture hashes, pairs, checksums, work, footprint and cold allocations, with zero warm allocations and no reported sample loss. All four census runs match gate results and reconcile charged node visits. [Verification](verification.log) · [Profile rows](profile-rows.csv) · [Run totals](run-totals.csv).

Reconstruct `WORK/narrow`, `WORK/narrow-build` and the pinned Box2D per the [narrow-stream report](../g2-bulk-streams-2026-11-20/README.md#correctness-and-reproduction). Run `bash profile.sh WORK NEW_CAPTURE_DIR` on an idle Ryzen with `ibs_op` and CPU 6 available. It samples the frozen Release executable and uses a [`.text`-matched](text.sha256), [layout-matched](text.layout) debug twin only for symbolization. `cycles:u` sums sample periods; source-line attribution is approximate. IBS dispatch-mode sampling is a separate run. [Environment](environment.txt) · [Run order](run-order.csv) · [Power settings before](power-before.txt) / [after](power-after.txt).

Run `bash census.sh WORK/narrow > RESULT_DIR/census.csv` and `python3 summarize.py CAPTURE_DIR DEBUG_ELF RESULT_DIR`. For archived captures keep the recorded `elf-path.txt` intact and use the matching debug twin. Timing ratios and verdicts come from unprofiled gate runs only.

## Handoff

Stage an inline four-bit count at the two `walkOrdered` charge sites first; preserve exact finite-budget charges, predicate order and callback prefixes. Compare emitted code without changing target flags. The translated ordered row needs roughly 4.8 ms less full-step time if unordered improves 10% and uncertainty stays fixed. The helper's sampled share is equivalent to about 4.6 ms at the observed 40.844 ms pair median, before replacement overhead; do not assume the helper change is sufficient. If more is needed, test compact child occupancy/bounds metadata or reuse at upper levels without precharging future sources.

For unordered, test overlapping the indexed geometry loads with classification (e.g., independent iterations or bounded lookahead) on the narrow stream before adding another geometry representation. Measure build and full step together; use the prior centers/spans result as a negative control. Retest G2/G9 together before retaining a combined candidate. G1/G2 still block release acceptance.
