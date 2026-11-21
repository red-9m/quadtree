# 03 · Two-stream control repeatability

**Repeatability still below target: 184/196 (93.9%), versus ≥95%.** Both runs give G2 11 pass / 4 inconclusive and G9 12 pass / 3 inconclusive, with no hard failures in those two gates. Keep two-stream scatter staged; do not rank candidates from these timings.

No candidate or runtime change. This retests the [unchanged staged two-stream executable](../g2-two-scatter-2026-11-21/README.md) after the [rank-lookahead screen](../g2-two-scatter-rank-2026-11-21/README.md#frozen-snapshot-slice) found 87.2% and 75.0% same-binary timing agreement against earlier runs. The [calibration target](../../MANIFEST.md#error-calibration-this-hardware) is at least 95% of comparable timed rows within combined error.

## Method and reproduction

Two independent processes run the same frozen `int32` snapshot slice, 15 rounds each, followed by the benchmark's own `--diff`. The command, fixture mix, timing protocol, gate limits and executable are unchanged. `index`, `index-ordered`, and the same four reference/prototype engines run in their usual order. No pooling, replacement of timed runs, or adjusted error term.

Use the [staged two-stream source and build](../g2-two-scatter-2026-11-21/README.md#correctness-and-reproduction) as `WORK/control` and `WORK/control-build`, with the pinned Box2D checkout at `WORK/box2d`. Run `bash run.sh WORK NEW_OUTPUT_DIR` and `python3 summarize.py NEW_OUTPUT_DIR`. The runner verifies staged source and `.text` hashes, pins the benchmark to CPU 6, uses the recorded `performance` power settings, checks the HPET clock source, and restores all power settings. The 10-second preflight requires at most 5% busy time for the host, CPU 6, and CPU 7; it does not modify the bench or its limits. A CPU-0 monitor records CPU jiffies and package Tctl every five seconds during the block. The first [preflight](preflight-1/idle-check.log) aborted before timing because host activity was 5.48%; the second [preflight](frozen/idle-check.log) passed.

## Frozen snapshot slice

`--diff` compares 196 timed rows out of 204; the other eight are capped before timing in both runs. Of 12 resolved changes on the same binary, four are faster and eight slower; the geometric mean ratio is 1.004. Ordered `split-line` at 100,000 is 9.6% slower in run 2 (ratio 1.096 [1.041–1.154]); `grid` on the same fixture is 16.7% slower. The translated and `uniform` library full-step intervals at 100,000 overlap no change.

Counts are pass / fail / inconclusive. G0 cross-checks and G3/G4/G6 pass in both processes; each run has 68 warm allocation-free library rows. Uncapped pair checksums, work, fixture hashes and footprint match the staged reference.

| Run | G1 | G2 | G9 |
|---|---:|---:|---:|
| 1 | 8 / 7 / 1 | 11 / 0 / 4 | 12 / 0 / 3 |
| 2 | 8 / 6 / 2 | 11 / 0 / 4 | 12 / 0 / 3 |

Selected frozen ratios at 100,000 entries (95% intervals). All rows below are inconclusive in both runs.

| Gate and scene | Run 1 | Run 2 |
|---|---:|---:|
| G2 `uniform` (≤2×) | 2.044 [1.959–2.133] | 2.014 [1.918–2.116] |
| G2 `split-line-translated` (≤2×) | 1.925 [1.838–2.016] | 1.959 [1.871–2.050] |
| G9 `split-line` (≤4×) | 3.882 [3.693–4.082] | 4.027 [3.805–4.262] |
| G9 `split-line-translated` (≤4×) | 3.967 [3.829–4.109] | 3.937 [3.791–4.090] |
| G9 `mixed-size` (≤4×) | 3.961 [3.783–4.147] | 3.960 [3.748–4.183] |

- [Host samples](frozen/load.csv): CPU 7 averaged 0.90% busy (sampled peak 6.32%); host activity averaged 9.27% including the benchmark on CPU 6. Package Tctl started at 37.2 °C and reached 71.9 °C; the runs were back-to-back, not matched for starting temperature. These observations do not identify the cause of timing drift.
- [Repeatability diff](frozen/repeatability-diff.txt) · [G2 phases, work and ratios](frozen/g2-comparison.csv) · [G9 phases, work and ratios](frozen/g9-comparison.csv) · [Gate counts](frozen/gate-counts.csv) · [Verification](frozen/verification.log). [Environment](frozen/environment.txt), [run order](frozen/run-order.csv), and raw samples in [frozen/1](frozen/1/) and [frozen/2](frozen/2/). Power settings restored.

## Handoff

**Timing is still a blocker.** Repeat the same frozen control pair on a quiet or dedicated host with matched starting thermal state. Record package temperature and CPU frequency across both runs; if a warm-up is needed, apply it before each process and exclude it from the timed slice. Keep CPU 6 affinity and the recorded power settings on this host. Require ≥95% `--diff` agreement before using the data to rank candidates, then check G2 and G9 together. Do not change runtime code, the candidate stack, gate limits, or the error term based on these runs. G1/G2 still block 08.
