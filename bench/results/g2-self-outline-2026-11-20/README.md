# 03 · Outline same-node unordered pair qualification

**Pair-phase gain reproduced, not promoted.** Outlining saves 0.26–0.45 ms on the three target unordered pair phases at 100,000. No full-step interval resolves a gain; skipping the emitted tail jump does not recover one. Runtime unchanged; direct child initialization remains staged.

The [candidate patch](candidate.patch) starts from direct child initialization alone. In the own-entry loop of `selfPairs`, only `int32` with the default predicate calls an outlined `selfPairQualifies`. That helper calls the existing `pairQualifies` with `ordered=false`; successful pairs use the original `reportPair`. No charges, bounds tests, filter decisions, callback order or topology change. Custom predicates, other coordinate types, cross-node callbacks, and ordered traversal keep their original source paths. No rank lookahead.

The [GCC Release symbols](symbols.csv) show unordered `selfPairs` shrinking from **1,956 to 1,267 bytes** and gaining [two calls](self-pair-calls.csv). The outline helper is a [five-byte tail jump](helper-tail.asm) to the existing `pairQualifies`. This reproduces the call boundary seen in the [rank hint ablation](../g9-rank-hint-ablation-2026-11-20/README.md), with an extra jump. The measured unordered `selfPairs` and `crossPairs` keep their starting offset modulo 64, but other symbols (including `walkOrdered`) move; this is not a whole-binary layout match.

## Matched phase screen

Two reversed nine-round blocks, `int32` at 100,000, both library engines. Figures are unordered pair medians in ms and matched full-step changes.

| Scene | Pairs, block 1 | Pairs, block 2 | Full-step change, blocks 1 / 2 |
|---|---:|---:|---:|
| `uniform` | 7.331 → 6.954 | 7.327 → 6.880 | −2.2% / −3.1% |
| `split-line` | 7.409 → 7.107 | 7.385 → 7.100 | −2.8% / −2.4% |
| `split-line-translated` | 6.989 → 6.703 | 6.932 → 6.674 | −3.0% / −0.9% |
| `mixed-size` | 33.757 → 33.727 | 33.848 → 33.430 | −0.7% / −1.1% |

- The three target unordered pair medians improve by ~0.26–0.45 ms, with build medians within 0.022 ms of control. This reproduces the magnitude and direction of the [prior unordered phase movement](../g9-rank-hint-ablation-2026-11-20/README.md#ordered-results) without rank lookahead. It tests outlining as a source change, not the isolated effect of code size.
- No full-step interval excludes no change in either engine. For `uniform`, the 3.1% block-2 unordered point gain has a ratio interval of [0.9387, 1.0003], still inconclusive. Ordered `uniform` and translated point estimates increase in both blocks. [All steps, phases and intervals](pilot/step-change.csv) · [Pilot verification](pilot/verification.log).

## Same-layout direct-call diagnostic

The [diagnostic twin](make-direct.py) changes only the relative displacements of the two calls in unordered `selfPairs`, routing them directly to the same `pairQualifies` body. The thunk and all addresses are unchanged; the [whole-ELF check](direct-verification.log) finds four changed bytes. No runtime source or additional instruction changes. This comparison isolates the cost of the tail jump within the outlined binary, not outlining versus inline source.

- The translated unordered pair median drops another 0.08–0.09 ms, but `uniform` shows at most 0.04 ms; no unordered full-step interval resolves a gain. [Diagnostic phases and intervals](direct-screen/step-change.csv) · [Verification](direct-screen/verification.log).
- Ordered `mixed-size` is ~5.2% slower in both diagnostic blocks even though the changed call sites do not execute on that row. Ordered translated improves only in block 1. The ordered negative control therefore shows substantial timing variation unrelated to executing the modified calls; do not use this run to claim an ordered effect or resolve G9.
- No frozen-slice rerun. The [latest matched direct-init control](../g9-home-lookahead-2026-11-20/frozen/15-control/gates.txt) remains G2 10 pass / 0 fail / 5 inconclusive, G9 15/15.

## Correctness and reproduction

The source candidate matches exact [unordered](checks/unordered-prefix.log) and [ordered](checks/ordered-prefix.log) budget/predicate/stop prefixes, [dense slots and topology under ASan/UBSan](checks/dense-order.log), and [full/moved-set pair identities](checks/equivalence.log). [CTest 21/21](checks/tests.log) and [full differential fuzzing](checks/fuzz.log) pass. Both screens match fixture hashes, pair checksums, work and memory across all 64 library rows, with zero warm allocations.

Reconstruct the [direct-init build](../g2-topology-cost-2026-11-20/README.md#correctness-and-reproduction) as `WORK/control` and `WORK/control-build`. Copy the source (dereference symlinks) to `WORK/candidate` and apply `patch -p1 < candidate.patch`. Verify [control](../g2-topology-cost-2026-11-20/candidate-inputs.sha256) and [candidate](candidate-inputs.sha256) inputs. Build GCC 16.2.1 Release with `QUADTREE_BENCH_NATIVE=OFF` and Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. Check [`.text` hashes](text.sha256), then run `python3 inspect.py WORK RESULT_DIR` for emitted symbols and calls. Use the fusion report's [functional checks](../g2-fused-commit-2026-11-20/verify.sh), [pilot runner](../g2-fused-commit-2026-11-20/pilot.sh) and [summarizer](../g2-fused-commit-2026-11-20/summarize-pilot.py). The pilot pins CPU 6 and restores power settings. [Raw phase runs](pilot/) · [Diagnostic raw runs](direct-screen/).

For the diagnostic only, make a new `DIAG` work directory with `control-build` pointing to `WORK/candidate-build` and `box2d` pointing to the pinned checkout. Create `DIAG/candidate-build/bench/` and run `python3 make-direct.py WORK/candidate-build/bench/quadtree-bench DIAG/candidate-build/bench/quadtree-bench`. Use the same pilot and summarizer on `DIAG`. The diagnostic never substitutes for the source candidate or a gate run.

## Handoff

Keep the outline patch as an isolated pair-phase factor, not the staged control. On direct child initialization, **profile the stable-scatter cursor update**: attribute `dst[next[tags[i]]++]` by instruction (tag/ID loads, cursor load–update–store, destination store), and measure how often successive IDs hit the same cursor by depth on both blockers. The [staged loader profile](../g2-staged-traffic-2026-11-20/README.md#staged-release-profile) puts scatter at 22–23% of unordered build cycles with almost all sampled tag/ID loads hitting L1. Screen independent cursor work only if the profile and census support it; do not retune classifier lookahead or stack the rank queue.

At the latest matched 15-round control's upper bounds, `uniform` and translated still need ~0.81 ms and ~0.76 ms less unordered full-step time at unchanged relative uncertainty. The translated G9 upper bound allows only 1.2% less unordered time if ordered time stays fixed; clearing its G2 gap would need ~2.29 ms less ordered time too. Measure both engines and rerun both gates for a resolved candidate. G1/G2 still block 08.
