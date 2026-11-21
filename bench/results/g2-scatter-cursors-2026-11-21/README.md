# 03 · Stable-scatter cursor dependencies

**The cursor update dominates sampled scatter cycles.** In the staged `planBulk<int32>`, 93–94% of unordered scatter cycle periods land on the cursor load–increment–store block, while 1,153/1,154 sampled cursor loads hit L1. At 100,000 entries adjacent IDs reuse the same cursor 25% (`uniform`) and 35% (`split-line-translated`) of the time. This supports screening independent cursor streams, not a claim of measured store-forwarding stalls. No runtime change or gate rerun.

## Direct-init Release profile

Fresh captures from the rebuilt, hash-checked direct-child-init control. Ryzen 9 5900HS, GCC 16.2.1 Release, `int32`, 100,000 entries. Two reversed blocks, separate `cycles:u` and AMD IBS processes for each engine and fixture. Times below are **shares of sampled build cycles**, except the last column.

| Scene / engine | Scatter loop | Cursor update | Destination store | Tag + ID loads | Cursor % of scatter |
|---|---:|---:|---:|---:|---:|
| `uniform` / `index` | 23.1–24.1% | 21.7–22.6% | 1.2% | 0.09–0.12% | 93.9–94.1% |
| `split-line-translated` / `index` | 23.2–23.4% | 21.6–22.0% | 1.2–1.4% | 0.09–0.10% | 93.0–94.1% |
| `uniform` / `index-ordered` | 10.7–11.2% | 10.2–10.3% | 0.4–0.8% | 0.04–0.05% | 92.0–95.4% |
| `split-line-translated` / `index-ordered` | 11.2–12.5% | 10.6–11.8% | 0.5–0.6% | 0–0.02% | 93.8–94.7% |

- `dst[next[tags[i]]++] = src[i]` (`detail/quadtree-index-impl.h:1666` on the staged source) lowers to a cursor load at `0x145f0d`, increment at `0x145f14` and update to the same indexed stack word at `0x145f1c`. Roughly 76–79% of unordered scatter cycle periods land at that last PC, ~15–18% at the increment. Periods can skid: the distribution locates the hot block, not the latency of its individual instructions.
- IBS confirms the tag and source-ID loads also mostly hit L1 (2,301/2,336 unordered samples). All 1,175 unordered cursor stores are labelled L1; destination stores are 1,093 L1 / 36 L2 / 1 unknown. These are sampled operations, not writebacks, cache-line fills, or a bandwidth measurement. Near-term reuse can create a store-to-load dependency even on L1 hits; this profile does not count forwarding or stalls.

[Disassembly](scatter-instructions.asm) · [Operands](operands.csv) · [Cycle periods](cycle-periods.csv) · [Per-PC cycles](cycle-sites.csv) · [IBS load/store labels](ibs-sites.csv).

## Untimed reuse census

Counted actual tag stream order inside each successfully split partition. Adjacent-pair percentages exclude partition boundaries; the recent-4 column counts all scatter IDs whose previous use of the same cursor is within the last four IDs of that partition. Ordered and unordered tag/cursor counts agree exactly by depth.

| Scene | Entries | Scatter IDs | Same adjacent, observed / shuffled¹ | Same adjacent without single-bin groups | Same cursor within four IDs |
|---|---:|---:|---:|---:|---:|
| `uniform` | 10,000 | 57,626 | 25.01% / 24.99% | 25.01% | 65.81% |
| `uniform` | 100,000 | 726,690 | 24.94% / 25.00% | 24.94% | 66.68% |
| `split-line-translated` | 10,000 | 67,718 | 37.05% / 36.74% | 32.00% | 71.28% |
| `split-line-translated` | 100,000 | 850,050 | 35.42% / 35.40% | 31.31% | 71.94% |

¹ Expected adjacency if tags are shuffled independently within each partition, preserving each tag histogram. Near agreement means the short reuse is largely explained by occupancy, not more clustering than expected from those histograms.

- For 100,000 `uniform`, per-depth adjacency stays between 24.58% and 25.09%, with no single-bin splits and a longest same-tag run of 11. On translated, depth 1 routes almost every item to the same cursor as its predecessor; 50,206 of those IDs are in single-bin groups. Outside depth 1, same-cursor adjacency starts near 25% and rises to 45% near the deepest splits. Dropping all single-bin groups still leaves 31.31% at 100,000, so a single-bin fast path does not address most short reuse or the uniform blocker.
- Nearly all IDs can be paired across the two halves of their partition (99.37% uniform and 99.42% translated at 100,000). Among these pairs, 24.90% and 35.38% have equal tags. Separate cursors per half would let those two placements proceed independently even when their tags match, provided each half gets its own stable output slice per tag. This is a census of coverage, not a measurement of two-stream performance.

[Raw counts](census.csv) · [Per-depth summary](census-by-depth.csv) · [Aggregate counts](census-summary.csv) · [Verification](verification.log).

## Reproduction

Run `bash reconstruct.sh WORK` with a new path to restore the staged source from `57d30dcc` and the recorded patch chain. It verifies [staged source hashes](../g2-topology-cost-2026-11-20/candidate-inputs.sha256), fetches the pinned Box2D (or accepts `BOX2D_SOURCE` at the same revision), and builds the Release executable. The reconstruction was used after the original temporary build disappeared; the rebuilt [`.text` hash and layout](text.sha256) match the previous staged ELF.

Run `bash profile.sh WORK NEW_PROFILE_DIR`; it pins CPU 6, keeps a [.text-layout-matched](text.layout) debug twin for symbolization only, and restores [platform and CPU settings](power-after.txt). [Environment](environment.txt) · [Run order](run-order.csv) · [Raw callchains and logs](raw-callchains.tar.gz). Then run `bash census.sh WORK/candidate > CENSUS.csv` and `python3 summarize.py CAPTURE_DIR DEBUG_ELF CENSUS.csv RESULT_DIR`. For archived captures keep their recorded `elf-path.txt`. The 16 profiles and eight census fixtures match frozen pair checksums and work, with zero warm allocations. The census patch runs only in a temporary, untimed build. No samples were reported lost in the published captures.

## Handoff

**Screen two independent, stable cursor streams on direct init.** While classifying, retain the first-half histogram; derive per-tag cursors for the two contiguous halves, then interleave one scatter from each. Each half retains its original visitation order, and the first half's output precedes the second within every tag. Leave short tails, prefix checks, scratch reservations and fallbacks intact. Isolate this factor: do not add a single-bin shortcut, change lookahead distance, or stack the rank and outline patches. Confirm distinct cursor updates in the Release ELF, verify exact topology/dense slots/callback prefixes, and time both engines.

The measured cursor shares correspond to ~1.28–1.39 ms of unprofiled unordered build time, not all removable. Clearing the [latest matched G2 upper bounds](../g9-home-lookahead-2026-11-20/frozen/g2-comparison.csv) at unchanged relative uncertainty still needs ~0.81 ms less `uniform` and ~0.76 ms less translated full-step time. The [translated G9 upper bound](../g9-home-lookahead-2026-11-20/frozen/g9-comparison.csv) permits only 1.2% unordered savings at fixed ordered time; clearing the translated G2 gap would need ~2.29 ms less ordered time too. The entire translated ordered scatter loop corresponds to only ~1.36–1.53 ms, so scatter alone is unlikely to close both gaps. Rerun G2/G9 together for a resolved candidate; G1/G2 still block 08.
