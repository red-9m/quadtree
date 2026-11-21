# 03 · Bounded rank home-bucket lookahead

**Build-phase win; not promoted to the staged control.** Ranked builds improve on the target scenes, but the frozen slice resolves no ordered full-step gain at 1,000 entries or above. G9 passes 15/15 in both repeats, yet the translated row gains no durable G9 headroom. Runtime unchanged; keep direct child initialization as the control.

The [patch](candidate.patch) starts from the staged direct-child-init build. During an eligible ranked `int32` bulk commit, prefetch each future rank's home bucket and keep its computed index in a 32-slot stack ring. Consume each home once, with a bounded tail and no reread of the home hash during probing. The [Release disassembly](rank-home-instructions.asm) confirms the home mix and `prefetcht0` in the preparation helper, and the queued index feeding the probe loop.

Eligibility: GCC/Clang, a fully ranked top-down permutation, at least 64 entries, a hash-mode table, the copy path for a trivially copy-constructible payload, and no projection. Noneligible builds use the original path. Start after `prepareReplacement`; do not prefetch before capacity and identity are committed. Keep dense order, metadata publication and rank insertion order. A probe-limit hit converts the already-live ranks, including the current one, then resumes the original commit loop at the next rank. No new allocations, reservation units, table size, hash changes or query changes.

## Matched phase screen

Two reversed nine-round blocks, `int32` at 100,000, both library engines on unchanged fixtures.

| `index-ordered` scene | Build median, block 1 / 2 (ms) | Full-step change, block 1 / 2 |
|---|---:|---:|
| `uniform` | 12.614 → 10.692 / 11.532 → 10.827 | −1.3% / +0.3% |
| `split-line` | 13.799 → 11.226 / 11.879 → 11.202 | −2.0% / −0.8% |
| `split-line-translated` | 12.710 → 11.242 / 12.290 → 11.173 | −4.4% / −2.5% |
| `mixed-size` | 12.021 → 11.730 / 11.380 → 11.168 | −1.8% / +0.0% |

- Build medians fall on all eight ordered measurements. Translated saves 1.12–1.47 ms in build; its full-step gain resolves in block 1 only, ratio 0.956 [0.918–0.995].
- Unordered full-step intervals include no change; build medians differ by less than 0.07 ms. Unordered pair medians generally fall despite unchanged traversal source; this does not isolate the cause.
- [All phases and intervals](pilot/step-change.csv) · [Verification](pilot/verification.log): all 32 rows match fixtures, results, work, allocations and footprint; zero warm allocations.

## Frozen snapshot slice

Counts are pass / fail / inconclusive. G0 cross-checks and G3/G4/G6 pass throughout; all 68 library rows per run have zero warm allocations.

| Rounds | G2 control → lookahead | G9 control → lookahead | G1 control → lookahead |
|---:|---:|---:|---:|
| 9 | 10 / 0 / 5 → 13 / 0 / 2 | 15 / 0 / 0 → 15 / 0 / 0 | 8 / 8 / 0 → 8 / 6 / 2 |
| 15 | 10 / 0 / 5 → 12 / 0 / 3 | 15 / 0 / 0 → 15 / 0 / 0 | 8 / 8 / 0 → 8 / 6 / 2 |

15-round `index-ordered`, 100,000 entries, control → lookahead, ms/snapshot. Phase medians and full-step estimates are separate.

| Scene | Build median | Pair median | Full step | G9 candidate (≤4×) |
|---|---:|---:|---:|---|
| `uniform` | 12.599 → 10.823 | 32.062 → 33.876 | 44.774 → 44.699 | 3.413 [3.294–3.537], pass |
| `split-line` | 12.660 → 11.364 | 38.558 → 38.932 | 52.331 → 50.740 | 3.714 [3.586–3.847], pass |
| `split-line-translated` | 12.202 → 11.313 | 39.180 → 39.507 | 51.684 → 50.761 | 3.838 [3.711–3.969], pass |
| `mixed-size` | 11.655 → 11.928 | 135.466 → 134.379 | 147.821 → 146.921 | 3.685 [3.574–3.799], pass |

