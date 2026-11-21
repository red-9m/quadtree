# 03 · 45-round frozen confirmation of packed cross-item eligibility

**G2 and G9 pass 15/15 in the 45-round run, replicating the nine-round pass.** Stage packed cross-item on top of two-stream + source radix for the measured slice. G1 remains a release blocker; no runtime patch shipped.

This is the [prespecified confirmation](../g2-translated-uncertainty-2026-11-21/README.md) of the exact [packed cross-item candidate](../g2-packed-cross-item-2026-11-21/README.md). The earlier nine-round candidate passed G2/G9; the 15-round candidate was inconclusive on translated 100,000 for both. Those runs and all their samples remain part of the record.

## Reproduction

Use the [same candidate work tree and builds](../g2-packed-cross-item-2026-11-21/README.md#checks-and-reproduction), then `bash ../g2-packed-cross-item-2026-11-21/run.sh WORK OUT --confirm-45` and `python3 verify.py OUT`. This is one fresh process of the entire frozen `int32` snapshot slice including G1 and all reference engines; `--confirm-45` changes only the round count and selects the candidate. There is no new matched control run, code change, threshold change, different fixture, pooling, truncated run or adjustment to the 2% process error. Before/after implications use the previous 9/15 matched screen.

Staged and candidate input/ELF `.text` hashes are checked by the runner. CPU 6 is pinned and power restored. [Environment](frozen/environment.txt) · [Run order](frozen/run-order.csv) · [Quiet preflight](frozen/quiet-start-45-candidate.csv) · [Temperature/frequency](frozen/thermal.csv). [Verification](frozen/verification.log) checks all fixture IDs, uncapped pair checksums and work, sample counts and memory against the previous frozen candidate.

## Results

| Candidate run | G1 pass / fail / inconclusive | G2 | G9 |
|---:|---:|---:|---:|
| 9 rounds | 8 / 6 / 2 | 15 / 0 / 0 | 15 / 0 / 0 |
| 15 rounds | 8 / 6 / 2 | 14 / 0 / 1 | 14 / 0 / 1 |
| 45 rounds | 8 / 8 / 0 | 15 / 0 / 0 | 15 / 0 / 0 |

45-round results for the previously unresolved G2 rows and translated G9. Phase medians are independent of the full-step estimator; times in ms.

| Gate / fixture | Build median | Pair median | Full step | Ratio [95% interval] |
|---|---:|---:|---:|---:|
| G2 `split-line` 10,000 | 0.435 | 0.685 | 1.125 | 1.896 [1.843–1.952] |
| G2 `uniform` 100,000 | 5.424 | 6.721 | 12.261 | 1.901 [1.845–1.958] |
| G2 `split-line-translated` 100,000 | 5.897 | 6.653 | 12.670 | 1.925 [1.860–1.991] |
| G9 `split-line-translated` 100,000 (ordered phases) | 11.674 | 35.862 | 47.838 | 3.776 [3.665–3.891] |

The translated G2 upper bound is only 0.009× below the limit. This confirms the gate in a new process without claiming headroom outside this slice. The confirmation started at 31.750 °C after quiet preflight. G0 cross-checks passed; G3/G4/G6 passed. All 68 warm library rows allocate zero. G1 failures are 0, 1, 2 and 4 in both scenes; the two empty rows were inconclusive in both earlier candidate runs. This is not evidence of a new code regression without a fresh matched control.

[G2 phases and work](frozen/g2-comparison.csv) · [G9 phases and intervals](frozen/g9-comparison.csv) · [Gate counts](frozen/gate-counts.csv) · [Full verdicts](frozen/45-candidate/gates.txt) · [Raw samples](frozen/45-candidate/snapshot-raw.csv.gz). [Exact paired before/after evidence](../g2-packed-cross-item-2026-11-21/README.md#frozen-g1g2g9-screen).

## Handoff

**Return to [02: small snapshots](../../../docs/plan/02-g1-small-snapshots.md) on the new exact staged ELF.** Refresh the 0/1/2/4-entry `uniform` and `clustered` attribution before choosing another change: isolate adapter preparation, replacement, and direct pair dispatch on the real build-plus-pairs path and confirm the empty-row variability. Use the [earlier singleton result](../g1-singleton-query-2026-11-19/README.md) to avoid repeating known inlining probes. Keep the adapter, brute baseline and G1 limits unchanged. Task 03's measured `int32` slice is closed; broader release checks belong to 08, which remains blocked by G1.
