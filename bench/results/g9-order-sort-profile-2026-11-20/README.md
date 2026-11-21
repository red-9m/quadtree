# 03 · Separate source and partner-row rank sorting

**Target long partner rows on `mixed-size`.** They account for 75.3% of row-sort comparisons. All partner-row sorting takes 26.75–27.68% of sampled pair cycles, versus 3.29–3.48% for the source sort. No runtime source or gate verdict changed.

## Release profile

`index-ordered`, 100,000 `int32` entries, with the [grouped child screen](../g2-grouped-child-screen-2026-11-20/README.md) applied. Ranges span three independent processes per scene. The same introsort body serves both call sites; DWARF callchains distinguish the source sort from the row sort. Build-time rank validation is excluded. No identified pair-sort sample lacked a resolved site.

| Scene | Source sort (% of pair cycles) | Partner-row sort (% of pair cycles) | Partner rows (% of resolved sort cycles) |
|---|---:|---:|---:|
| `mixed-size` | 3.29–3.48 | 26.75–27.68 | 88.5–89.4 |
| `split-line-translated` | 12.54–12.74 | 1.85–2.09 | 12.7–14.1 |

[Attribution](attribution.csv) · [Phase periods](phase-periods.csv) · [Symbolized sort callchains](sort-callchains.csv.gz) · [Raw callchains and benchmark rows](raw-callchains.tar.gz). Sampled user-cycle shares are not wall times or guaranteed savings.

## Exact comparison census

- `mixed-size`: source sort compares 2.09 million times; row sorts compare 17.45 million times. The 1,190 rows of length ≥65 contain 47.8% of partners and account for 13.14 million comparisons (75.3% of row-sort comparisons). Counts do not attribute cycles by row length.
- `split-line-translated`: source sort compares 2.10 million times; rows compare 155,571 times. No row exceeds 64 partners. A long-row-only change will not target this scene's source sort.
- [Census](census.csv) · [Verification](verification.log). All six Release profiles match the screened gate rows' fixture IDs, pair checksums, candidate counts and node visits, with zero warm allocations. Both count-only runs match the results and work; their diagnostic binaries were not timed.

## Reproduction

Reconstruct the [screened candidate](../g2-grouped-child-screen-2026-11-20/README.md#reproduction-and-handoff). Use CPU 6 and the recorded [environment](environment.txt) and performance settings; [restore the platform profile](restored-mode.txt) afterward. Run `bash profile.sh SCREENED_SOURCE SCREENED_BUILD BOX2D_SOURCE NEW_CAPTURE_DIR`. The profiler checks the timed Release hash and byte-matched symbol twin ([hashes](text.sha256) · [layout](text.layout) · [run order](run-order.csv)).

Run `bash census.sh SCREENED_SOURCE > RESULT_DIR/census.csv`, then `python3 summarize.py CAPTURE_DIR RELEASE_ELF DEBUG_TWIN RESULT_DIR`. To reanalyze archived captures, use the release DSO path recorded in `environment.txt` for `RELEASE_ELF`.

## Handoff

Stage a long-row sort on top of the screen: cache each partner's full 64-bit rank into contiguous scratch before sorting, keep the short-row path and source sort unchanged, and account for scratch in the workspace reservation. Compare exact ordered callbacks and finite-budget prefixes before full-step timing. The screened `mixed-size` G9 upper bound is 4.337× against 4×; it needs about 8% less ordered step time if the denominator and uncertainty stay fixed. Retest G2 and G9 together: an unordered gain tightens G9. Release acceptance remains blocked.
