# 03 · Snapshot build traffic on the staged radix control

**Target the classifier's geometry gather, not just narrower IDs and tags.** `planBulk` takes 61–65% of sampled unordered build cycles on the two 100,000-entry blockers. Four loads of `Descriptor<int32>::box` account for 75–82% of known-source build load samples reported outside L1. No runtime patch or gate verdict changed.

## Attribution

Two independent process captures per event, engine and scene on the [staged radix Release build](../g9-radix-row-sort-2026-11-20/README.md). All rows are `int32`, 100,000 entries. Ranges across captures, percent of sampled **build** cycles:

| Scene | Engine | Split classify | Split scatter | Partition total | Dense commit | Topology | Rank validation + insert |
|---|---|---:|---:|---:|---:|---:|---:|
| `uniform` | `index` | 38.8–40.5 | 17.9–19.3 | 63.7–63.9 | 13.0 | 13.1–13.3 | — |
| `split-line-translated` | `index` | 34.2–39.8 | 19.9–20.7 | 61.0–65.2 | 12.0–13.6 | 13.2–14.0 | — |
| `uniform` | `index-ordered` | 15.4–15.7 | 9.9–10.2 | 27.5–28.0 | 15.8–16.4 | 6.4–6.7 | 42.7–43.1 |
| `split-line-translated` | `index-ordered` | 17.0–17.1 | 9.7–10.4 | 29.5–29.7 | 15.3–15.8 | 7.1 | 41.5 |

Partition total includes ID initialization, finish copies, split control and plan records in addition to classification and scatter. The 15-round unprofiled control puts unordered build medians at 7.545 ms and 8.318 ms respectively, versus full steps of 14.986 ms and 15.203 ms. Use these timings, not profile timings, to size the remaining gap.

- **Geometry gather:** the four hottest load PCs (`0x145911`–`0x14591b`) read the box out of a 56-byte descriptor selected by an ID from the current permutation. They account for 287–298 of 352–368 unordered non-L1 build load samples for `uniform` and 354 of 464–473 for `split-line-translated`. The [instruction window](hot-loop.asm) distinguishes these loads from the preceding source-ID load (`0x145905`) and the [scatter's tag/ID accesses](focus-pcs.csv).
- **Scatter and materialization:** unordered scatter accounts for 1–9% of non-L1 build load samples, even though it accounts for 18–21% of build cycles. Topology accounts for 25–28% of sampled build stores but only 13–14% of build cycles. The current data do not establish that scatter or topology is limited by write bandwidth.
- **Ordered headroom:** the same geometry loads dominate ordered non-L1 build load samples (57–67%). Rank duplicate validation is 28–29% of ordered build cycles; rank insertion is about 14%. They are separate costs, not partner-row sorting.

`cycles:u` attribution sums sampled periods by DWARF source range; source attribution is approximate. AMD IBS dispatch-mode samples report individual load/store ops and data-source labels, not transactions, bytes, stalls, or removable time. “Outside L1” counts loads with a known source other than `L1 hit`; the four loads can touch the same cache line. The two events use separate processes. These are profiles, not gate timings or a prediction of speedup.

[Phase attribution](attribution.csv) · [Sample totals](run-totals.csv) · [Cycle periods](cycle-periods.csv) · [IBS source labels](ibs-data-sources.csv) · [Focus PCs](focus-pcs.csv) · [Per-PC breakdown](sites.csv.gz) · [Raw callchains and run logs](raw-callchains.tar.gz).

## Verification and reproduction

All 16 captures match the [15-round staged control](../g2-cross-screen-2026-11-20/15-control/snapshot.csv) on fixture hashes, pairs, checksums, work, footprint and cold allocations. All have zero warm allocations; no `PERF_RECORD_LOST` in the published captures. [Verification](verification.log) · [Profile rows](profile-rows.csv) · [Run order](run-order.csv) · [Environment](environment.txt) · [Restored platform profile](restored-mode.txt).

Reconstruct `WORK/candidate` and `WORK/candidate-build` for the radix control using the [recorded source hashes and build](../g9-radix-row-sort-2026-11-20/README.md#correctness-and-reproduction). Build the symbolization twin with the [radix profiler's debug compile command](../g9-radix-row-sort-2026-11-20/profile.sh). Run `bash profile.sh WORK DEBUG_ELF NEW_CAPTURE_DIR` on an idle Ryzen with `ibs_op` and CPU 6 available. The script verifies [`.text` hash](text.sha256) and [layout](text.layout), records the actual Release ELF, and uses the debug twin only for symbolization. Run `python3 summarize.py CAPTURE_DIR DEBUG_ELF OUTPUT_DIR`; for the archive, extract the raw callchains with their recorded `elf-path.txt` intact.

## Handoff

Prototype a compact, separate classifier input for `planBulk` indexed by entry ID: either contiguous `Box<int32>` or 16-byte precomputed centers and unsigned spans. Keep the single-quadrant prefix test, stable dense order, and fallback identical. Measure that change before combining it with narrow permutation/tag streams; the sampled bottleneck is the 56-byte-stride descriptor gather, not tag loads. Preserve transactional build behavior and prepared allocation guarantees.

Size the next change against about **2.4–2.6 ms** lower unordered full-step time to clear the observed G2 upper bounds at unchanged uncertainty. That is roughly half of partition time if all savings come from partition; even eliminating scatter alone would not close the gap using these phase shares. For `split-line-translated`, G9 would require about 14% lower ordered time if unordered drops 17% and uncertainty stays fixed. Profile or optimize ranked build work alongside the partition change, then retest G2 and G9 together. Release acceptance remains blocked.
