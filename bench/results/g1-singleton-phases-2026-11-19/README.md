# 02 · Small snapshots — isolate singleton replacement costs

**No runtime change retained.** In the warm singleton probe, removing edge validation, the identity CAS, or extent publication separately saved a median 0.5–0.7 ns/scene. Replacing `mExtent = extent` with `mExtent.emplace(extent)` saved 0.1 ns in the probe and did not improve G1 on repeat. The gate still needs about 7 ns less per singleton scene.

## Isolation

The [probe script](isolate.sh) uses the retained source and the [exact-pair driver](../g1-flat-input-2026-11-19/phase.cpp). Each process runs 131,072 warm 256-scene batches of one-entry `int32` fixtures. Nine rounds per scene rotate variant order. The workload and pair scan are unchanged. The script edits only staged headers, one substitution at a time in `Index::buildFlatInteger`:

- `prevalidated`: omit the first box's `canonicalize` check; fixtures already contain valid integer edges.
- `single-threaded`: use a relaxed load and store instead of CAS on the same owner counter. Each build in this single-threaded probe still gets a fresh owner, with the same exhaustion check. Not thread-safe as a library implementation.
- `no-extent`: omit the optional extent assignment, leaving entry stores unchanged. The singleton pair scan does not read the extent. This is not a valid library implementation.
- `emplace`: publish the same engaged extent with `optional::emplace`. This is the only contract-preserving candidate tested in G1.

| Variant | Uniform ns/scene | Clustered ns/scene | Paired median saving, uniform / clustered | Cycles/scene, uniform / clustered |
|---|---:|---:|---:|---:|
| Control | 13.0 | 13.1 | — | 58.10 / 58.29 |
| `prevalidated` | 12.4 | 12.4 | 0.6 / 0.7 ns | 55.27 / 55.24 |
| `single-threaded` | 12.4 | 12.4 | 0.6 / 0.6 ns | 55.46 / 55.43 |
| `no-extent` | 12.6 | 12.6 | 0.5 / 0.5 ns | 56.09 / 56.05 |
| `emplace` | 12.9 | 12.9 | 0.1 / 0.1 ns | 57.52 / 57.61 |

[Raw samples](phase-raw.csv) include total user cycles and instructions; normalize by `repeats × 256`. Savings are medians of same-round control minus variant. The whole probe is timed, so code generation and overlap affect each delta. The deltas are neither additive nor instruction latencies. In particular, prior samples attributed to the optional assignment do not imply that removing it will save its sampled share of cycles. Probe figures are not gate timings.

## Full G1 step

The [comparison CSV](g1-comparison.csv) gives absolute times, errors and ratio intervals for all 16 rows per run. [27-round control](27-control/gates.txt), [27-round `emplace`](27-emplace/gates.txt), [45-round `emplace`](45-emplace/gates.txt) and [45-round control](45-control/gates.txt) all give **8 pass, 6 fail, 2 inconclusive**. The 1/2/4 rows fail in both scenes; both empty rows are inconclusive on this run's CPU setting, including the unchanged control. The previous repeat used `performance` and passed both empty rows; this run used `power`.

`emplace` measured 20.6–20.7 ns/scene at one entry, versus 20.5–21.6 ns for the control. Both library engines matched pair counts, checksums, fixture hashes, candidates and node visits across runs, with zero warm allocations. The [exact-pair probe](exact-pair-probe.log) also matched brute pair identities in all 16 `emplace` rows, with zero warm allocations. [16 CTest checks passed](tests.log) on unchanged source, including exception-disabled build/failure tests and the quick differential fuzzer.

**Next:** stay on 02. Profile the actual batched `runStep` for the control at 0/1/2/4. Separate adapter/replacement from pair dispatch and counter/checksum work before choosing another runtime change. Keep the frozen harness and brute baseline unchanged.

## Reproduction

Use the [retained source reconstruction and checksums](../g1-view-construction-2026-11-19/README.md#reproduction). Run `bash bench/results/g1-singleton-phases-2026-11-19/isolate.sh /fresh/outdir 6 9` on the unmodified source. The [phase input hashes](phase-source-inputs.sha256) cover the modified-in-staging headers and the driver.

For gates, build the unchanged source and a copy with only [emplace.patch](emplace.patch) using the [baseline Release recipe](../current-snapshot-2026-11-18/README.md#environment-and-reproduction). For each binary run `taskset -c 6 bash bench/results/g1-flat-input-2026-11-19/reproduce.sh /path/to/quadtree-bench /fresh/outdir 27`, then repeat with `45`. Run order: 27 control, 27 `emplace`, 45 `emplace`, 45 control. Each run retains its summary CSV, compressed raw samples and gate verdicts.

Environment: Ryzen 9 5900HS, pinned to CPU 6 (SMT sibling CPU 7); GCC 16.2.1; Linux 7.3.0-1-custom; perf 7.1.8; `powersave` governor, `power` energy preference, HPET kernel clock source. Gate timer: invariant TSC. Release flags: `-O3 -DNDEBUG`, no native/LTO. Box2D `v3.2.0` at `178495ba35695aefe9c321e569d2277f06db3b90`.
