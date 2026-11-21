# 03 · Ordered rank-table insertion profile

**Target the rank-table epoch load, not hash mixing or collision handling.** The probe's address/load/test block accounts for 90–94% of sampled rank-insertion cycles. At 100,000 entries the frozen fixtures use 1.309 probes per rank and never convert to the tree. No runtime change or gate rerun.

## Staged Release attribution

`index-ordered`, `int32`, 100,000 entries; two reversed capture blocks per scene. The binary is the hash-checked direct-child-init build from the [G2/G9 retest](../g2-topology-cost-2026-11-20/README.md#frozen-snapshot-slice), not the former control. Ranges are percent of sampled **build** cycles; duplicate validation is separate from rank insertion.

| Scene | Rank insert total | Epoch address / load / branch | Hash mix + mask | Probe stats + bound | Bucket publish | Duplicate validation |
|---|---:|---:|---:|---:|---:|---:|
| `uniform` | 20.0–21.8 | 18.7–20.5 | 0.2–0.5 | 0.4–0.5 | 0.1–0.2 | 16.4–16.5 |
| `split-line` | 16.9–19.4 | 15.3–18.1 | 0.3–0.7 | 0.4–0.6 | 0.1 | 15.1–15.4 |
| `split-line-translated` | 19.8–20.7 | 18.2–19.0 | 0.5–0.6 | 0.4–0.5 | 0.1–0.3 | 15.9–16.1 |
| `mixed-size` | 18.7–21.2 | 17.4–19.0 | 0.2–0.7 | 0.6–0.8 | 0.1–0.2 | 16.3–16.4 |

- The [audited `insertRank` assembly](rank-instructions.asm) reads the four-byte bucket epoch at `0x14429d`; its dependent branch is `0x1442a1`. Nearly all epoch-block cycle samples land on that branch. This localizes the cost to the probe test but is not a direct load-latency or branch-misprediction measurement: ordinary `cycles:u` samples can skid. GCC holds the cumulative probe count in a register across the loop and stores it once on success, not once per bucket (`quadtree-index.h:1137–1160` on the staged source).
- IBS confirms non-L1 sources at the epoch load itself: **80/103** pooled samples (50 `RAM hit`, 27 `core, same node Any cache hit`, 3 `L2 hit`; 23 `L1 hit`). On translated, 22 of 29 are non-L1 across the two blocks. There are only 5–17 epoch samples per capture; use these as evidence of poor locality, not precise hit rates or bytes. Audited probe-counter loads all report L1. Store sources are not a write-bandwidth measurement.
- No `buildRankTree` or splay samples. The skipped branches on the hash path still preserve the collision limit. `uniqueBuildRanks` at `detail/quadtree-index-impl.h:2264` remains a separate build cost; neither the census nor this profile changes it.

[Cycle periods](cycle-periods.csv) · [Per-PC periods](cycle-sites.csv) · [IBS data sources](ibs-data-sources.csv) · [Audited operands](focus-pcs.csv).

## Untimed probe census

All six frozen large-scene fixtures, 1,000 / 10,000 / 100,000 entries, using the benchmark adapter's ranks. The [census](census.csv) reads actual rank-store counters and verifies the warm probe total with `sum(displacement + 1)` from the resulting buckets. At a given size the six scenes share the same rank set and probe totals, even though their dense insertion order and displacement histories can differ.

| Entries | Buckets | Load factor | Probes | Probes / rank | Max displacement across scenes |
|---:|---:|---:|---:|---:|---:|
| 1,000 | 2,048 | 48.83% | 1,528 | 1.5280 | 10–14 |
| 10,000 | 32,768 | 30.52% | 12,089 | 1.2089 | 9–11 |
| 100,000 | 262,144 | 38.15% | 130,866 | 1.3087 | 19–27 |

At 100,000, ~81% land in their home bucket; fewer than 0.09% move eight buckets or more. The table is 4 MiB. Unique cache lines containing the **home epochs alone** span ~3.14 MiB in total, before accounting for collision probes; occupied epoch lines span ~3.22 MiB. Cold builds initialize every bucket once; warm rebuilds reset no cells and have no allocations. No conversion, cold or warm, in any census row. These counts do not cover adversarial ranks.

[Census summaries](census-summary.csv) · [Reconciliation](verification.log).

## Reproduction

Recreate `WORK/candidate`, `WORK/candidate-build`, and `WORK/box2d` from the [direct-init report](../g2-topology-cost-2026-11-20/README.md#correctness-and-reproduction). Run `bash profile.sh WORK NEW_CAPTURE_DIR` on an idle host with AMD `ibs_op` user filtering. It verifies [source inputs](../g2-topology-cost-2026-11-20/candidate-inputs.sha256), [.text hash](text.sha256) and [.text layout](text.layout), pins CPU 6 (`CPU` override), and leaves power settings unchanged. The matched debug ELF is for symbolization only. [Environment](environment.txt) · [Run order](run-order.csv) · [Raw samples and logs](raw-callchains.tar.gz).

Run `bash census.sh WORK/candidate > CENSUS.csv`, then `python3 summarize.py CAPTURE_DIR DEBUG_ELF CENSUS.csv RESULT_DIR`. For archived captures extract the raw files with the original `elf-path.txt` and use the matching debug twin. All 16 profiles and all 18 census cases match the staged fixture/work/pair checksums; zero warm allocations. No lost samples reported in the published captures. Cycle and IBS runs are separate; their profiled wall times are not gate evidence.

## Handoff

Screen **bounded lookahead of future rank home buckets** during ordered dense commit, against direct child initialization. Preserve dense commit/insertion order, prevalidated duplicate handling, epoch wrap/reset and bounded-probe tree fallback. Compute each home's hash once if possible; prefetch must be optional, stay within committed reservation, and not alter diagnostics, allocation or failure behavior. Test the hash and collision fallback paths before timing; avoid changing the hash or table size based on these non-adversarial fixtures. Keep the unranked path as a control and rerun G2/G9 together if the ordered full-step gain resolves.

Translated rank insertion corresponds to ~2.4–2.5 ms of the unprofiled 12.14 ms ordered build median; not all is removable. At unchanged uncertainty, the [15-round staged G9 upper bound](../g2-topology-cost-2026-11-20/frozen/g9-comparison.csv) allows only 2.2% unordered savings with ordered time fixed. Clearing the translated G2 gap at equivalent uncertainty still needs about 1 ms less ordered full-step time. G1/G2 still block 08.
