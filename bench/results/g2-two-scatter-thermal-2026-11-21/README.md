# 03 · Two-stream control at matched starting temperature

**Repeatability meets the target: 189/196 timed rows (96.4%), including all 68 library rows, agree within error.** G2 still has three inconclusive rows in each run; translated G9 at 100,000 passes in only one. Use the matched-start protocol for the next screen; keep two-stream scatter staged.

This repeats the [same staged two-stream executable](../g2-two-scatter-2026-11-21/README.md) after a [back-to-back retest](../g2-two-scatter-repeatability-2026-11-21/README.md) reached 93.9% same-binary timing agreement, below the [95% target](../../MANIFEST.md#error-calibration-this-hardware). No candidate or runtime changes.

## Method and reproduction

Run the same frozen `int32` snapshot slice twice in independent processes, 15 rounds each, on CPU 6. Keep the executable, command, engines, fixtures, error term and gate limits unchanged. Use a cold, quiet start for both: Tctl ≤40 °C, run 2 within 1 °C of run 1, and at most 5% busy over a rolling 10-second preflight on the host, CPU 6 and SMT sibling CPU 7. Wait up to 300 s per start. No warm-up or changed fan settings; the cooldown is untimed. The monitor on CPU 0 records package Tctl, CPU 6 `scaling_cur_freq`, and CPU jiffies at five-second intervals; the launcher takes separate before/after snapshots for each run. Frequency samples are cpufreq readings, not an effective-frequency measurement.

Use the [staged source and build](../g2-two-scatter-2026-11-21/README.md#correctness-and-reproduction) as `WORK/control` and `WORK/control-build`, with the pinned Box2D checkout at `WORK/box2d`. Run the repeatability report's [runner](../g2-two-scatter-repeatability-2026-11-21/run.sh) with `WORK NEW_OUTPUT_DIR --matched-temperature`, then its [summarizer](../g2-two-scatter-repeatability-2026-11-21/summarize.py) with `NEW_OUTPUT_DIR`. The runner checks source and `.text` hashes, preserves the recorded power/clock setup, and restores power settings. The [quiet-start helper](../g2-two-scatter-repeatability-2026-11-21/quiet-start.py) logs the preflight separately from timing.

## Frozen snapshot slice

Run starts were 32.750 and 33.750 °C, with 204 s of untimed cooldown. Before launch, host/CPU 6/CPU 7 busy time was 0.43% / 0.40% / 0.00% and 0.63% / 0.70% / 0.00%, respectively. Package Tctl peaks were 68.1 and 67.6 °C. CPU 6 sampled frequency ranges were 4,577–4,642 and 4,579–4,642 MHz. CPU 7 averaged 0.31% and 0.00% busy during the runs.

The seven resolved timing changes are all in reference/prototype engines; the `--diff` geometric mean is 0.999. Compared with the back-to-back retest (93.9%), this pair meets the prespecified coverage target. Both starting temperature and background load differed between retests, so this is a repeatability result under the recorded conditions, not evidence that temperature caused the earlier drift.

Counts are pass / fail / inconclusive. G0 cross-checks and G3/G4/G6 pass; each run has 68 warm allocation-free library rows. Fixture hashes, uncapped checksums, work and footprint agree with the staged reference.

| Run | G1 | G2 | G9 |
|---|---:|---:|---:|
| 1 | 8 / 7 / 1 | 12 / 0 / 3 | 14 / 0 / 1 |
| 2 | 8 / 6 / 2 | 12 / 0 / 3 | 15 / 0 / 0 |

| Gate and scene | Entries | Run 1 ratio [95% interval] | Run 2 ratio [95% interval] |
|---|---:|---:|---:|
| G2 `split-line` (≤2×) | 10,000 | 1.969 [1.911–2.029] | 1.943 [1.879–2.009] |
| G2 `uniform` (≤2×) | 100,000 | 2.004 [1.944–2.065] | 2.000 [1.944–2.059] |
| G2 `split-line-translated` (≤2×) | 100,000 | 1.988 [1.927–2.050] | 1.994 [1.933–2.056] |
| G9 `split-line-translated` (≤4×) | 100,000 | 3.883 [3.760–4.011] | 3.830 [3.720–3.942] |

- [G2 phases, work and ratios](frozen/g2-comparison.csv) · [G9 phases, work and ratios](frozen/g9-comparison.csv) · [Gate counts](frozen/gate-counts.csv) · [Repeatability diff](frozen/repeatability-diff.txt) · [Verification](frozen/verification.log).
- [Run boundaries](frozen/run-boundaries.csv) · [Preflights](frozen/quiet-start-1.csv) and [cooldown](frozen/quiet-start-2.csv) · [Temperature, frequency and load](frozen/load.csv) · [Environment](frozen/environment.txt) · [Run order](frozen/run-order.csv) · Raw samples and gate logs: [run 1](frozen/1/), [run 2](frozen/2/). Power settings restored.

## Handoff

**Profile ordered source-rank sorting on the staged two-stream control.** At 100,000 `int32` entries on `uniform` and `split-line-translated`, separate the `orderedSelf` source sort from partner-row sorting and the ordered walk. Use the hash-checked Release executable and a `.text`-matched symbol twin; check an unordered run as a control and match fixture hashes, checksums, work and zero warm allocations. An [older grouped-screen profile](../g9-order-sort-profile-2026-11-20/README.md#release-profile) put translated source sorting at 12.5–12.7% of ordered pair cycles; verify that opportunity on this executable before changing sorting or scratch use. Do not stack rank lookahead or change gates.

At unchanged relative uncertainty and baseline times, the two runs imply 0.37–0.41 ms less unordered `uniform` time and 0.32–0.35 ms less unordered translated time to clear G2 at 100,000. The translated gain would also need 0.65–1.37 ms less ordered time to clear G9 in both runs' conditions. These are timing budgets, not predicted savings. Any later candidate must retest G2 and G9 jointly; G1/G2 still block 08.
