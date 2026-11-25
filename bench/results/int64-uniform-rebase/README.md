# Task 09 — `int64` uniform 100,000 snapshot screen

**Target row passes in the local two-process screen, not a release sign-off.** Control is `6a7fe81`; the working-tree candidate is [final/candidate.patch](final/candidate.patch) (see [hashes](final/source-binary.sha256)). Same benchmark, baseline engines, scenes, thresholds, and frozen 2× G2 limit. Release GCC 16.2.1 on CPU 6, Ryzen 9 5900HS. Local `powersave` governor / `power` energy preference throughout; not the historical release run's power/quiet-start protocol. See [environment](environment.txt), [rule](resolution-rule.md), and [command](screen.sh).

| Source | Pooled G2 ratio vs grid (95% interval) | Verdict |
|---|---:|---|
| Control | 2.114 [2.072–2.158] | fail |
| Narrow unordered pair kernel | 1.934 [1.893–1.976] | pass |
| Same + 12-byte bulk placement records | 2.202 [2.157–2.249] | fail; rejected |
| Final: narrow ordered and unordered full pairs | 1.955 [1.915–1.996] | pass |

`python3 pool.py control pair combined final` reproduces the intervals in this directory. Each process has 45 interleaved rounds; both final processes separately straddle 2×. Do not discard either. The control `index` step estimate was 13.99 ms in both processes, final 12.88–12.92 ms (~8% less time). Pair medians fell from 7.40–7.45 ms to 6.30 ms; build medians stayed near 6.6 ms. Compact build records pushed the build to 8.24 ms, so they are not in the final source. Raw samples, process summaries, gate verdicts and experimental diffs are in the variant directories.

**Mechanism:** check actual subtree bounds once before full pair enumeration. If both axis spans fit `INT32_MAX`, all opposing endpoint differences fit signed 32 bits even in translated worlds near `INT64_MIN` or `INT64_MAX`. SSE2 subtracts low words modulo 2³² to obtain exact signed gaps; zero gaps still use the library's boundary and degenerate-box rules. Geometry, node placement, storage and public views remain 64-bit. No new allocations, cached state or insert/update synchronization. Wide spans, custom predicates and non-SSE2 builds retain the original path.

**Verification:** all 22 Release CTests passed, including exception-disabled tests. `translatedInt64Snapshots()` checks exact unordered/ordered pair identities, filters, handles, geometry, degenerates and boundary contacts across translations and at the span cutoff, then extends the world beyond the cutoff by insertion. On the target, both final processes match control: 788,738 candidate tests, 73,905 pairs, checksum `47126aad363c014b`, zero warm allocations; G9 passes.

**Regression screen (not a new release matrix):**
- `int64`: [5-round matrix](final/int64-matrix.gates) G2 19 pass, 1 inconclusive (the target, separately passed by the preregistered screen). Former `split-line-translated` n=100 fail now 3.633 [3.483–3.790], limit 4. G9 15/15; G3/G4/G6 pass. G1 at 0–4 remains open (six failures and two inconclusive; no exception for `int64`).
- `int32`: [5-round matrix](final/int32-matrix.gates) G2 20/20 and G3/G4/G6 pass. G9 14/15, one inconclusive; `clustered` n=1 G1 upper 3.980 outside the 3.5 exception cap. In the prespecified [follow-up](final/int32-followup/) on these two rows, pooled G9 `mixed-size` n=100000 3.853 [3.775–3.932] passes; G1 `clustered` n=1 2.946 [2.892–3.001] fails the normal limit but meets the existing exception cap. Other small rows stay inside the cap.
- Full persistent, pathological, portability, and release-run environment rechecks remain outstanding. Keep the change as a working-tree candidate. Next: official full `int64` and `int32` recheck before promotion.
