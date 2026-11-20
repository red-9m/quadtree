# 02 · Small snapshots — isolate the singleton build call and frame

**Inlining warm singleton replacement saves 0.8–1.1 ns/scene versus the matched outlined helper.** It drops the `buildPrepared` call and frame on the warm path, but both singleton G1 rows still fail. No runtime, adapter, harness or brute source changed in the retained tree.

## Staged hot/cold split

- [Inline](inline.patch): `buildRange` sends a prepared, contiguous integer singleton with no handle output through an inline helper. The helper specializes the existing flat commit for one entry. The general path remains the fallback for cold builds, ranked input, fixed extent, or another ineligible case. Workspace checks, ownership acquisition and geometry validation still run before commit.
- [Outlined](outlined.patch): same source after the inline patch, replacing only the helper's `gnu::always_inline` with `gnu::noinline`.

The [disassembly](call-boundary.asm) confirms that the warm `int32` runner uses the original `buildPrepared` call and `0x158`-byte callee stack allocation in control, a `0x38`-byte callee allocation in outlined, and neither build call nor separate frame in inline. The inline path still calls `LibraryIndex::pairs` after commit. The runner's own stack allocation falls from `0x4d8` to `0x498` bytes. The outlined/inline comparison measures the consequence of inlining, including caller register allocation and code layout, not an isolated `call`/`ret` latency.

## Full G1 repeats

| Rounds | Control | Outlined | Inline |
|---:|---:|---:|---:|
| 81 | [8 / 6 / 2](81-control/gates.txt) | [8 / 8 / 0](81-outlined/gates.txt) | [8 / 6 / 2](81-inline/gates.txt) |
| 117 | [8 / 6 / 2](117-control/gates.txt) | [8 / 8 / 0](117-outlined/gates.txt) | [8 / 6 / 2](117-inline/gates.txt) |
| 153 | [8 / 6 / 2](153-control/gates.txt) | [8 / 7 / 1](153-outlined/gates.txt) | [8 / 6 / 2](153-inline/gates.txt) |

Counts are pass / fail / inconclusive. All 1/2/4 rows fail and all 8–64 rows pass. Outlined adds empty failures: absolute empty `index` times overlap control; brute is faster in those binaries. Inline saves 0.6–1.0 ns/scene at zero entries, but its empty intervals still cross 2×. This off-path change is not evidence of a faster empty build.

At one entry, inline saves 1.5–2.0 ns/scene vs control except for the 117-round clustered process, where control was slower and the delta was 3.5 ns/scene. All six comparisons resolve a gain against the matched outlined helper. At 153 rounds, inline takes 17.4 ns/scene in `uniform` and 17.6 in `clustered`; reaching 2× brute needs another 4.6 and 4.4 ns/scene respectively on the point estimates. Neither patch is retained.

[Full G1 comparison](g1-comparison.csv) lists all 16 rows per run with absolute estimates and frozen intervals. [Cross-binary changes](index-change.csv) use log-ratio intervals from rounded estimates; no process intervals are pooled. Each run directory has raw samples, summary CSV and frozen gate output.

All [864 engine rows](verification.log) match retained pair counts, checksums, fixture hashes and traversal counters. Both library engines have zero warm allocations in all 288 library rows. The probes match exact unordered pair identities and verify zero warm allocations in all 16 rows for [inline](inline-exact-pair-probe.log) and [outlined](outlined-exact-pair-probe.log). All 20 CTest checks pass on each staged build ([inline](inline-tests.log), [outlined](outlined-tests.log)), including build/failure, exception-disabled and quick differential-fuzzer checks.

**Next:** stay on 02. Isolate the remaining singleton `LibraryIndex::pairs` call by staging a library-only `Index::forEachPair` early-return wrapper with the nontrivial path outlined. Verify whether the wrapper makes the unchanged adapter inline on the 0/1 path before timing. If it does not, record the remaining gap rather than changing the adapter or repeating the result-copy variants.

## Reproduction

Use the original [unpadded Release control](../g1-performance-2026-11-19/README.md#reproduction) and its retained source. Apply `inline.patch` to both staging trees, then `outlined.patch` to the outlined tree only. Use the same Release flags and pinned Box2D. Verify the [binary and `.text` hashes](binaries.sha256); no padding was used.

Set the platform profile and CPU energy preferences to `performance`, then run from the repository root:

```sh
bash bench/results/g1-singleton-frame-2026-11-19/repeat.sh /path/to/control-bench /path/to/outlined-bench /path/to/inline-bench /fresh/outdir
```

The [environment](environment.txt) and [run order](run-order.csv) record nine independent processes pinned to CPU 6: three variants at 81, 117 and 153 rounds, rotating position. Same six engines, seed and 256-scene fixtures as the original G1 repeat. Use the [existing exact-pair recipe](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction) and `ctest --test-dir <build> --output-on-failure` on both staged sources. Compress each `snapshot-raw.csv` with `gzip -n` before running `summarize.py` on the result directory.
