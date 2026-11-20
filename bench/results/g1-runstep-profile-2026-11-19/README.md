# 02 · Small snapshots — profile the actual batched `runStep`

**The hot path changes with size.** In `index`, empty and singleton steps concentrate samples around the `Counters` return and aggregation. At 2 and 4 entries, replacement and pair dispatch dominate together. No runtime, benchmark, or gate code changed.

## Results

The unmodified Release benchmark ran one engine, scene and size per process to isolate the actual `Runner::step`/`runStep` used by G1. Both replicates used the same 256-scene batch and seed as G1. The second reversed run order and changed sampling frequency. These single-engine profiles are diagnostics, not gate runs.

| Entries | `index` ns/scene | Input adapter | Replacement | Pair dispatch + views | Aggregation alone |
|---:|---:|---:|---:|---:|---:|
| 0 | 10.2–10.9 | 5.7–7.0% | 3.7–4.3% | 10.2–11.2% | 65.5–69.1% |
| 1 | 19.9–21.4 | 7.0–8.3% | 43.8–45.3% | 5.1–6.3% | 36.7–39.3% |
| 2 | 35.9–36.9 | 5.3–5.8% | 36.6–38.0% | 33.9–35.2% | 13.6–15.2% |
| 4 | 55.2–58.7 | 6.7–7.3% | 28.4–31.2% | 37.5–40.0% | 7.9–11.1% |

Ranges cover `uniform` and `clustered` in both replicates. Percentages are shares of attributed user-cycle sample periods, not elapsed phase times. The [phase CSV](phase-summary.csv) separates returned-counter aggregation, checksum and loop/timer; at four entries, out-of-line `Index::view` accounts for 9.3–13.1%. The [PC histogram](period-by-pc.csv.gz) records weights, inline stacks and phase assignments. Attributed PCs account for 95.8–98.8% of `index` process periods but only 66.8–85.7% of `brute` periods: outlined vector helpers and libc copies lack sampled caller context and are not bucketed. Do not compare the engines' phase percentages as absolute costs.

### Empty and singleton counter transfer

On this GCC build, `LibraryIndex::pairs` is out of line and returns `Counters` through stack storage. It writes `candidates` and `references` as separate 8-byte stores (`0xd3520` and `0xd352b`). Back in `runStep`, `movdqa 0x310(%rsp),%xmm2` at `0xe57d0` reads both as a single 16-byte load immediately before the aggregation arithmetic. That PC alone accounts for 49% of sampled periods in the uniform empty run and 28% in the uniform singleton (replicates pooled). The `brute` pair loop is inlined and has no equivalent out-of-line `Counters` return in this hot loop.

A second [Zen 3 store-to-load interlock profile](stli-samples.tar.gz) placed 63.9% of event weight at that same PC for empty `index` and 34.8% for singleton `index`. This supports a forwarding bottleneck at the return/consume boundary; skid and compiler scheduling prevent assigning exact time to the load. A singleton replacement hotspot similarly lands just after the identity call, before the rank-capacity test; that is not a direct measurement of `resetSlots`. The [earlier removal probes](../g1-singleton-phases-2026-11-19/README.md#isolation) remain better evidence for the cost of the identity CAS and extent publication in isolation.

All 32 [profile runs](profile-run-results.csv.gz) returned `ok`, matched pair counts, checksums and fixture hashes across engines and replicates, and had zero warm allocations. No new gate verdicts; the [last unchanged repeat](../g1-singleton-phases-2026-11-19/45-control/gates.txt) still has 8 pass, 6 fail and 2 inconclusive. Runtime source and its [16 passing CTest checks](../g1-singleton-phases-2026-11-19/tests.log) are unchanged.

**Next:** stay on 02. Test a library-only single-candidate path for exactly two entries inside `Index::tinyPairs`, then compare complete build-plus-pairs across all 16 G1 rows. Preserve filters, contact, predicate/view identity, Stop and counters; 0 and 8–64 are regression checks. Do not optimize the frozen harness to remove the counter transfer.

## Reproduction

Reconstruct the [retained source](../g1-view-construction-2026-11-19/README.md#reproduction) and build the [baseline Release binary](../current-snapshot-2026-11-18/README.md#environment-and-reproduction). Run `bash bench/results/g1-runstep-profile-2026-11-19/profile.sh /path/to/build /path/to/box2d /fresh/outdir 6 stli` from the repository root. Omit `stli` if the host lacks `ls_bad_status2.stli_other`. The script samples the Release binary, then symbolizes via a `-g` twin with byte-identical [`.text`](text.sha256) and matching section offsets. Attribution uses perf DSO offsets and `addr2line` inline frames. The script retains the raw perf data and benchmark CSVs.

Environment: Ryzen 9 5900HS, CPU 6 (SMT sibling CPU 7); GCC 16.2.1; Linux 7.3.0-1-custom; perf 7.1.8; `powersave` governor, `power` energy preference, HPET kernel clock source. Release flags: `-O3 -DNDEBUG`, no native/LTO, pinned Box2D `v3.2.0` (`178495ba35695aefe9c321e569d2277f06db3b90`). `cycles:u` at 4999 and 5477 Hz; optional `ls_bad_status2.stli_other:u` at period 8191.
