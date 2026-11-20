# 02 · Small snapshots — unpadded G1 in performance mode

**G1 still fails at 1, 2 and 4 entries in both scenes.** Neither binary passes all 16 rows in any of the six processes. The direct-result patch stays staged; runtime, harness and brute baseline are unchanged.

| Rounds | Control: pass / fail / inconclusive | Direct: pass / fail / inconclusive |
|---:|---:|---:|
| 81 | [8 / 6 / 2](81-control/gates.txt) | [8 / 6 / 2](81-direct/gates.txt) |
| 117 | [8 / 6 / 2](117-control/gates.txt) | [8 / 6 / 2](117-direct/gates.txt) |
| 153 | [9 / 6 / 1](153-control/gates.txt) | [8 / 6 / 2](153-direct/gates.txt) |

- **0 entries:** Only `153-control` clustered passes, at 1.944× [1.893–1.997]. The other 11 empty measurements are inconclusive. Absolute `index` estimates are 10.5–10.6 ns/scene with overlapping intervals; brute ranges from 5.2 to 5.4 ns/scene. This is not evidence of an absolute empty-path regression.
- **1 entry:** Direct minus control ranges from −0.3 to +0.3 ns/scene. Every difference interval includes zero; the earlier 2.3 ns/scene slowdown did not repeat. At 153 rounds the control takes 18.8–19.1 ns/scene versus 6.7 ns/scene for brute, a 5.4–5.7 ns point-estimate gap to 2×.
- **2 and 4 entries:** Direct result saves 3.1–4.7 and 2.0–3.1 ns/scene, respectively. All six comparisons at each size resolve a gain, but all G1 rows at those sizes still fail.
- **8–64 entries:** All G1 rows pass. No resolved `index` regression at these sizes or `index-ordered` regression at any size.

[Full G1 comparison](g1-comparison.csv): absolute ns/scene and errors, frozen ratio intervals and verdicts. Each run directory contains its summary CSV, compressed raw samples and gate output. [Index change](index-change.csv) compares direct/control timing using log-ratio intervals from the rounded estimates. No intervals are pooled across processes.

All [576 engine rows](verification.log) report `ok` and match the previous runs' per-engine pair counts, checksums, fixture hashes and traversal counters. Both library engines have zero warm allocations in all 192 library rows. The [exact-pair probe](exact-pair-probe.log) matched unordered pair identities against brute with zero warm allocations in all 16 rows on both sources. All [20 CTest checks](tests.log) passed on the staged direct-result build.

**Next:** stay on 02. Isolate singleton replacement overhead outside the flat commit (`buildPrepared` dispatch and workspace checks) on the actual batched runner. Use a diagnostic control to measure elapsed cost before proposing another fast path; keep the frozen harness, brute baseline and build guarantees intact.

## Reproduction

Reuse the two [original Release builds](../g1-direct-pair-result-2026-11-19/README.md#reproduction) or reconstruct them with pinned Box2D. Verify the [binary and `.text` hashes](binaries.sha256); do not relink with padding. Run from the repository root:

```sh
bash bench/results/g1-performance-2026-11-19/repeat.sh /path/to/control-bench /path/to/direct-bench /fresh/outdir
```

Set the platform profile and all CPU `energy_performance_preference` policies to `performance` before running; restore the prior settings afterward. The script checks both before and after each process. The [environment log](environment.txt) records the restored `balanced` profile and `power` preferences. The `powersave` governor was unchanged (`amd-pstate-epp`). The benchmark used invariant `rdtsc` with the kernel clock source `tsc`, as in the prior direct-result run, not HPET. All processes were pinned to CPU 6; the [run order](run-order.csv) interleaves variants at 81, 117 and 153 rounds. Each process uses the same six engines, seed and 256-scene fixtures as the earlier G1 repeat.

Use the [prior exact-pair recipe](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction) on both sources and run `ctest --test-dir <direct-build> --output-on-failure` for correctness checks. Probe wall times are diagnostic, not G1 measurements.
