# 02 · Small snapshots — construct the pair result in final storage

**Not retained.** The direct result saved 3.5–4.2 ns/scene at two entries and 2.2–3.2 ns/scene at four. Six G1 rows still failed; both empty rows were inconclusive in every candidate run. Runtime and benchmark source are unchanged.

The [staged patch](direct.patch) gives the existing `tinyPairs` branch a context that writes to the local `QueryResult` being returned, instead of returning an owned context member. It does not change pair dispatch, budget checks, counters, public layouts, or the adapter. Other branches keep the owning context. The initial candidate needed explicit `WorkBudget{}` arguments in `forEachNode` and `forEachEntry` to disambiguate construction with the inherited constructor. That fix did not change benchmark [`.text`](text.sha256).

The [disassembly](result-boundary.asm) shows the tiny path bypassing the broad stack-to-stack `QueryResult` copy. The inlined adapter still spills the needed counters; it does not construct `bench::Counters` directly from traversal registers. The singleton still branches before tiny dispatch, but its unordered `pairs` code moved and stack offsets changed. Ordered `LibraryIndex::pairs` has the same instruction sequence after normalizing relocated targets and RIP-relative displacements.

| Rounds | Control: pass / fail / inconclusive | Direct: pass / fail / inconclusive |
|---:|---:|---:|
| 81 | [9 / 6 / 1](81-control/gates.txt) | [8 / 6 / 2](81-direct/gates.txt) |
| 117 | [8 / 6 / 2](117-control/gates.txt) | [8 / 6 / 2](117-direct/gates.txt) |
| 153 | [10 / 6 / 0](153-control/gates.txt) | [8 / 6 / 2](153-direct/gates.txt) |

At 153 rounds, two-entry `index` went from 35.3 to 31.1 ns/scene in `uniform` and from 34.7 to 30.9 in `clustered`. Absolute empty `index` timing overlapped control in all three comparisons, but the empty gate intervals did not clear 2× brute. The `clustered` singleton slowed by 2.3 ns/scene at 81 rounds, but that did not repeat. There was no resolved ordered eight-entry regression on any run, unlike the [unscoped scalar return](../g1-pair-result-copy-2026-11-19/README.md). All 8–64 G1 rows still pass.

The [G1 comparison](g1-comparison.csv) lists all 16 rows per run with absolute times, errors and frozen intervals. [Index change](index-change.csv) compares both library engines against matched controls using log-ratio intervals from the rounded estimates. All 576 engine rows matched pair counts, checksums, fixture hashes and traversal counters. Both library engines had zero warm allocations. The [exact-pair probe](exact-pair-probe.log) matched unordered pair identities against brute with zero warm allocations in all 16 rows. All [20 CTest checks](tests.log) passed on the final staged source, including exception-disabled tests and the quick differential fuzzer.

**Next:** stay on 02. Isolate binary-layout sensitivity of the 0/1-entry paths before another tiny-path change. Compare control and direct-result runner placement and use a controlled alignment sweep with both sources and the harness unchanged. Treat the sweep as diagnostic, not G1 acceptance.

## Reproduction

Reconstruct the [retained source](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction) and verify the [control hashes](../g1-tiny-store-fusion-2026-11-19/control-inputs.sha256). Apply `direct.patch` in a separate tree. Build both with the [baseline Release recipe](../current-snapshot-2026-11-18/README.md#environment-and-reproduction) and verify [`.text` hashes](text.sha256) with `objcopy --dump-section .text=NAME.text`. Run the [same batched recipe](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction) in the order recorded in [environment.txt](environment.txt). Run the same exact-pair probe and `ctest --test-dir <build> --output-on-failure` on the staged candidate.
