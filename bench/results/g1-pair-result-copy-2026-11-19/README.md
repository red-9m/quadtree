# 02 · Small snapshots — remove the `forEachPair` result copy

**Not retained.** Scalar return construction removed the broad `QueryResult` copy and saved 3.2–4.5 ns/scene at two entries. Neither measured variant closed a failing G1 row, and each showed repeatable regressions elsewhere. Runtime and benchmark source are unchanged.

## Code generation

Only `Index::forEachPair` changed in staging. All paths used the same dispatch, visitors, budgets, public result layout, adapter and runner.

- [`member`](member.patch) returned status and the `Counters` subobject separately. GCC stopped copying status together with a counter, but still copied all counters to a second stack block. Not timed.
- [`scalar`](scalar.patch) returned status and each counter separately. GCC read only the fields used by unordered `LibraryIndex::pairs`, directly into registers, and removed the intermediate copy.
- [`tiny`](tiny.patch) used the scalar return only after `tinyPairs`. GCC avoided the broad copy on that branch but still staged the fields it needed in stack slots. Other paths kept the aggregate return.

The [disassembly](result-boundary.asm) shows the return sites. The last store-to-load boundary in the frozen runner was not changed.

## G1 build + pairs

| Rounds | Control: pass / fail / inconclusive | Candidate: pass / fail / inconclusive |
|---:|---:|---:|
| 63 | [8 / 6 / 2](63-control/gates.txt) | [`scalar` 10 / 6 / 0](63-scalar/gates.txt) |
| 99 | [10 / 6 / 0](99-control/gates.txt) | [`scalar` 10 / 6 / 0](99-scalar/gates.txt) |
| 81 | [8 / 6 / 2](81-control/gates.txt) | [`tiny` 9 / 7 / 0](81-tiny/gates.txt) |
| 117 | [8 / 6 / 2](117-control/gates.txt) | [`tiny` 8 / 8 / 0](117-tiny/gates.txt) |

- `scalar` saved 3.6–4.5 ns/scene at two entries and 0.9–1.4 ns/scene on the empty rows. The two-entry repeat still measured ~2.54× brute in both scenes. `index-ordered` at `uniform` eight entries was 3.9–4.2% slower on both runs, with both change intervals outside 1×.
- `tiny` saved 3.2–3.9 ns/scene at two entries with no resolved regression at eight entries in `index-ordered`. The `clustered` singleton was 2.4–2.6 ns/scene slower on both runs even though it does not enter `tinyPairs`. The `uniform` empty row failed in both runs; `clustered` empty also failed on repeat. On that repeat the absolute empty `index` times overlapped control and brute was faster. These results do not identify an instruction-level cause for the off-path regressions.

The [G1 comparison](g1-comparison.csv) lists all 16 rows per run with absolute estimates and frozen intervals. [Index change](index-change.csv) compares each candidate against its matching control for both library engines, using log-ratio intervals from the rounded CSV estimates. The eight runs matched fixture hashes, pair counts, checksums and traversal counters. Both library engines had zero warm allocations. Exact unordered pair identities matched brute, with zero warm allocations, on all 16 rows for each timed candidate: [`scalar`](scalar-exact-pair-probe.log), [`tiny`](tiny-exact-pair-probe.log). Both timed candidates passed all 20 CTest checks, including exception-disabled tests and the quick differential fuzzer: [`scalar`](scalar-tests.log), [`tiny`](tiny-tests.log).

**Next:** stay on 02. Test constructing the result in its final storage instead of rewriting field copies. Keep public layouts and the adapter fixed. Inspect the generated singleton and ordered paths before repeating G1; do not rerun these return variants unchanged.

## Reproduction

Reconstruct the [retained source](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction) and verify its [control input hashes](../g1-tiny-store-fusion-2026-11-19/control-inputs.sha256). Apply exactly one candidate patch in each staged source tree. Build with the [baseline Release recipe](../current-snapshot-2026-11-18/README.md#environment-and-reproduction); verify [`.text` hashes](text.sha256) with `objcopy --dump-section .text=NAME.text`. Use the [same batched recipe](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction) in the order recorded in [environment.txt](environment.txt). It also records the pinned CPU, governor, compiler, kernel, clock source and Box2D revision. Use the same exact-pair probe from that recipe and run `ctest --test-dir <build> --output-on-failure` on each staged build.
