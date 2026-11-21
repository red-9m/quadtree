# 02 · Refresh G1 on the packed cross-item staged control

**G1 remains 8/16.** Deferred tiny-build extent improves unordered two- and four-entry steps, but clears no failed row and repeatedly slows ordered steps. Reject it; the packed cross-item staged control and task 08 blocker are unchanged.

## 1. Profile the exact staged ELF

[Phase split](profile/phase-summary.csv) · [Sampled PCs](profile/period-by-pc.csv.gz) · [Raw perf records](profile/raw-perf.tar.gz) · [Environment](profile/environment.txt). The executable matches the packed cross-item `.text` checksum; the debug twin has byte-identical `.text` and offsets. Sample the real batched `runStep` with 256 scenes per batch in two replicates, reversing scene/size/engine order and using 4999/5477 Hz `cycles:u`. One scene/size/engine per process, so these times are diagnostics, not gate verdicts. Warm library allocations are zero. The profiler's `replacement` bucket includes the inlined warm build; `pair dispatch + view` includes the result boundary and view materialization. Sample periods are not elapsed phase times.

| Entries | `index` ns/scene | Adapter input | Replacement | Pair dispatch + view | Runner aggregation |
|---:|---:|---:|---:|---:|---:|
| 0 | 10.1–10.9 | 5.8–6.2% | 3.7–4.2% | 9.3–11.1% | 67.3–70.8% |
| 1 | 18.8–19.8 | 6.2–6.6% | 42.7–44.4% | 5.4–5.9% | 37.9–41.5% |
| 2 | 33.6–34.2 | 5.0–6.3% | 32.3–35.1% | 36.6–40.1% | 14.2–15.7% |
| 4 | 51.6–55.3 | 6.3–7.0% | 24.2–26.3% | 42.4–43.1% | 9.1–9.6% |

Both empty rows failed in the preceding 45-round full slice, whereas both were inconclusive at nine and 15. Empty replacement has little sampled weight; the frozen counter aggregation is not an optimization target. Singleton replacement is the largest library-controlled bucket. The earlier `optional::emplace` publication change did not resolve G1; the candidate below removes the build-time extent union/publication rather than changing the optional assignment or inlining another query wrapper.

## 2. Test one isolated library candidate

[Patch](candidate.patch): defer extent calculation on the existing warm contiguous, unranked 1–4-entry integer flat-build path. Geometry and rank checks, identity acquisition and every commit/output store except extent publication remain. Disengaged extent with live entries denotes a deferred union with at most four boxes; `extent()` resolves it without modifying const state. Mutation uses the resolved bound; removal materializes it before changing membership so it retains the old extent even when emptied. Move, shrink, fixed extent, rank fallback and failed build preserve their contracts. No adapter, runner, brute, solver, public layout or fixtures change. The candidate can alter cold or large code generation despite its narrow logical scope; a G1 pass would justify a joint regression gate, not immediate runtime promotion.

[Extent/handle probe](extent-check.cpp) exercises 24 combinations against both binaries: sizes 1–4; Adaptive, Fixed and both Snapshot policies; replacement rollback, handles, move, update, insertion, shrink, removal and empty rebuild. [Probe log](extent.log) · [ASan/UBSan](extent-sanitized.log) · [CTest](tests.log) (21/21). The [exact-pair probe](exact-pairs.log) compares unordered pair identities against brute at all 16 rows and verifies zero warm allocations.

## 3. Verify the full G1 slice

| Rounds | Control (pass / fail / inconclusive) | Candidate |
|---:|---:|---:|
| 81 | 8 / 8 / 0 | 8 / 8 / 0 |
| 117 | 8 / 8 / 0 | 8 / 8 / 0 |

117-round full-step estimates (ns/scene), control → candidate. The last column shows the span of both candidate scene intervals; each row's exact interval is in the CSV.

| Entries | `clustered` | `uniform` | Candidate G1 95% bounds (limit 2×) |
|---:|---:|---:|---:|
| 0 | 10.6 → 10.5 | 10.6 → 10.6 | 2.028–2.195, both fail |
| 1 | 19.2 → 19.0 | 19.2 → 19.4 | 2.842–3.134, both fail |
| 2 | 33.8 → 32.8 | 35.2 → 33.4 | 2.696–3.018, both fail |
| 4 | 58.5 → 53.6 | 61.0 → 57.6 | 2.521–2.764, both fail |

- Both empty control rows fail twice, resolving the earlier uncertainty. Unordered 0/1 changes do not resolve. Unordered 2 saves 1.0–6.1 ns and 4 saves 3.4–4.9 ns across the matched pairs. Matched control-to-candidate full steps are useful even when the G1 ratio moves the other way; two-entry brute is also faster in the 117-round candidate process.
- Ordered 4 regresses by 6.1–6.5% in both scenes and comparisons; ordered `uniform` 32 and 64 also regress twice. These are full-binary effects, not direct timing of extent calculation. No advancement to the joint G2/G9 screen.
- [G1 verdicts and work](frozen/g1-comparison.csv) · [Matched step intervals](frozen/step-change.csv) · [Raw data and gates](frozen/) · [Verification](frozen/verification.log). All 192 matched engine rows agree on fixtures, work, checksums, allocations and footprint. Both engines have zero warm allocations across all sizes.

## Reproduction

Use the [exact staged packed cross-item tree](../g2-packed-cross-item-2026-11-21/README.md#checks-and-reproduction) and its Box2D checkout. `bash profile.sh STAGED_WORK OUT` reproduces diagnostics. For the matched screen link its candidate and Release build as `WORK/control` and `WORK/control-build`. Copy the tree as `WORK/candidate`, apply `candidate.patch`, and rebuild at `WORK/candidate-build` with GCC 16.2.1 Release, `QUADTREE_BUILD_BENCHMARKS=ON`, `QUADTREE_BUILD_TESTS=ON`, `QUADTREE_BENCH_NATIVE=OFF`, `QUADTREE_BENCH_BOX2D=ON`, pinned `WORK/box2d`. Invoke `bash run.sh WORK NEW_OUTPUT_DIR` and `python3 summarize.py NEW_OUTPUT_DIR` with the raw-profile archive available. Inputs and `.text` are verified by the runner. CPU 6 and 81 rounds control → candidate, then 117 candidate → control with launch ≤40 °C and within 1 °C in each pair; existing frozen batch and error term. Power restored.

## Handoff

**02 remains open; 08 stays blocked.** The staged binary still needs about 0.6 / 5.8–6.0 / 8.8–10.0 / 16.5–17.8 ns/scene at 0 / 1 / 2 / 4 entries just to reach the 117-round 2× point estimates; intervals require more. Don't repeat the extent, result-copy or query-wrapper variants. Before another candidate, identify a contract-preserving change with a measured path to the singleton budget and a separate explanation for how the empty return can improve under the unchanged adapter and runner. Do not change fixtures, baselines, or limits to close G1.
