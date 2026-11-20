# 02 · Small snapshots — split warm replacement at 2 and 4 entries

**The largest replacement hotspot is at the identity call boundary, not dispatch or the entry-store loops.** The generated code spills and reloads live state around `acquireModuleIdentity()`. `perf` identifies the region, not the cost of a particular instruction. No runtime or benchmark source changed.

## Replacement split

The unmodified Release `index` ran the actual batched `runStep` for `uniform` and `clustered`, 2 and 4 entries, 256 scenes per batch. The [phase CSV](phase-summary.csv) combines two archived cycle profiles and one fresh cycle profile per scene and size. A separate fresh run sampled retired instructions. All used the same [machine code](text.sha256).

| Region | 2 entries (% of replacement) | 4 entries (% of replacement) |
|---|---:|---:|
| `buildRange` + `buildPrepared` dispatch and workspace guard | 16.6–18.7 | 17.1–21.0 |
| Rank check, geometry validation and extent union | 10.1–13.0 | 16.0–19.7 |
| Identity acquisition function | 8.1–10.3 | 5.5–7.8 |
| Identity call setup, result check and reloads | 40.4–43.0 | 30.7–36.6 |
| Entry commit: reset, extent, store loops and final publication | 18.5–20.2 | 22.2–24.1 |
| Entry-store loops alone (part of commit) | 14.3–16.8 | 20.7–22.6 |

Replacement accounts for 36.6–38.1% of attributed step cycle weight at two entries and 28.4–31.2% at four. Dispatch and entry commit each account for about 5–8% of the whole step. These are period-weighted sample shares, not elapsed phase times or speedup estimates. The [PC histogram](replacement-by-pc.csv.gz) records sample counts, weights, inline frames and bucket assignments.

### The hotspot after identity acquisition

Source attribution alone places `0xe3d3f` on `mEntries.resetSlots()`. The [disassembly](identity-boundary.asm) shows `mov 0x20(%rsp),%r10`, a reload of the entry count after the identity call. The first actual rank-capacity load follows at `0xe3d44`. The reload PC alone receives 13.7–14.6% of attributed step cycle weight at two entries and 8.5–10.3% at four. It also receives 11.4–11.7% and 8.2–9.2%, respectively, of retired-instruction sample weight. That concentration in both events is evidence of imprecise delivery; it does **not** show that the reload, the CAS or `resetSlots()` itself takes that share of the step.

All eight [fresh runs](profile-run-results.csv) matched the unchanged gate run's pair counts, checksums, fixture hashes, candidate tests and node visits; warm allocations stayed zero. No new gate verdicts. The [control repeat](../g1-two-entry-dispatch-2026-11-19/45-control/gates.txt) remains 10 pass, 6 fail.

**Next:** stay on 02. Test inlining the existing saturating identity acquisition only on the flat prepared path in a staged build. Inspect whether the call/reload sequence disappears, then check the full G1 matrix. Keep fresh owner issuance, exhaustion behavior and transactional guarantees unchanged. Do not infer a gain from moving samples alone.

## Reproduction

Use the unchanged Release binary and byte-identical `-g` twin from the [batched runner profile](../g1-runstep-profile-2026-11-19/README.md#reproduction). Run `bash bench/results/g1-replacement-profile-2026-11-19/profile.sh /path/to/release/quadtree-bench /path/to/debug/quadtree-bench /fresh/outdir 6` from the repository root. The script requires matching section offsets and the recorded `.text` hash before using the PC range. It keeps the raw perf data and re-bins the earlier runner histogram alongside the fresh samples.

Environment: Ryzen 9 5900HS, pinned CPU 6; GCC 16.2.1; Linux 7.3.0-1-custom; perf 7.1.8; `powersave` governor, `power` energy preference, HPET clock source. Release flags: `-O3 -DNDEBUG`, no native/LTO. Fresh `cycles:u` and `instructions:u` profiles each sampled at 5477 Hz in separate processes.
