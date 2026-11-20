# 02 · Small snapshots — isolate the 0/1-entry pair-query call

**Removing the `LibraryIndex::pairs` call made the empty and singleton steps slower.** The wrapper adds two G1 failures in all three repeats. No runtime, adapter, harness, or brute baseline changes retained.

The staged [patch](wrapper.patch) keeps the existing 0/1-entry early return in `Index::forEachPair` and moves the unchanged nontrivial path into a `gnu::noinline` helper. It preserves the ordered rank check, workspace handling, budgets and counters. This run uses the original build path, not the earlier staged singleton build split.

[Disassembly](call-boundary.asm) confirms that the unchanged adapter inlines in both `int32` runners. Only sizes above one call `forEachPairNontrivial`. The unordered 0/1 path still writes two zero counters to the stack and reloads them before aggregation. This is a code-generation difference, not a measured store-to-load penalty or proof of the slowdown's cause.

## Full G1 repeats

| Rounds | Control: pass / fail / inconclusive | Wrapper: pass / fail / inconclusive |
|---:|---:|---:|
| 81 | [8 / 6 / 2](81-control/gates.txt) | [8 / 8 / 0](81-wrapper/gates.txt) |
| 117 | [9 / 6 / 1](117-control/gates.txt) | [8 / 8 / 0](117-wrapper/gates.txt) |
| 153 | [9 / 6 / 1](153-control/gates.txt) | [8 / 8 / 0](153-wrapper/gates.txt) |

- **0 entries:** `index` is 1.8–2.3 ns/scene slower, with all six intervals resolving a regression. Both scenes fail in every wrapper run.
- **1 entry:** `index` is 1.6–2.2 ns/scene slower in five comparisons. The 117-round clustered comparison is unresolved (21.4 vs 21.3 ns/scene). Both scenes fail every run.
- **2 and 4 entries:** Both scenes still fail. `index` at two entries slows by 3.5–5.3 ns/scene in all six comparisons.
- **8–64 entries:** All G1 rows still pass. Ordered eight-entry `clustered` slows by 3.1–5.7% in all three runs.

[Full G1 comparison](g1-comparison.csv) gives absolute ns/scene and frozen ratio intervals for all 16 rows per run. [Cross-binary changes](index-change.csv) use the log-ratio interval on rounded estimates. Each run directory contains compressed raw samples, summary CSV and gate output.

All [576 engine rows](verification.log) match retained fixture hashes, pair counts, checksums and traversal counters. Both library engines have zero warm allocations in all 192 rows. [Exact unordered pair identities](exact-pair-probe.log) match brute with zero warm allocations in all 16 rows. All [20 CTest checks](tests.log) pass on the staged wrapper, including the exception-disabled tests and quick differential fuzzer.

**Handoff:** pause G1 micro-optimizations; G1 remains open and blocks acceptance. At 153 rounds, retained singleton `index` still needs 5.7–5.8 ns/scene to reach the 2× brute point estimate. Reopen with a new contract-preserving approach, not another repeat of the build/query wrapper probes. Start [03 — G2 snapshot cost](../../../docs/plan/03-g2-snapshot-cost.md) independently.

## Reproduction

Reconstruct the [retained source](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction) and apply `wrapper.patch` in a separate tree. Build Release with GCC 16.2.1 and pinned Box2D (`178495ba35695aefe9c321e569d2277f06db3b90`). Check the [binary and `.text` hashes](binaries.sha256); no relinking or padding.

With the platform profile and CPU energy preferences set to `performance`:

```sh
bash bench/results/g1-singleton-query-2026-11-19/repeat.sh /path/to/control-bench /path/to/wrapper-bench /fresh/outdir
```

The [environment](environment.txt) and [run order](run-order.csv) record six independent processes pinned to CPU 6. Same six engines, seed and 256-scene fixtures as the earlier G1 repeats; invariant `rdtsc` with `tsc` kernel clock source. Use the [existing exact-pair recipe](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction) and `ctest --test-dir <build> --output-on-failure` on the staged tree.
