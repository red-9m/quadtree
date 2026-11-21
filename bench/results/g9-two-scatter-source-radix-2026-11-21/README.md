# 03 · Radix-sort the ordered source list on two-stream scatter

**Stage as the large-snapshot control, not for release.** Source radix resolves a 3.5–4.9% translated ordered full-step gain and passes G9 15/15 in both runs. G2 remains at 12/15; some small ordered rows regress. Keep the runtime unchanged.

The [source-sort profile](../g9-two-scatter-source-sort-2026-11-21/README.md) attributed 14.1–14.2% of translated ordered pair cycles and 17.6–17.7% on `uniform` to sorting the source list. This screen starts from the exact [staged two-stream control](../g2-two-scatter-2026-11-21/README.md) and adds only the [source sort patch](candidate.patch). The working-tree runtime is unchanged.

## Candidate

`orderedSelf` uses the existing partner-row radix sorter on `mOrder` for full ordered `int32` self queries of at least 4,096 sources when both existing build buffers cover the source list. The fixed large-source threshold leaves the 1,000-entry G9 scale on index sorting; it is not a tuned crossover. The helper takes an explicit order buffer (source list or partner row), caches full 64-bit ranks, and keeps the original eight histograms, constant-byte skips and stable scatters. The original index-sort fallback, row threshold and fallback, traversal, moved-set paths, cross queries and workspace reservation contract are unchanged. No rank lookahead or unordered outline. [Release callsites](source-sort-instructions.asm) show the source size and both capacity checks and the shared radix helper.

## Frozen phase and gate screen

The frozen `int32` snapshot slice is also the matched phase screen: both library engines plus unchanged baselines, nine rounds control → candidate, 15 rounds candidate → control. CPU 6 and the same cold, quiet preflight as the [repeatability retest](../g2-two-scatter-thermal-2026-11-21/README.md), with Tctl within 1 °C inside each comparison. Limits, fixtures, run estimate and error term are unchanged.

The initial 15-round control launch stopped at the immediate post-preflight Tctl check, before starting the benchmark. The [resume](frozen/resume-metadata/) retained the three completed samples and ran only the missing control (`run.sh WORK RESUME_DIR --final-control 39500`). An untimed control process warmed the already-cooled host, then the normal rolling quiet check and the same start band were applied. No timed runs were replaced. [Run order and starts](frozen/run-order.csv) · [Environment](frozen/environment.txt) · [Tctl and frequency](frozen/thermal.csv).

Counts are pass / fail / inconclusive. G0 cross-checks and G3/G4/G6 pass throughout. Each run has 68 warm allocation-free library rows; matched uncapped checksums, work and memory agree.

| Rounds | G2 control → radix | G9 control → radix | G1 control → radix |
|---:|---:|---:|---:|
| 9 | 12 / 0 / 3 → 12 / 0 / 3 | 14 / 0 / 1 → 15 / 0 / 0 | 8 / 6 / 2 → 8 / 6 / 2 |
| 15 | 12 / 0 / 3 → 12 / 0 / 3 | 14 / 0 / 1 → 15 / 0 / 0 | 8 / 7 / 1 → 8 / 8 / 0 |

15-round `index-ordered`, 100,000 entries, ms/snapshot. Phase medians and full-step estimates are separate.

| Scene | Build median | Pair median | Full step | G9 radix (≤4×) |
|---|---:|---:|---:|---:|
| `uniform` | 11.537 → 11.402 | 32.304 → 29.350 | 45.144 → 40.969 | 3.178 [3.079–3.281], pass |
| `split-line` | 11.813 → 11.807 | 38.240 → 35.936 | 51.090 → 47.922 | 3.500 [3.378–3.625], pass |
| `split-line-translated` | 11.821 → 11.940 | 37.856 → 36.114 | 50.182 → 48.448 | 3.662 [3.535–3.794], pass |
| `mixed-size` | 11.149 → 11.595 | 136.000 → 131.217 | 147.379 → 144.367 | 3.580 [3.454–3.711], pass |

