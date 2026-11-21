# 03 · Ordered source-rank sort on two-stream scatter

**Source sorting accounts for 14.1–14.2% of translated ordered pair cycles and 17.6–17.7% on `uniform`.** Partner-row sorting is only 1.7–2.6% on these fixtures. Screen a source-only sort change next, not a partner-row change. No runtime change or new gate verdict.

## Release profile

The [staged two-stream ELF](../g2-two-scatter-2026-11-21/README.md) is byte-identical to the [matched-temperature control](../g2-two-scatter-thermal-2026-11-21/README.md). Sampled `cycles:u` on Ryzen 9 5900HS, GCC 16.2.1 Release, `int32`, 100,000 entries. Two reversed blocks, separate processes for both engines and both scenes, 2,999 and 3,469 Hz, CPU 6 pinned. Quiet block starts were 39.375 and 40.000 °C; fixtures within a block ran back-to-back. No samples reported lost.

Reported values are shares of **ordered pair cycles**, not whole steps. `ordered_walk` includes the call from `orderedSelf` and its candidate callback. Remaining pair work includes source-list initialization, the outer loop, and reporting.

| Scene | Source sort | Partner-row sort | Ordered walk | Other pair work |
|---|---:|---:|---:|---:|
| `uniform` | 17.64–17.73% | 1.74–1.95% | 75.32–75.45% | 5.01–5.17% |
| `split-line-translated` | 14.13–14.24% | 2.22–2.59% | 78.72–79.48% | 4.18–4.46% |

- DWARF callsites distinguish shared `rankSort` invocations: staged `detail/quadtree-index-impl.h:2957` sorts `mOrder`; `:2987–2993` dispatches or sorts each `mRow`. Build-time rank validation is excluded. No detected sort sample had unresolved phase or callsite. The four unordered controls have no query-sort samples.
- All eight profiles match frozen fixture hashes, pair counts and checksums, candidate counts and node visits, with zero warm allocations. [Verification](verification.log) · [Per-capture attribution](attribution.csv) · [Sample counts and cycle periods](phase-periods.csv) · [Symbolized sort stacks](sort-callchains.csv.gz).
- The [older grouped-screen profile](../g9-order-sort-profile-2026-11-20/README.md#release-profile) found 12.54–12.74% source-sort share on translated. The current result confirms the opportunity on the staged executable; it is not a matched A/B between layouts. Applying these shares to the separate [unprofiled pair medians](../g2-two-scatter-thermal-2026-11-21/frozen/g9-comparison.csv) gives a rough scale of 5.4 ms translated and 5.6–5.7 ms `uniform`. These are not measured sort wall times or all removable.

## Reproduction

Use the staged two-stream source and build as `WORK/control` and `WORK/control-build`, with the pinned checkout at `WORK/box2d`. Run `bash profile.sh WORK NEW_CAPTURE_DIR`, then `python3 summarize.py NEW_CAPTURE_DIR RESULT_DIR`. The runner verifies source and full Release hashes and verifies that the symbol twin matches both [`.text` bytes](text.sha256) and [layout](text.layout). `perf` runs only the Release ELF. To reanalyze the archive, extract it with the recorded `run-order.csv` and `elf-path.txt` and a verified `staged.debug` twin.

[Raw callchains and benchmark rows](raw-callchains.tar.gz) · [Environment](environment.txt) · [Run order and Tctl](run-order.csv) · Quiet starts [1](quiet-start-1.csv), [2](quiet-start-2.csv). Power settings [before](power-before.txt) and [after](power-after.txt) match.

## Handoff

**Screen cached full-width radix sorting for the `orderedSelf` source list on two-stream scatter.** Reuse the existing partner-row radix implementation on `mOrder` for large full ordered `int32` self queries only when both reserved build buffers already cover the source list. Preserve the index-sort fallback, partner-row behavior, moved-set and cross queries, 64-bit rank order, and exact budget/predicate/stop prefixes. No new workspace requirement, rank lookahead, or unordered outline. Verify full-width ranks, insufficient scratch, and exact callback streams before a matched two-engine phase screen. Retest G2 and G9 together before promoting any combination.

Translated ordered savings of roughly 0.65–1.37 ms would give room for the unordered improvement required by G2 under the [matched-start timing budgets](../g2-two-scatter-thermal-2026-11-21/README.md#handoff). A source-only change does not close G2 by itself. G1/G2 still block 08.
