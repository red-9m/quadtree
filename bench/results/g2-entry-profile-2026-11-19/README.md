# 03 · Representative snapshots — profile bulk entry commit

**Target the spatial-order input gathers, not dense stores or count publishing.** At 100,000 entries, 89–93% of sampled entry-commit cycle weight lands immediately after the descriptor rank-flag or indirect payload loads. Precise AMD IBS samples identify those input accesses as cache misses; stores and bookkeeping are mostly L1 hits. No runtime change or new gate timing.

## Attribution

Re-binned the retained [cycle callchains](../g2-phase-profile-2026-11-19/raw-callchains.tar.gz) for unordered `int32` `uniform`, `split-line`, `split-line-translated` and `clustered` at 100,000. Entry commit is 21–25% of attributed build cycles. The [disassembly](entry-commit.asm) shows a sequential key read, an indexed 56-byte descriptor lookup, a dependent load through `descriptor.payload`, then sequential box, payload, meta and slot stores. The caller increments `mSize` and `mSlotsUsed`, checks the cached rank flag and rewrites `key.entry`. No ranks or output handles are published on these rows.

| Cycle sample site | Share of entry commit, across eight profiles |
|---|---:|
| `0x13d00f`, first instruction after indirect payload load `0x13d00c` | 68–76% |
| `0xe3795`, first instruction after descriptor flag load `0xe3791` | 13–21% |
| Dense payload/box/meta/slot store PCs | 0.8–2.2% |
| Count increment, rank branch and key rewrite PCs | 0–1.3% |

[Cycle summary](cycle-summary.csv) · [Per-PC period and caller](cycle-sites.csv). Peaks at successor PCs are consistent with sampling skid or dependency stalls; they do not measure individual load or store latency. The two peaks total 89–93% within each run, not the sum of the independent range endpoints.

## Precise memory samples

Two new IBS profiles per scene, using the same unbatched Release runner. Ranges are shares of sampled ops at the named instruction sites, not overall cache miss rates or estimates of removable time.

| Operation | IBS L1 hit | IBS RAM hit |
|---|---:|---:|
| Sequential key index load | 97–100% | 0% |
| Indexed descriptor loads | 1–4% | 13–35% |
| Indirect payload load | 0–2% | 45–71% |
| Dense entry stores | 98–99% | 0% |
| Bookkeeping loads / key rewrite stores | 99.6–100% / 98–100% | at most 0.12% / 0% |

Most non-RAM input gather samples are decoded by `perf` as `core, same node Any cache hit`. P90 of nonzero reported load-miss latency is 375–545 cycles for descriptor loads and 435–578 for the payload load. Store hit labels do not measure store-buffer stalls or writeback bandwidth. [IBS summary](ibs-summary.csv) and [focused raw ops](ibs-commit-ops.csv.gz) retain DSO-relative PCs, decoded levels and reported miss latencies.

[Verification](verification.log): all eight archived cycle rows and eight new [IBS rows](ibs-run-results.csv) match retained fixture hashes, pair counts, checksums, work and footprint, with zero warm allocations. The Release and debug twin [`.text` bytes](text.sha256) and [layout](text.layout) match. The profiled timings are not gate runs; retained G2 remains 8 pass / 6 fail / 1 inconclusive. Even the entire entry-commit phase is smaller than the 7.7–8.8 ms full-step gap on the three failing 100,000-entry rows.

**Next:** isolate one bounded descriptor/payload lookahead prefetch candidate for the spatial bulk commit. Keep sorted dense order, payload construction, handle mapping and publication unchanged. Compare exact outputs and the frozen full-step slice against retained code before any combination with the staged key hoist.

## Reproduction

Use the [retained Release and debug twin recipe](../g2-phase-profile-2026-11-19/README.md#reproduction); verify the hashes and layout above. Pin to CPU 6 on the same Zen 3 machine with all energy preferences and platform profile set to `performance`. From the repository root:

```sh
here=bench/results/g2-entry-profile-2026-11-19
work=$(mktemp -d)
mkdir "$work/cycles" "$work/report"
tar -xzf bench/results/g2-phase-profile-2026-11-19/raw-callchains.tar.gz -C "$work/cycles"
bash "$here/profile-ibs.sh" /path/to/control-bench "$work/ibs"
python3 "$here/summarize.py" "$work/cycles" "$work/ibs" \
  /tmp/qt-g1-direct-result.gXB9Jz/control-bench /path/to/debug-twin "$work/report"
```

The first binary path passed to `summarize.py` is the archived DSO identifier, not a file it opens. [Environment](environment.txt) · [IBS run order](run-order.csv). The IBS command requests dispatched-op periods 16,384 and 32,768 with reversed order. `perf record` warned that `/proc/schedstat` could not be opened, but recorded samples and exited 0 on all eight runs.
