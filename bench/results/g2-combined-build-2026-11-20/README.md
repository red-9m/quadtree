# 03 · Representative snapshots — combine the staged build changes

**Not retained.** Bulk-commit prefetch plus the integer `makeKey` hoist cut full-step time 17.5–24.0% on the three failing 100,000-entry rows in the 15-round repeat. `clustered` 100,000 passes G2 in both repeats, but six G2 failures remain and G9 gains a repeatable failure at `split-line-translated` 100,000. Runtime source is unchanged.

## Candidate

Applied the staged [integer key hoist](../g2-integer-key-2026-11-19/hoist.patch) and [bounded descriptor/payload prefetch](../g2-entry-prefetch-2026-11-19/prefetch.patch) unchanged to a separate retained-source copy. No `crossPairs` patch. The [input manifest](candidate-inputs.sha256) differs from control only in `detail/quadtree-index-impl.h`. The rebuilt control matches the original unpadded Release [`.text` hash](text.sha256).

## Frozen full 01 snapshot slice

Counts are pass / fail / inconclusive.

| Rounds | G2 control | G2 combined | G1 control | G1 combined | G9 control | G9 combined |
|---:|---:|---:|---:|---:|---:|---:|
| 9 | [8 / 6 / 1](9-control/gates.txt) | [9 / 6 / 0](9-candidate/gates.txt) | 10 / 6 / 0 | 8 / 6 / 2 | 14 / 1 / 0 | 11 / 3 / 1 |
| 15 | [7 / 6 / 2](15-control/gates.txt) | [9 / 6 / 0](15-candidate/gates.txt) | 8 / 6 / 2 | 8 / 6 / 2 | 14 / 1 / 0 | 10 / 2 / 3 |

15-round repeat, 100,000 entries, control → combined (ms/snapshot):

| Scene | Step estimate | Build median | Pairs median | Combined G2 ratio to grid |
|---|---:|---:|---:|---:|
| `uniform` | 20.581 → 16.984 | 13.291 → 9.865 | 7.167 → 7.027 | 2.633 [2.549–2.719] fail |
| `split-line` | 24.006 → 18.562 | 16.355 → 11.221 | 7.501 → 7.128 | 2.567 [2.465–2.672] fail |
| `split-line-translated` | 24.072 → 18.285 | 16.479 → 11.248 | 7.182 → 6.812 | 2.770 [2.679–2.865] fail |
| `clustered` | 29.824 → 25.535 | 14.914 → 11.044 | 14.725 → 14.480 | 1.709 [1.617–1.805] pass |

All six failing rows resolve a full-step gain in both repeats. In the 15-round run, the three failing 10,000-entry rows are 11.2–13.7% faster; at 100,000, build medians fall 25.8–31.7% on the three failing rows. Combined ratios on the six failures are still 2.485–2.770× grid, requiring roughly another 20–28% of full-step time at the point estimates just to reach 2×. The upper interval bound must also pass. The 15-round control additionally has an inconclusive `split-line-translated` 1,000 row; it passes in the combined run and in both 9-round runs.

[All G2 verdicts, phases, work and allocations](g2-comparison.csv) · [Matched step changes](step-change.csv) · warm samples in the four run directories. Phase medians are descriptive. Both binaries used CPU 6 and `rdtsc` calibrated at 3.29376–3.29377 GHz with the `hpet` kernel clock source after reboot, not the prior session's `tsc` clock source. Compare only the matched runs above. The earlier power settings were [restored](restored-mode.txt) after timing.

## Regression gates and correctness

- G0/G3/G6 pass throughout. G4 passes on the candidate; the 15-round control's `split-line-translated` 1,000 timing is inconclusive.
- G1's six hard failures are unchanged. Both candidate empty rows are inconclusive in both repeats; no new hard G1 failure.
- G9 `mixed-size` 100,000 remains a pre-existing failure. `split-line-translated` 100,000 now fails in both repeats (4.249× and 4.208× vs 4×). `split-line` 100,000 fails at 9 rounds and is inconclusive at 15. `uniform` 100,000 is inconclusive in both; `clustered` 100,000 is also inconclusive at 15. On the newly failing split-line rows, absolute ordered step time also improved, but not as much as unordered. That does not waive G9.
- [Key probe](integer-key-equivalence.log) and [entry probe](entry-prefetch-equivalence.log) match exact keys, topology, dense entries, full and moved-set callbacks, counters and handle mappings. [Full-slice verification](verification.log) matches uncapped fixtures, work, checksums and footprint; all 272 library rows allocate zero when warm. [All 20 CTest checks](tests.log) pass.

**Handoff:** pause task 03. Keep both patches staged as evidence, not as defaults. To resume, identify a change large enough to close the remaining full-step gap and address ordered-query cost before retaining the combination; use the same frozen G2/G9 gates. Task 04 can proceed independently. Release acceptance stays blocked.

## Reproduction

Reconstruct the [retained inputs](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction). From the repository root, apply the two saved patches to a separate source copy, verify [patch hashes](patches.sha256) and [candidate input hashes](candidate-inputs.sha256), then build both with GCC 16.2.1 Release and Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. Verify [`.text` hashes](text.sha256) with `objcopy --dump-section .text=NAME.text`. Set the platform profile and all CPU energy preferences to `performance` for timing, then restore them.

```sh
bash bench/results/g2-integer-key-2026-11-19/verify.sh /path/to/control-source /path/to/candidate-source
bash bench/results/g2-entry-prefetch-2026-11-19/verify.sh /path/to/control-source /path/to/candidate-source
bash bench/results/g2-integer-key-2026-11-19/repeat.sh /path/to/control-bench /path/to/candidate-bench /fresh/outdir
python3 bench/results/g2-integer-key-2026-11-19/summarize.py /fresh/outdir
ctest --test-dir /path/to/candidate-build --output-on-failure
```

[Environment](environment.txt) · [Run order](run-order.csv).