- Translated ordered build saves **1.07 ms at nine rounds and 0.89 ms at 15**; full-step ratios are 0.985 [0.951–1.020] and 0.982 [0.951–1.015]. G9's translated upper bound moves 3.969 → 3.934 at nine rounds but 3.952 → 3.969 at 15. The latter leaves only **0.8% unordered headroom** with ordered time fixed. `uniform` loses almost all of its 15-round build gain to a higher pair median. Traversal source is unchanged; neither layout effects nor cache effects are isolated.
- `split-line` at 100,000 and translated at 10,000 move to G2 pass in both runs. Their improvements do not establish a G2 benefit from rank lookahead: unordered builds never execute the ring, and timed unordered pairs and baselines also move. The 15-round candidate retains three inconclusive rows (`split-line` 10,000; `uniform` and translated 100,000).
- [G2 phases and work](frozen/g2-comparison.csv) · [G9 phases and ratios](frozen/g9-comparison.csv) · [Matched steps and intervals](frozen/step-change.csv) · [Gate counts](frozen/gate-counts.csv) · [Verification](frozen/verification.log). All 272 library rows are allocation-free warm; all matched uncapped results and work agree. [Raw samples and run order](frozen/).

## Correctness and reproduction

The [targeted probe](checks/home-test.log) compares exact bucket/tree state, rank counters, handles and ordered stream with exceptions disabled and under ASan/UBSan. It verifies 63–65 and 95–97 entry boundaries, 257 entries, repeated warm epoch wraps, real collision conversion during commit, tree-mode rebuild, duplicate rollback and zero warmed resource requests. The [109-case build-rank probe](checks/rank-correctness.log) also passes. [Dense slots and topology](checks/dense-order.log), [unordered](checks/unordered-prefix.log) and [ordered](checks/ordered-prefix.log) exact prefixes, and [full/moved-set identities](checks/equivalence.log) match. [CTest 21/21](checks/tests.log) and [full differential fuzzing](checks/fuzz.log) pass.

Reconstruct the [direct-init control](../g2-topology-cost-2026-11-20/README.md#correctness-and-reproduction) as `WORK/control` and `WORK/control-build`. Copy the source (dereference symlinks) to `WORK/candidate`, apply `patch -p1 < candidate.patch`, and verify [control](../g2-topology-cost-2026-11-20/candidate-inputs.sha256) and [candidate](candidate-inputs.sha256) source hashes. Build `WORK/candidate-build` with GCC 16.2.1 Release, `QUADTREE_BENCH_NATIVE=OFF`, and pinned Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. Verify [`.text` hashes](text.sha256).

Run `bash verify-home.sh WORK/control WORK/candidate`, the [rank probe](../g9-build-rank-2026-11-20/verify-ranks.sh) on both source trees, and the [functional checks](../g2-fused-commit-2026-11-20/verify.sh) on `WORK`. Run the fusion report's [pilot runner](../g2-fused-commit-2026-11-20/pilot.sh) with `WORK NEW_PILOT_DIR`, then its [summarizer](../g2-fused-commit-2026-11-20/summarize-pilot.py) with the pilot directory. [Environment](pilot/environment.txt) · [Run order](pilot/run-order.csv) · [Raw samples](pilot/). For the frozen slice, run the [full runner](../g2-inline-count-2026-11-20/run.sh) with `WORK NEW_OUTPUT_DIR full`, then the [G2/G9 summarizer](../g2-cross-screen-2026-11-20/summarize.py) on that directory. Both runners pin CPU 6 and restore power settings.

## Handoff

Keep [direct child initialization](../g2-topology-cost-2026-11-20/README.md#frozen-snapshot-slice) as the reference; do not stack this ring yet. Isolate **rank prefetch versus unchanged-query timing** on the two 100,000-entry blockers. Compare the lookahead Release executable with an instruction-layout-matched no-hint diagnostic twin (same queue and hash work, replace only the audited rank prefetch with a same-length NOP); retain the unmodified direct-init executable as the end-to-end control. Measure build and pairs in matched processes, both engines; use the twin only for attribution, not for gate acceptance. Do not tune distances until the probe benefit and pair-phase tradeoff separate.

On this candidate's 15-round estimates, clearing G2 at unchanged relative uncertainty needs ~0.74 ms less unordered `uniform` time and ~0.28 ms less unordered translated time. The latter also needs ~0.68 ms less ordered time to keep G9 within 4×. These are candidate budgets, not new passes or measured improvements. G1/G2 still block 08.
