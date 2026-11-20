# 02 · Small snapshots — isolate singleton workspace and dispatch

**Workspace guards account for about 0.4–0.5 ns/scene in this diagnostic; flat eligibility checks show no resolved cost.** Skipping both leaves `index` at a median 18.5 ns/scene versus 6.6 for brute. The singleton still needs about 5.3 ns/scene to reach the 2× point estimate. No runtime, harness, or brute source changed; no new G1 verdicts.

## Controlled ablation

[Patch only temporary copies](ablate.py) of the verified original, unpadded control ELF. Replace instructions in place, leaving function addresses, section layout, caller, and flat commit unchanged. [Disassembly diff](guard-ablation.asm):

- `workspace`: NOP the three workspace capacity checks, active check, and enter/leave stores in the `int32` `buildPrepared` specialization. The singleton adapter reserves workspace before building, with no concurrent or reentrant use in this benchmark.
- `dispatch`: NOP the small-set threshold and slot capacity checks, and make the already-taken snapshot-extent branch unconditional. Keep the entry capacity check so the cold first build allocates through the original fallback; it prepares both entry and slot storage. Keep the rank, geometry, and singleton-versus-general commit checks.
- `combined`: apply both changes. These binaries are diagnostic only; they do not preserve the general build contract.

The unmodified batched `runStep` times build plus pairs with the original `index` adapter and `brute`. The experiment measures these specific instruction changes, not an upper bound on all replacement overhead. The `buildPrepared` call, stack frame, and return are untouched. NOPs still affect decoding; deltas are not instruction latencies or additive costs.

## Results

Median within-block savings, ns/scene, with nominal 96.1% order-statistic intervals across 12 process blocks. Savings are control minus variant; estimates are rounded by the benchmark to 0.1 ns/scene.

| Variant | Uniform | Clustered |
|---|---:|---:|
| `workspace` | +0.45 [0.4, 0.5] | +0.40 [0.4, 0.5] |
| `dispatch` | 0.00 [0.0, 0.0] | −0.05 [−0.1, 0.0] |
| `combined` | +0.40 [0.3, 0.5] | +0.40 [0.3, 0.5] |

Brute's median within-block change was zero for every variant in both scenes. The [per-block comparison](block-change.csv) and [aggregate](summary.csv) include both engines. All [192 engine rows](verification.log) match retained fixture hashes, checksums and traversal counters, report zero pairs, and have zero warm allocations. No new build tests: runtime source is unchanged.

**Next:** stay on 02. Isolate the remaining `buildPrepared` call/frame cost before another singleton fast path. Verify a staged hot/cold split actually removes that boundary on the warm path; the [earlier split](../g1-view-construction-2026-11-19/README.md) did not shrink the frame. Do not retest workspace guard removal.

## Reproduction

Use the original [unpadded Release control](../g1-performance-2026-11-19/README.md#reproduction); the [hash check](binaries.sha256) rejects other ELFs. Set the platform profile and CPU energy preferences to `performance` before running, then restore them. From the repository root:

```sh
bash bench/results/g1-singleton-dispatch-2026-11-19/isolate.sh /path/to/control-bench /fresh/outdir
```

Ryzen 9 5900HS, GCC 16.2.1 Release, `rdtsc` with the kernel clock source `tsc`, pinned to CPU 6. The [environment](environment.txt) and [run order](run-order.csv) record 48 independent processes: four variants in each of 12 balanced blocks. Each process runs 67 rounds on the 256-scene singleton fixtures for both scenes. [Raw archive](raw-samples.tar.gz) includes all 411,648 warm samples, process CSVs and logs. Extract it into a fresh directory and pass that directory to `summarize.py` to regenerate comparisons.
