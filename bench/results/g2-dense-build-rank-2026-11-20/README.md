# 03 · Dense bulk build and rank sorting

**Not retained.** Six G2 rows still fail after two repeats. The candidate closes the `clustered` 100,000-entry inconclusive result and avoids the previous combined patch's new G9 failures, but it does not close the full-step gap. Runtime source is unchanged.

## Candidate

Staged one [combined patch](candidate.patch) against `57d30dcc49cff7de5563f46625b385cc145861cb`; no harness or workload changes. It combines unsigned, root-relative integer Morton descent, linear materialization of bulk references and node summaries, the prior [descriptor/payload prefetch](../g2-entry-prefetch-2026-11-19/prefetch.patch), and allocation-free rank introsort for build validation and ordered queries. The floating-point and mutation key paths are unchanged. Rank sorting uses bounded recursion and a heap fallback; it does not change traversal, budget charges, or output order.

## Frozen snapshot slice

Counts are pass / fail / inconclusive. G0, G3, G4 and G6 passed in both repeats. Each run covered the same 204 rows, including 68 uncapped library rows with zero warm allocations.

| Rounds | G2 control → candidate | G1 control → candidate | G9 control → candidate |
|---:|---:|---:|---:|
| 9 | 8 / 6 / 1 → 9 / 6 / 0 | 8 / 6 / 2 → 8 / 8 / 0 | 14 / 1 / 0 → 14 / 1 / 0 |
| 15 | 8 / 6 / 1 → 9 / 6 / 0 | 8 / 6 / 2 → 8 / 6 / 2 | 14 / 1 / 0 → 14 / 1 / 0 |

15-round repeat, `index`, milliseconds per snapshot. Steps are full-step estimates; build and pairs are phase medians.

| Scene | Entries | Step before → after | Build before → after | Pairs before → after | G2 after (limit 2×) |
|---|---:|---:|---:|---:|---:|
| `uniform` | 10,000 | 1.603 → 1.492 | 0.940 → 0.820 | 0.659 → 0.669 | 2.562 [2.488–2.638] |
| `split-line` | 10,000 | 1.756 → 1.622 | 1.042 → 0.883 | 0.709 → 0.730 | 2.662 [2.579–2.748] |
| `split-line-translated` | 10,000 | 1.745 → 1.616 | 1.053 → 0.910 | 0.687 → 0.703 | 2.726 [2.645–2.808] |
| `uniform` | 100,000 | 21.302 → 17.542 | 14.441 → 10.145 | 7.182 → 7.315 | 2.717 [2.628–2.809] |
| `split-line` | 100,000 | 21.940 → 18.836 | 14.731 → 11.444 | 7.171 → 7.319 | 2.685 [2.598–2.774] |
| `split-line-translated` | 100,000 | 21.615 → 18.431 | 14.704 → 11.469 | 6.818 → 6.940 | 2.840 [2.756–2.927] |
| `clustered` | 100,000 | 27.706 → 24.205 | 13.382 → 10.194 | 14.275 → 13.952 | 1.733 [1.683–1.784], pass |

- Full-step estimates improved 7.0–7.6% at 10,000 and 14.1–17.6% at 100,000 on the six failing rows. Unordered pair medians did not improve on them. The remaining gap is 22–30% of candidate full-step time at the point estimates; passing requires the upper interval bound to meet the limit.
- At 100,000, ordered full steps improved 13.6–20.9% on the three failing G2 scenes. G9's pre-existing `mixed-size` failure fell from 4.461× to 4.200× [4.082–4.322]; it still fails. Both G1 empty rows failed in the 9-round candidate run and stayed inconclusive in the 15-round repeat.
- [Per-row verdicts, work, allocations and phase times](g2-comparison.csv) · [Matched step and phase changes](step-change.csv) · [9-round control](9-control/gates.txt) / [candidate](9-candidate/gates.txt) · [15-round control](15-control/gates.txt) / [candidate](15-candidate/gates.txt).
- [Exact comparison](equivalence.log) matches integer boundary/extreme keys, fixture topology, dense IDs, and complete ordered/unordered callback streams. [Full-slice verification](verification.log) matches uncapped fixtures, pair counts, checksums, work, allocations and footprint. [CTest: 21/21](tests.log).

## Reproduction

Extract the named commit to two source directories, excluding `bench/results` and `docs`. Apply `candidate.patch` only to the candidate (`patch -p1`). Verify [control inputs](control-inputs.sha256), [candidate inputs](candidate-inputs.sha256) and the compiled [`.text` hashes](text.sha256). Use Release (`-O3 -DNDEBUG`), GCC 16.2.1, and Box2D `178495ba35695aefe9c321e569d2277f06db3b90` via `FETCHCONTENT_SOURCE_DIR_BOX2D`. Run `bash verify.sh /path/to/control-source /path/to/candidate-source` and the candidate CTest suite.

Use the [frozen full-slice runner](../g1-flat-input-2026-11-19/reproduce.sh) with `taskset -c 6` and its `full` argument. Set platform profile and CPU energy preferences to `performance` during timing and restore them afterward. This run used `rdtsc` with the kernel `tsc` clock source, so compare the matched runs here, not absolute times against the earlier `hpet` run. Repeat in [recorded order](run-order.csv); gzip each `snapshot-raw.csv`, then run the [summarizer](../g2-integer-key-2026-11-19/summarize.py) on the parent directory. [Environment](environment.txt) · [Restored profile](restored-mode.txt).

**Handoff:** pause 03. Do not retune the prefetch distances or rank sort to chase a 22–30% full-step gap. Resume with a design that removes build passes (key generation/sort/materialization) or substantial traversal work, and measure the remaining ordered `mixed-size` cost separately. Keep G2 and G9 unchanged. 08 stays blocked.
