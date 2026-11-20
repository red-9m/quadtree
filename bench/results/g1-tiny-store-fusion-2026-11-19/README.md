# 02 · Small snapshots — fuse tiny flat entry stores

**Not retained.** Fusing the four store loops in `buildFlatInteger` saved 2.1–2.6 ns/scene at four entries in `uniform` and 3.4–4.2 ns/scene in `clustered`. Two entries showed no resolved gain. No failing G1 row passed, and the `uniform` empty row failed in all three fusion timings. Runtime code stays at the [identity-inline control](../g1-flat-path-identity-inline-2026-11-19/README.md).

The [staged patch](fusion.patch) fuses box, payload, metadata, and slot construction for sizes 2–4 only. Singleton and larger stores, validation, owner acquisition, handle publication, and pair dispatch are unchanged. The rebuilt control benchmark is byte-identical to the previously retained benchmark.

| Rounds | Control: pass / fail / inconclusive | Fusion: pass / fail / inconclusive |
|---:|---:|---:|
| 45 | [10 / 6 / 0](45-control/gates.txt) | [8 / 8 / 0](45-fusion/gates.txt) |
| 63 | [9 / 6 / 1](63-control/gates.txt) | [9 / 7 / 0](63-fusion/gates.txt) |
| 81 | [8 / 6 / 2](81-control/gates.txt) | [8 / 8 / 0](81-fusion/gates.txt) |

At 81 rounds, four-entry `index` went from 60.0 to 57.8 ns/scene in `uniform` and from 57.0 to 52.8 in `clustered`, still at 2.66× and 2.58× brute. No absolute `index` regression was resolved at any size. At zero entries in `uniform`, `index` stayed at 10.4–10.6 ns/scene, but brute measured 5.0 in each fusion process versus 5.4–5.5 in the controls. The gate failure does not show a slower empty `index` path. Both scenes passed at sizes 8–64 in every run.

[Full G1 comparison](g1-comparison.csv): estimates, absolute errors, frozen ratio intervals and verdicts for all 16 rows per run. [Index change](index-change.csv): fusion/control timing ratios and 95% intervals, using the same error propagation on the rounded CSV estimates. Each run directory contains the raw samples and gate output.

Both library engines matched pair counts, checksums and fixture hashes in all six runs. Candidate tests and node visits were unchanged, and both engines had zero warm allocations. The [exact-pair probe](exact-pair-probe.log) matched unordered `index` pair identities against brute and found zero warm allocations at all 16 rows. All [20 CTest checks](tests.log) passed, including build/failure checks, exception-disabled builds, and the quick differential fuzzer.

**Next:** stay on 02. Profile the retained runner at 1 and 2 entries after identity inlining to rank the remaining adapter, replacement, and pair costs against the 2× budget. Do not rerun store fusion unchanged.

## Reproduction

Reconstruct the retained source from the [identity-inline recipe](../g1-flat-path-identity-inline-2026-11-19/README.md#reproduction), starting at the recorded [HEAD](source-head.txt). Verify the [control input hashes](control-inputs.sha256). In a separate source tree, apply `fusion.patch` with `git apply` and verify the [fusion input hashes](fusion-inputs.sha256). Build each with the [baseline Release recipe](../current-snapshot-2026-11-18/README.md#environment-and-reproduction) and verify [`.text` hashes](text.sha256) with `objcopy --dump-section .text=NAME.text`. Run `taskset -c 6 bash bench/results/g1-flat-input-2026-11-19/reproduce.sh /path/to/quadtree-bench /fresh/outdir ROUNDS` in the order listed in [environment.txt](environment.txt). Both variants ran on the Ryzen 9 5900HS with GCC 16.2.1, Release `-O3 -DNDEBUG`, and the same pinned Box2D. The clock source was `tsc` for this comparison, not `hpet` as in the earlier control measurements.

For the exact-pair check, compile [phase.cpp](../g1-flat-input-2026-11-19/phase.cpp) against the fusion source with `c++ -std=gnu++20 -O3 -DNDEBUG -Ibench -I. <phase.cpp> bench/bench-memory.cpp -o phase` and run `taskset -c 6 ./phase SCENE N` for each of the 16 rows. Build the staged tree and run `ctest --test-dir <build> --output-on-failure` for verification.
