# 03 · Representative snapshots — attribute pair traversal

**86–89% of `crossPairs` calls return on empty or disjoint nodes.** Its node checks and traversal loops take 29–39% of sampled pair-phase cycles, excluding exact entry tests and reporting. Next, test an inline rejection path that avoids entering the full recursive body. No runtime or benchmark source changed.

## Timing attribution

Re-binned the [original Release callchains](../g2-phase-profile-2026-11-19/raw-callchains.tar.gz) for `uniform`, `split-line`, and `split-line-translated` at 10,000 and 100,000 entries. Each row has two independent processes. The matching [9-round baseline](../g2-phase-profile-2026-11-19/g2-budget.csv) measured 0.66–0.71 ms and 6.82–7.24 ms for pairs at those sizes. No new gate timing.

Shares of attributed pair cycle periods across the three scenes and both processes per size:

| Phase | 10,000 entries | 100,000 entries |
|---|---:|---:|
| `crossPairs` node checks and loops | 35.9–39.0% | 29.5–35.0% |
| `itemsVsSubtree` and `walk` node checks and loops | 11.1–15.2% | 10.1–13.8% |
| `selfPairs` node checks and loops | 7.1–10.5% | 5.6–8.7% |
| Exact entry tests: filters, geometry and candidate budget | 25.8–29.5% | 30.4–38.2% |
| Reporting: orientation, views, callback and checksum | 12.4–15.6% | 10.7–17.4% |

[Phase summary](pair-summary.csv), [sites](pair-sites.csv), and [PC histogram](pair-pcs.csv.gz). Ranges are per-process, not pooled. Source mapping and sampling skid limit instruction attribution: for example, GCC attributes `0x9da38` to the filter-rejection line, but the instruction is an epilogue jump. Do not treat the node-check bucket as an isolated bounds-test cost or the full share as removable.

## Call census

A [count-only probe](census.sh) uses the same singleton-snapshot fixtures (`batch=1`), `LibraryIndex`, and sink checksum. It instruments temporary headers, not the timed binary. `crossPairs` rejects 80.5–84.8% of calls on disjoint bounds after charging a node visit; another 1.3–8.5% return on empty nodes without a charge. Filter summaries reject none of these fixture calls. Roughly 85–87% of calls come from the recursive child-pair loop, not the sibling pairs submitted by `selfPairs`.

[Exact counts](census.csv) partition cross calls by outcome and reconcile charged visits across `selfPairs`, `crossPairs`, and `walk`. All six census rows match retained candidate counts, node visits, pair counts and checksums. All 12 source profiles also match retained fixtures and work; both probes report zero warm allocations. See [verification](verification.log).

**Next:** stage a small inline `crossPairs` entry gate and outline the body for surviving node pairs. Preserve charge-before-prune, filter rejection counts, recursion and callback order, and `BudgetExhausted`/`Stopped` behavior, including `Instrumented=false`. Confirm in disassembly that disjoint pairs bypass the recursive body; check exact callback streams and limited-budget prefixes before timing the full slice. Keep the staged key patch separate for the first comparison.

## Reproduction

Use the retained [source, Release ELF, and debug twin](../g2-phase-profile-2026-11-19/README.md#reproduction). Verify the recorded `.text` hash and layout before symbolizing. Extract `raw-callchains.tar.gz` into a fresh capture directory and run from the repository root:

```sh
bash bench/results/g2-pair-profile-2026-11-19/census.sh > /tmp/census.csv
python3 bench/results/g2-pair-profile-2026-11-19/summarize.py /path/to/captures /tmp/qt-g1-direct-result.gXB9Jz/control-bench /path/to/debug-twin /path/to/outdir
```

The Release argument is the DSO path stored in the captures, not a new executable path. For new captures pass the new recorded DSO path. [Environment and run order](../g2-phase-profile-2026-11-19/README.md#reproduction) are unchanged; the census does not produce timing evidence.
