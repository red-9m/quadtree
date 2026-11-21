# 03 · Attribute the staged loader's traffic

**Gather/classify and stable scatter still take about half of unordered build cycles, but the audited loads mostly hit L1.** Skipping already-routed groups saves essentially nothing on `uniform`, the hard G2 blocker. A two-level tag model covers both blockers and warrants a timed prototype. No runtime change or new gate verdict.

## Staged Release profile

Reattribute the [16 captures from the node-locality study](../g2-node-locality-2026-11-20/README.md#staged-layout-and-profile): two reversed blocks per engine and scene, GCC 16.2.1 Release, Ryzen 9 5900HS, `int32` at 100,000. Ranges are percent of classified **build** cycles, not step cycles. Partition total includes permutation initialization, copies, and plan overhead.

| Scene / engine | Gather/classify | Stable scatter | Partition total | Dense + topology commit |
|---|---:|---:|---:|---:|
| `uniform` / `index` | 26.7–27.0% | 22.2–23.0% | 55.1–55.2% | 32.7–32.9% |
| `split-line-translated` / `index` | 28.2% | 22.6–22.8% | 55.1–55.3% | 31.7–33.5% |
| `uniform` / `index-ordered` | 12.2–13.8% | 11.7% | 27.5–27.6% | 30.0% |
| `split-line-translated` / `index-ordered` | 15.1–16.6% | 10.3–12.2% | 29.3–29.5% | 27.7–27.9% |

- Of **530–629** audited geometry demand-load samples per unordered capture, only **11–20** report a source outside L1 (1.7–3.8% of those loads, or 5.8–11.8% of all known non-L1 build loads). This is not the dominant non-L1 site reported in the [pre-lookahead narrow profile](../g2-narrow-profile-2026-11-20/README.md#unordered-build-attribution). The captures are not a matched prefetch A/B; they cannot isolate why the distribution changed.
- Audited scatter tag/ID loads hit L1 97.9–100% of the time on the unordered rows. The scatter still consumes 22–23% of build cycles. Neither the load-source samples nor the cycle shares establish a store-bandwidth bottleneck or measure removable stall time.
- Ordered rank validation and rank insertion together take about 35% of build cycles on both rows. The validator's audited helper is counted as build work even in truncated callchains.

[Cycle periods](cycle-periods.csv) · [IBS data sources by phase](ibs-data-sources.csv) · [Audited operand counts](focus-pcs.csv) · [Disassembly](load-instructions.asm). `cycles:u` attribution uses DWARF and the explicit copy-call PCs; IBS counts sampled operations, not line fills or bytes. The classifier range includes lookahead, demand gathers, geometry tests and the histogram/tag write; the scatter range includes the cursor update.

## Stable-routing census

[Untimed `planBulk` counters by depth](census.csv) on the frozen fixtures, at 10,000 and 100,000. Ordered and unordered routing counts are identical. Counts reconcile with the materialized topology, fixture hashes, pair checksums and work; all eight warm builds + queries allocate zero.

At 100,000, the current classifier makes **726,719** indexed box gathers on `uniform` and **852,530** on `split-line-translated`. The latter performs **850,050** scatter iterations. Logical stream bytes below count ID reads/writes, box demand reads, lookahead-ID loads, tag bytes and two-way ID copies. They exclude cache-line fills, prefetch line traffic, cursor/histogram accesses, plan records, and fallback-key traffic (0 and 14 prefix keys on these two rows).

| Scene | Gather/classify bytes | Scatter bytes | Own/finish copy bytes | Net logical bytes avoided by two-level tags |
|---|---:|---:|---:|---:|
| `uniform` | 17.71 MB | 6.54 MB | 0.59 MB | 7.26 MB (29.2%) |
| `split-line-translated` | 20.78 MB | 7.65 MB | 0.23 MB | 9.10 MB (31.8%) |

- **Direct own routing:** no successful split has own entries to redirect on these four fixtures. No own-copy bytes saved.
- **Skip already-routed groups:** only 10 IDs on `uniform` at 100,000 have monotone tags (0 at 10,000). On translated, 74,076 IDs are already grouped, of which 50,216 belong to three single-bin, no-stayer splits detectable from the existing histogram. Bypassing those three scatters saves at most 0.45 MB at those sites before downstream buffer-parity effects. Do not add a general monotonicity scan or advance this as a G2 fix.
- **Two-level tags:** on even depths, compute the next child tag while widths and centers are live; pack the two tags into one byte. Stable scatter writes the odd-depth tag to a second tag stream in reserved scratch, indexed by child output position. Odd-depth classification reads those tags for its histogram instead of rereading IDs and boxes. At 100,000 this could replace **326,698 / 410,583** demand gathers (**45.0% / 48.2%**) on uniform / translated. **73,302 / 29,259** predicted tags would be unused. The net-byte model charges one extra tag write per routed child and an odd-depth histogram tag read; it excludes extra child-cell setup, fit arithmetic, dispatch and any changed cache behavior. This is a traffic opportunity, not a speedup forecast or a tested algorithm.

[Exact counts and model](traffic-model.csv) · [Reconciliation](verification.log). The model counts every original split, incl. single-quadrant groups, and keeps their fallback prefix tests; it does not combine the proposed savings.

## Verification and reproduction

Reconstruct `WORK/control` and the `.text`-matched debug twin per the [node-locality report](../g2-node-locality-2026-11-20/README.md#correctness-and-reproduction). Reuse its [raw callchains](../g2-node-locality-2026-11-20/raw-callchains.tar.gz) and original `elf-path.txt`, or rerun its profiler. Source hashes, `.text` and environment are recorded there. No new performance measurements depend on the instrumented binary.

Run `bash census.sh WORK/control > CENSUS.csv` to apply [census-only counters](census.patch) in a temporary tree. Run `python3 summarize.py CAPTURE_DIR DEBUG_ELF CENSUS.csv OUTPUT_DIR` to regenerate the tables. The [15-round staged control](../g2-contiguous-pairs-2026-11-20/15-control/gates.txt) remains G2 9 pass / 1 fail / 5 inconclusive and G9 15/15.

## Handoff

Screen **two-level stable tags** against the staged loader, not a new prefetch distance or another narrower ID stream. Reuse the exact child `Cell` and split arithmetic; retain stop thresholds, stable permutation, single-quadrant prefix and sorted fallbacks, and allocation-free prepared builds. Test signed extremes, one-axis splits, topology, dense order and callback prefixes before timing. Compare both engines with the unmodified fixtures and report build and full-step effects separately.

At 100,000, `uniform` still needs roughly **1.38 ms (10.2%)** less unordered full-step time and translated needs **1.00 ms (7.3%)**, at unchanged relative uncertainty. The sampled uniform gather/classify share corresponds to only ~1.65 ms of its unprofiled build; replacing half its box loads cannot be assumed to remove half that time. Translated G9 allows only **3.8%** unordered improvement at fixed ordered time. Clearing the translated G2 gap at the same uncertainty would also need roughly **1.85 ms (3.6%)** less ordered full-step time. Retest G2/G9 together before advancing the candidate. G1/G2 still block 08.