- At 100,000, ordered full-step gains resolve in both comparisons for `uniform` (6.9–9.2%), `split-line` (5.3–6.2%) and translated (3.5–4.9%). Translated ratios are 0.951 [0.918–0.985] and 0.965 [0.935–0.997]. It saves 1.74–2.04 ms in pair medians and moves the G9 upper bound from 4.015× to 3.788× at nine rounds and from 3.932× to 3.794× at 15. The matched representative unordered full-step intervals include no change. `mixed-size` ordered improves at nine rounds only.
- G2 still leaves `split-line` at 10,000 and `uniform` / translated at 100,000 inconclusive in both runs. At 15 rounds the unordered `clustered` empty row changes from G1 inconclusive to fail; its matched timing interval spans no change and the verdict change does not repeat at nine rounds. Separately, ordered `clustered` at 8 and 16 and `uniform` at 16 regress by 3.8–6.4% with intervals excluding no change in both runs, below the radix threshold. The algorithm does not execute there; code generation or layout effects are not isolated. Do not use large-scene gains to waive these regressions.
- [G2 phases and work](frozen/g2-comparison.csv) · [G9 phases and ratios](frozen/g9-comparison.csv) · [Matched steps and intervals](frozen/step-change.csv) · [Gate counts](frozen/gate-counts.csv) · [Verification](frozen/verification.log) · [Raw samples and per-row verdicts](frozen/).

## Correctness and reproduction

The [source-order probe](checks/source-order.log) checks full dense permutations against an independent 64-bit rank sort and exact predicate/visitor streams, statuses and counters across 4,095/4,096/4,097 entries, ranks varying across all bytes or only high/low bytes, finite budgets, stop callbacks and build-scratch reuse. It checks allocation-free queries and distinguishes the radix branch from zero scratch and each independently one-key-short buffer. GCC and ASan/UBSan outputs match. The [long-row probe](checks/long-row.log), [dense order](checks/dense-order.log), [ordered](checks/ordered-prefix.log) and [unordered](checks/unordered-prefix.log) prefixes and [full/moved-set identities](checks/equivalence.log) match. [CTest 21/21](checks/tests.log) and [differential fuzzing](checks/fuzz.log) pass.

Reconstruct the [two-stream control](../g2-two-scatter-2026-11-21/README.md#correctness-and-reproduction) as `WORK/control` and `WORK/control-build`; copy its source (dereference symlinks) to `WORK/candidate` and apply `patch -p1 --fuzz=0 < candidate.patch`. Use the [control](../g2-two-scatter-2026-11-21/candidate-inputs.sha256) and [candidate](candidate-inputs.sha256) source checksums, GCC 16.2.1 Release, `QUADTREE_BUILD_TESTS=ON`, `QUADTREE_BUILD_BENCHMARKS=ON`, `QUADTREE_BENCH_NATIVE=OFF` and pinned `WORK/box2d` (`178495ba35695aefe9c321e569d2277f06db3b90`) as `FETCHCONTENT_SOURCE_DIR_BOX2D`. Compare [`.text` hashes](text.sha256).

Run `bash verify-source.sh WORK/control WORK/candidate` plus the [long-row probe](../g9-cached-row-sort-2026-11-20/verify-dense.sh) and [functional checks](../g2-fused-commit-2026-11-20/verify.sh). Run `bash run.sh WORK NEW_OUTPUT_DIR` and the [G2/G9 summarizer](../g2-cross-screen-2026-11-20/summarize.py) on that output directory. The runner checks the exact control ELF, uses quiet cold starts, records Tctl and CPU 6 `scaling_cur_freq`, and restores power settings.

## Handoff

**Screen same-node unordered pair qualification outlining on the staged two-stream + source-radix control.** Rebase only the [prior outline factor](../g2-self-outline-2026-11-20/README.md): default-predicate `int32` own-node qualification in `selfPairs`, not rank lookahead or cross-node traversal. The prior test saved 0.26–0.45 ms in pair medians on direct init but did not resolve full-step gains. Verify emitted calls and exact predicate/callback prefixes and work, then time both engines with quiet matched starts. Include `split-line` at 10,000 and the two 100,000-entry blockers; rerun G1/G2/G9 and retain the small ordered rows as regression checks.

At unchanged baseline times and relative uncertainty, the staged control needs about 0.31–0.39 ms less unordered `uniform` time and 0.39–0.50 ms less translated time at 100,000. Translated's worst new G9 upper bound (3.794×) permits about **5.15%** unordered savings if ordered time stays fixed. These are timing budgets, not predicted gains. G1/G2 still block 08.