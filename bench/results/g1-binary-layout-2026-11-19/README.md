# 02 · Small snapshots — 0/1-entry binary-layout sweep

**Code placement moved the empty brute denominator across the 2× threshold without materially moving `index`.** It did not reproduce the 2.3 ns/scene singleton slowdown from the first [direct-result run](../g1-direct-pair-result-2026-11-19/README.md). No runtime, harness, or baseline code changed; this is not a new G1 verdict.

## Controlled relinks

The [sweep](sweep.sh) shifts the compiled benchmark TU's main `.text` by 0–112 bytes in 16-byte increments using linker padding. It changes only the input section alignment metadata and padding, not the benchmark object instructions or fixtures. The shift moves both runners and other functions in the same section; it does not isolate one particular instruction or cache mechanism. Both unshifted relinks match their Release `.text` hashes. The [verification](summarize.py) checks exact address shifts, sizes, and instruction sequences after normalizing relocated operands for both runners, `LibraryIndex::pairs`, and `Index::buildPrepared`.

The unshifted `index` runner moved from `0xe4e00` to `0xe5100` between control and direct-result; brute moved from `0x5a8c0` to `0x5ab80`. Both runner entries retain 64-byte alignment. The intra-cache-line residues at the `index` pair and build entry points also match between the two original builds; their page offsets differ. [Placement CSV](placement.csv) records both offsets at each shift.

## Results

Ranges of medians across three processes per placement and scene. Ratios are medians of same-process point-estimate ratios, not frozen G1 intervals.

| Source | Entries | `index` ns/scene | Brute ns/scene | `index` / brute |
|---|---:|---:|---:|---:|
| Control | 0 | 10.4–10.7 | 4.8–5.5 | 1.91–2.23× |
| Direct result | 0 | 10.4–10.6 | 5.0–5.3 | 1.98–2.10× |
| Control | 1 | 19.0–19.2 | 6.6–7.1 | 2.70–2.89× |
| Direct result | 1 | 18.9–19.1 | 6.5–7.1 | 2.70–2.92× |

At control shifts +32 and +96, both scenes had a 4.8 ns empty brute median versus 5.4–5.5 ns at +0 and +64. The pattern repeated 64 bytes apart. This shows that placement alone can push the empty ratio across 2× on the same source. It does not prove the cause of every prior empty verdict or singleton outlier, and no placement brought the singleton near 2×.

All 384 rows reported `ok`, matched retained-per-sweep pair counts, checksums, fixture hashes and traversal counters, and had zero warm allocations. The [placement timings](placement-timings.csv) give medians and per-placement ratio ranges; [process summaries](round-summary.csv) provide absolute estimates and half-widths.

**Next:** stay on 02. Repeat the full G1 matrix for the original, unpadded control and direct-result Release binaries in the manifest's calibrated `performance` energy preference. Interleave independent processes. Do not pick a padding offset, change brute, or treat an inconclusive empty row as a pass. The singleton gap remains regardless of placement.

## Reproduction

Reconstruct and build the [control and direct-result Release sources](../g1-direct-pair-result-2026-11-19/README.md#reproduction) with pinned Box2D. From the repository root run `bash bench/results/g1-binary-layout-2026-11-19/sweep.sh /path/to/control-build /path/to/direct-build /fresh/outdir 6`. The two-engine diagnostic uses the original batched `runStep`, seed and 256-scene fixtures for `uniform` and `clustered`, sizes 0 and 1. Each process uses 67 rounds; each placement/source runs in three blocks with different [orders](run-order.csv). No gate evaluator is run.

[Control hashes](control-text.sha256), [direct hashes](direct-text.sha256), [raw samples](raw-samples.tar.gz), [environment](environment.txt).
