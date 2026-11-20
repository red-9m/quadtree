# 02 · Small snapshots — reduce pair view construction cost

**G1 remains open: 10 pass, 6 fail in both retained runs.** The 2× brute-force limit still fails at 1, 2 and 4 entries in both scenes. Both empty rows and all 8–64-entry rows stayed green.

## Diagnosis and change

The [phase split](phase-summary.csv) separates adapter, replacement, out-of-line `Index::view`, and the rest of the scan (including checksum). On the unchanged run, replacement accounted for 59–60% of probe cycles at one entry and 36–39% at two. The adapter accounted for 21% at one and 10% at two. At four, the out-of-line view alone accounted for 42–48%. Gate steps needed to lose roughly 7 ns at one, 13 ns at two, and 25–31 ns at four. Probe cycle fractions are not gate timings: the noinline driver changes inlining and omits some gate bookkeeping.

`Index::view` now constructs an empty optional rank in the returned view and emplaces a rank only when present. This avoids constructing and copying a separate optional from `rankAt`. The [weighted profile](profile-by-location.csv.gz) shows the out-of-line view's share falling to 12–14% at four entries. The API, benchmark adapter, brute baseline, and build paths are unchanged.

The [adapter metadata cache](adapter.patch) saved little at the failing sizes. [Splitting general build preparation](split.patch) did not shrink GCC's warm-path stack frame or improve on the adapter variant. Building views [inside tiny pair dispatch](inline-view.patch) improved four-entry steps more but failed both previously passing empty rows on repeat. The [combined variant](combined.patch) and the [empty-adapter shortcut](empty-adapter.patch) did not preserve those passes either. None was retained.

## Measurements

| G1 run | Pass | Fail | Inconclusive |
|---|---:|---:|---:|
| [Unchanged, 27 rounds](before/gates.txt) | 10 | 6 | 0 |
| [Retained, 27 rounds](retained/gates.txt) | 10 | 6 | 0 |
| [Retained, 45 rounds](retained-repeat/gates.txt) | 10 | 6 | 0 |

Matched four-entry timings fell from 76.0 to 62.3 ns/scene (`uniform`) and from 68.1 to 59.5 ns/scene (`clustered`). The repeat measured 62.5 and 59.5 ns/scene. Two-entry steps fell by about 2 ns; the singleton showed no repeatable gain. The [comparison CSV](g1-comparison.csv) gives absolute times, errors, and frozen ratio intervals for all rows and variants.

The [exact-pair probe](exact-pair-probe.log) matched brute with zero warm allocations in all 16 rows. The [benchmark checks](checks.txt) also passed for both library engines: matching pair counts, checksums, fixture hashes, candidates, and node visits, with zero warm allocations. [All 16 CTest checks passed](tests.log), including exception-disabled tests and the quick differential fuzzer. The tiny-pair test now checks handle, geometry, filter, and optional rank in both visitors and predicates after slot reuse. No representative or persistent timing runs were made.

**Next:** stay on 02. Isolate the singleton replacement cost before choosing another implementation. Inspect validation, identity acquisition, and extent publication separately; keep fresh identity and failure guarantees. Pair view construction alone cannot close the singleton row because that row reports no pairs.

## Reproduction

Reconstruct the [previous source](../g1-tiny-dispatch-2026-11-19/README.md#reproduction) for the unchanged build. Apply [retained.patch](retained.patch) and verify the [source checksums](source-inputs.sha256) for the retained build. For rejected variants, apply only the matching experiment patch to the unchanged source. Use the [task 01 Release recipe](../current-snapshot-2026-11-18/README.md#environment-and-reproduction) with pinned Box2D; see [environment](environment.txt).

Run `bash bench/results/g1-flat-input-2026-11-19/reproduce.sh /path/to/quadtree-bench /fresh/outdir 27`, then repeat with `45`. Each run directory retains summary CSV, compressed raw samples and gate verdicts. Build the [phase probe](../g1-flat-input-2026-11-19/phase.cpp) using the task 01 `perf` recipe and sample 1, 2 and 4 entries in both scenes. Run the probe without `perf` at 0, 8, 16, 32 and 64 to check exact pairs and warm allocations on the remaining rows.
