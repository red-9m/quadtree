# 03 · Bulk classifier on two-stream + source radix

**Classifier work is 29.8–33.1% of sampled unordered build cycles; histogram increments alone are only 0.5–1.3%.** Most cycles in the classifier land on geometry and loop-control PCs. Interleave the two existing classification halves to test loop and gather overlap, not a histogram-only rewrite. No runtime change or gate rerun.

## Release profile

Exact staged control from the [outline screen](../g2-radix-outline-2026-11-21/README.md), without the outline. Two reversed quiet-start blocks, CPU 6, GCC 16.2.1 Release, `int32`. Twelve `cycles:u` captures cover both engines; eight AMD IBS captures cover the three unordered cases and ordered translated. Starts and settings are in [run order](run-order.csv) and [environment](environment.txt).

Ranges are percentages of **sampled unordered build cycles**. Columns show selected components; classification also includes tag stores and prefetch/control work.

| Fixture | Gather | Geometry | Histogram | Classifier loop control | Stable scatter | Dense + topology commit |
|---|---:|---:|---:|---:|---:|---:|
| `uniform` 100,000 | 5.31–5.34 | 10.96–11.53 | 0.53–0.57 | 12.31–12.36 | 16.38–16.62 | 32.23–32.65 |
| `split-line` 10,000 | 3.54–4.15 | 12.82–13.71 | 0.70–1.33 | 13.96–14.12 | 15.97–16.42 | 28.91–29.21 |
| `split-line-translated` 100,000 | 4.89–5.72 | 12.20–12.24 | 0.66–0.68 | 12.86–13.25 | 15.02–15.81 | 30.19–32.68 |

- Most loop-control periods land on the comparisons at `0x146bba` and `0x147272`, adjacent to the upper/lower histogram increments. `cycles:u` can skid: these are sampled PCs, not measured branch or histogram latency. The geometry column includes fit tests and center/quadrant computation; gather includes ID/address formation and three or four field loads, some fused with subtraction.
- IBS: 4,528/4,619 unordered classified gather LOAD samples and 983/983 labelled histogram LOAD samples report L1. Unlabelled operations are reported separately. This does not count cache-line traffic or establish that there are no dependencies. The ordered translated classifier is 17.7–19.5% of build cycles; its scatter is 9.18–9.88%. Dense commit includes existing rank insertion on ordered builds.
- [Audited operands](operands.csv) · [Cycle attribution](cycle-periods.csv) · [Per-PC periods](cycle-sites.csv) · [IBS source labels](ibs-sites.csv). All 20 profiles match frozen fixture hashes, checksums and traversal counts with zero warm allocations. No samples reported lost. [Verification](verification.log).

## Reproduction

Use the source-radix control as `WORK/control` and `WORK/control-build`, with pinned `WORK/box2d`. Run `bash profile.sh WORK CAPTURE_DIR`, then `python3 summarize.py CAPTURE_DIR RESULT_DIR`. The runner verifies the exact Release ELF and [`.text`-matched debug twin](text.sha256); only Release is profiled. For archived captures combine the [raw callchains](raw-callchains.tar.gz), [run order](run-order.csv), [original DSO path](elf-path.txt) and [hashes](text.sha256) with the verified `staged.debug` twin. Power settings restored.

## Handoff

**Screen lockstep classification of the two existing halves.** In `planBulk<int32>` at the existing two-stream threshold, classify one lower and one upper ID per iteration into their existing independent histograms; classify the possible upper tail and keep short partitions scalar. Preserve geometry, tag values, scatter and prefix logic, each half's order, and the bounded distance-16 lookahead. This tests sharing loop control and overlapping gathers; the samples do not justify claiming histogram stalls. Check exact permutation/topology/callbacks, verify the single paired loop in Release, then time both engines on the three blocker rows. Run frozen G2/G9 only if the screen warrants it.
