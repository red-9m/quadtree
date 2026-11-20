# 02 · Small snapshots — profile the retained 1/2-entry runner

**At two entries, pair-result handling attracts more cycle samples than dispatch and geometry.** At one entry, replacement and the runner's counter aggregation dominate. No runtime, harness, or brute source changed.

## G1 budget

The independent [99-round repeat](99-control/gates.txt) of the unchanged G1 matrix returned **10 pass, 6 fail**. Both empty rows passed; both scenes still failed at 1, 2 and 4 entries. On the point estimates, the one-entry rows need 5.7 ns/scene less to reach 2× brute; the two-entry rows need 8.8–9.7 ns/scene less. These are [point-estimate gaps](g1-budget.csv), not the savings required for the full confidence interval to pass. The run directory contains estimates, frozen intervals and raw timings for all 16 rows.

## Actual batched `runStep`

Shares of attributed step cycle periods across two repeats, `uniform` and `clustered`, on the retained Release binary:

| Phase | 1 entry | 2 entries |
|---|---:|---:|
| Input adapter | 6.3–8.0% | 4.9–5.4% |
| Index replacement | 40.1–41.1% | 33.0–34.6% |
| Pair dispatch and traversal, excluding views/result | 4.3–5.2% | 14.1–14.9% |
| Pair-result copy and `Counters` construction | 2.5–3.4% | 18.1–21.3% |
| Pair views | 0% | 2.2–3.6% |
| Runner counter aggregation | 38.5–40.7% | 14.8–15.2% |

[Full phase split](phase-summary.csv) includes checksum, loop/timer, replacement subphases, and separate retired-instruction and store-to-load-interlock (STLI) samples. Attributed step PCs account for 73–74% of user-cycle periods. Samples are not elapsed phase times or estimates of removable time.

- At one entry, `runStep` loads two separately stored `Counters` fields with a single 16-byte load (`0xe56d0`). That PC attracts 30.6–31.9% of attributed step cycle weight and 32.5–33.7% of attributed STLI weight. The frozen runner and `Counters` transfer are not optimization targets.
- At two entries, the `QueryResult` copy inside `LibraryIndex::pairs` loads 16 bytes at `0xd3448`, spanning the separately initialized status byte and part of the counter block. That PC attracts 9.2–12.2% of cycle weight and 8.7–10.9% of STLI weight. The [disassembly](hotspots.asm) shows this is before the adapter returns `Counters`, not the geometry loop. This is evidence to test eliminating the intermediate result, not a measured store-forwarding latency.
- After the inlined CAS, samples concentrate at `0xe3cdb`, the branch after the `mExtent` store: 18.0–20.0% of step cycle weight at one entry and 10.3–11.6% at two. Retired-instruction samples concentrate there too. Skid prevents charging those samples to the extent assignment, slot reset, or identity CAS. The [earlier extent experiment](../g1-singleton-phases-2026-11-19/README.md#full-g1-step) did not improve G1.

All 20 [profile runs](profile-run-results.csv) and the G1 repeat matched retained fixture hashes, pair counts, checksums and traversal counters. Both library engines reported zero warm allocations in G1. The [fresh exact-pair probe](exact-pair-probe.log) matched brute identities and found zero warm allocations in all four focused rows. Build/failure coverage is [unchanged](../g1-flat-path-identity-inline-2026-11-19/tests.log).

**Next:** stay on 02. Stage a library-only `Index::forEachPair` result-construction variant. Verify that it removes the intermediate `QueryResult` copy before running the full G1 matrix. Preserve pair dispatch, counters, layouts, pair identities, the adapter and the frozen runner; do not retest single-candidate dispatch or store fusion.

## Reproduction

Use the [retained source and Release recipe](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction), verifying the control input hashes. From the repository root run `bash bench/results/g1-retained-runner-profile-2026-11-19/profile.sh /path/to/build /path/to/box2d /fresh/outdir 6`. The script samples the Release executable and uses a `-g` twin with byte-identical [`.text`](text.sha256) and matching [section layout](release.layout) for symbolization. It keeps the 99-round G1 timing separate from the single-engine diagnostic runs. The [raw perf archive](raw-perf.tar.gz) contains each `.data` capture and its benchmark CSV; the [PC histogram](period-by-pc.csv.gz) records sample counts, periods and inline frames.

See [environment](environment.txt). Sampling: `cycles:u` at 4999 and 5477 Hz with reversed scene/size order; `instructions:u` at 5477 Hz; `ls_bad_status2.stli_other:u` at period 2047 in separate `index` and `brute` processes. Reuse the [exact-pair probe recipe](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction) with the retained source for `uniform` and `clustered`, sizes 1 and 2.
