# 03 · Representative snapshots — hoist integer Morton key inputs

**The staged patch cuts build time 12.5–17.1% and full-step time 7.1–11.8% across the six failing G2 rows.** `clustered` at 100,000 now passes, but all six hard failures remain. The patch stays staged: empty `uniform` G1 fails in both candidate runs.

[Patch](hoist.patch): compute each integer box's exact `std::midpoint` centers and unsigned spans once in `Index::makeKey`. Keep the same per-cell span limits, split/child selection, Morton digit encoding and floating-point calculations. No changes to thresholds, storage, the adapter, or the benchmark harness.

## Full 01 snapshot slice

| Rounds | G2 control | G2 candidate | G1 control | G1 candidate |
|---:|---:|---:|---:|---:|
| 9 | [8 / 6 / 1](9-control/gates.txt) | [9 / 6 / 0](9-candidate/gates.txt) | 10 / 6 / 0 | 8 / 8 / 0 |
| 15 | [8 / 6 / 1](15-control/gates.txt) | [9 / 6 / 0](15-candidate/gates.txt) | 8 / 6 / 2 | 8 / 7 / 1 |

Counts are pass / fail / inconclusive. At 100,000 entries, `clustered` moves from inconclusive to pass in both runs: 1.812× [1.757–1.870] and 1.859× [1.798–1.921] against grid. The six G2 failures remain at 2.49–3.01× grid in the 15-round candidate run.

At 100,000 entries in the 15-round run, control → candidate, ms/snapshot:

| Scene | Step estimate | Build median | Pairs median |
|---|---:|---:|---:|
| `uniform` | 20.547 → 18.558 | 13.178 → 11.519 | 7.140 → 6.997 |
| `split-line` | 22.162 → 19.989 | 14.784 → 12.733 | 7.267 → 7.151 |
| `split-line-translated` | 21.806 → 19.639 | 14.801 → 12.747 | 6.840 → 6.715 |
| `clustered` | 27.952 → 26.423 | 13.501 → 12.046 | 14.350 → 14.060 |

[All 15 G2 verdicts, phases, counts and allocations](g2-comparison.csv) per run. [Step changes](step-change.csv) compare the matched builds with log-ratio intervals; phase medians are descriptive. Raw warm samples and gate outputs are in each run directory.

G0/G3/G4/G6 pass. G9 still fails only at `mixed-size` 100,000 (4.65–4.66× in the candidate); the ratio rises as unordered gets faster. Empty G1 `index` estimates overlap control in all four comparisons; brute is 0.3–0.5 ns/scene faster in the candidate binary despite unchanged source. Both runner addresses moved, so the new G1 failures do not establish an absolute empty `index` regression. They are still failures under the frozen evaluator. The nine-round run also shows an absolute four-entry `clustered` slowdown that does not resolve on repeat. Do not retain the patch or infer an empty pass from overlapping times.

## Correctness

[Byte-for-byte probe](equivalence.log): matches `hi`, `lo`, `level`, and `entry` on 15 `int32` G2 fixtures and over 1.17 million `int32`/`int64` keys spanning signed limits, degenerate axes, odd extents, and nonzero base depths. On all 15 fixtures, the node preorder (cells, bounds, counts and filter summaries), dense entry IDs, and exact unordered and ordered pair callback streams match. Warm probe builds and traversals allocate zero.

[Full-slice verification](verification.log): uncapped rows match fixture hashes, pair counts, checksums, work, allocations and memory. All 272 library rows allocate zero when warm. All [20 CTest checks](tests.log) pass on the staged source, including build/failure, exception-disabled, and differential-fuzzer checks.

**Next:** profile retained `Index::selfPairs`, `crossPairs`, and `itemsVsSubtree` on the six failing G2 rows to separate node dispatch/pruning from exact pair tests. Select one measured traversal target, preserving budgets and callback semantics. Recheck the key patch only in a combined candidate; do not tune layout against the empty G1 denominator.

## Reproduction

Reconstruct the [retained source](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction) and verify its input hashes. Apply `hoist.patch` to a separate candidate tree. Build both Release with GCC 16.2.1 and Box2D `178495ba35695aefe9c321e569d2277f06db3b90`; verify [`.text` hashes](text.sha256) with `objcopy --dump-section .text=NAME.text`. Set the platform profile and CPU energy preferences to `performance` before running from the repository root:

```sh
bash bench/results/g2-integer-key-2026-11-19/repeat.sh /path/to/control-bench /path/to/candidate-bench /fresh/outdir
python3 bench/results/g2-integer-key-2026-11-19/summarize.py /fresh/outdir
bash bench/results/g2-integer-key-2026-11-19/verify.sh /path/to/control-source /path/to/candidate-source
ctest --test-dir /path/to/candidate-build --output-on-failure
```

The [environment](environment.txt) and [run order](run-order.csv) record four serial processes pinned to CPU 6, with the original 01 fixtures, engine set and frozen limits. The [diagnostic driver](equivalence.cpp) adds a key accessor to temporary probe headers only; neither timing binary includes it.
