# 03 · Representative snapshots — inline `crossPairs` rejection

**Rejected.** Inlining the node gate did not resolve a full-step gain on any of the six failing G2 rows. It slowed `clustered` and added a G2 failure at 100,000 in both repeats. Runtime source is unchanged; the integer key hoist was not combined with this patch.

## Candidate

[Patch](inline-gate.patch): inline `crossPairs` and outline `crossPairsOverlapping`. The inline gate checks active/empty state, charges the node visit, then checks bounds and filters in the original order. Only a surviving node pair enters the recursive body. The [unordered `int32` disassembly](call-boundary.asm) shows the bounds branches bypassing the outlined call for both child and sibling pairs. No thresholds, adapter, baseline, or other query paths changed.

## Full 01 snapshot slice

| Rounds | G2 control | G2 candidate | G1 control | G1 candidate |
|---:|---:|---:|---:|---:|
| 9 | [8 / 6 / 1](9-control/gates.txt) | [7 / 7 / 1](9-candidate/gates.txt) | 10 / 6 / 0 | 8 / 6 / 2 |
| 15 | [8 / 6 / 1](15-control/gates.txt) | [7 / 7 / 1](15-candidate/gates.txt) | 9 / 6 / 1 | 8 / 6 / 2 |

Counts are pass / fail / inconclusive. `clustered` at 10,000 goes from pass to inconclusive in both repeats; its step slows 4.7–6.1%. At 100,000 it goes from inconclusive to fail in both. The 15-round `clustered` 100,000 step slows 7.4%, with the matched ratio interval entirely above 1× [1.041–1.108].

At 100,000 entries in the 15-round repeat, control → candidate, ms/snapshot:

| Scene | Step estimate | Build median | Pairs median |
|---|---:|---:|---:|
| `uniform` | 20.661 → 20.715 | 13.160 → 13.080 | 7.115 → 7.373 |
| `split-line` | 21.907 → 21.874 | 14.632 → 14.708 | 7.209 → 6.957 |
| `split-line-translated` | 21.660 → 21.304 | 14.680 → 14.648 | 6.823 → 6.535 |
| `clustered` | 27.726 → 29.772 | 13.353 → 13.751 | 14.300 → 15.776 |

Both 100,000-entry split-line scenes show lower pair medians in both repeats, but the full-step change intervals include 1×. A high rejection rate alone did not predict a full-step gain. [All 15 G2 rows](g2-comparison.csv) per run include absolute phases, work, allocations and frozen intervals. [Step changes](step-change.csv) compare matched control/candidate estimates; phase medians are descriptive. Each run directory includes raw warm samples and gate output.

G0/G3/G4/G6 pass. The G1 hard failures are unchanged; both candidate empty rows are inconclusive in both repeats. G9 still fails only at `mixed-size` 100,000.

## Correctness

[Exact stream probe](equivalence.log): the six failing G2 fixtures and a filtered 192-entry fixture match byte-for-byte for emitted pair IDs, order, predicate calls, status and all counters with and without instrumentation. The probe covers independent and combined budgets, exact and one-short limits on the large instrumented rows, Stop, and both boundary modes on the filtered fixture. Warm probe queries and builds allocate zero. [Full-slice checks](verification.log) match uncapped fixture hashes, pair results, traversal counts, allocations and memory; all 272 library rows have zero warm allocations. [All 20 CTest checks](tests.log) pass on the candidate, including exception-disabled, build/failure and differential-fuzzer checks.

**Next:** profile the retained bulk entry commit at 100,000 entries. Separate spatial-order descriptor/payload gathers and dense box/meta/slot stores from publish bookkeeping before choosing a change. The [build profile](../g2-phase-profile-2026-11-19/README.md#build-attribution) assigns 21–25% of build cycles to this phase. Do not combine this rejected traversal patch with the key hoist.

## Reproduction

Reconstruct the [retained source](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction) and apply `inline-gate.patch` in a separate tree. Build Release with GCC 16.2.1 and Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. Check [`.text` hashes](text.sha256) with `objcopy --dump-section .text=NAME.text`. Set the platform profile and CPU energy preferences to `performance`, then run from the repository root:

```sh
bash bench/results/g2-cross-gate-2026-11-19/verify.sh /path/to/control-source /path/to/candidate-source
bash bench/results/g2-integer-key-2026-11-19/repeat.sh /path/to/control-bench /path/to/candidate-bench /fresh/outdir
python3 bench/results/g2-integer-key-2026-11-19/summarize.py /fresh/outdir
ctest --test-dir /path/to/candidate-build --output-on-failure
```

[Environment](environment.txt) and [run order](run-order.csv): four independent processes pinned to CPU 6, same 01 fixtures and limits as the retained baseline.
