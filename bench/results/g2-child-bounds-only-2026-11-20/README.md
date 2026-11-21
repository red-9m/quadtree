# 03 · Isolate cross-child bounds screening

**Not retained.** Screening alone lowers unordered pair medians on both split-line scenes, but no unordered full-step gain resolves. Ordered `mixed-size` slows in both blocks. Runtime unchanged; keep inline counts + bounded lookahead staged.

## Candidate

[Candidate patch](candidate.patch) applies only the cross-child hunks from the [earlier combined screen](../g2-cross-screen-2026-11-20/README.md#candidate) to the [staged control](../g2-inline-count-2026-11-20/README.md). `selfPairs` and recursive `crossPairs` screen child pairs by occupancy and closed bounds before recursing. Charge disjoint nonempty pairs in batches, flushing before the next admitted child pair or at the end of a row. Empty nodes remain uncharged; the chargeable prefix, filter/predicate calls, and callback order stay unchanged. Structural bounds checks can precede the grouped charge, as with the staged ordered walk. No exact overlap, loader, ordered traversal, or storage changes.

## Matched phase screen

Two reversed nine-round blocks, `int32`, 100,000 entries. Unordered pair medians in milliseconds, followed by full-step percentage changes; [intervals and both engines’ phases](pilot/step-change.csv).

| Scene | Pairs, block 1 | Pairs, block 2 | Full step, blocks 1 / 2 |
|---|---:|---:|---:|
| `uniform` | 7.351 → 7.461 | 7.289 → 7.491 | −1.1% / +1.9% |
| `split-line` | 7.407 → 7.114 | 7.409 → 7.062 | −1.5% / −3.1% |
| `split-line-translated` | 7.019 → 6.666 | 6.994 → 6.654 | −2.2% / −2.0% |
| `mixed-size` | 34.405 → 34.784 | 34.113 → 34.613 | +2.0% / +1.7% |

- All eight unordered full-step intervals include no change. The split-line pair savings of 0.29–0.35 ms did not resolve a full-step gain; `uniform`, the hard G2 blocker in the last staged control, gets no pair gain.
- Ordered `mixed-size` full steps slow 7.2% and 4.7%, with both intervals excluding no change. Pair medians rise 133.674 → 143.124 ms and 136.032 → 143.970 ms. Ordered traversal source is unchanged; the screen does not identify the cause of this timing change.
- [All 32 rows](pilot/verification.log) match staged fixtures, checksums, pair counts, work, allocations and footprint. Zero warm allocations. Raw samples and run order are in [pilot/](pilot/).
- No frozen slice rerun or new gate verdict. The [previous matched 15-round control](../g2-contiguous-pairs-2026-11-20/15-control/gates.txt) remains the staged evidence: G2 9 pass / 1 fail / 5 inconclusive; G9 15/15.

## Checks and reproduction

Exact [unordered budget/predicate/stop prefixes](checks/unordered-prefix.log) match with both boundary modes, instrumentation on/off, and before/after mutation. [Ordered prefixes](checks/ordered-prefix.log), [dense slots/topology under ASan/UBSan](checks/dense-order.log) and [full/moved-set identities](checks/equivalence.log) match. [CTest 21/21](checks/tests.log) and [full differential fuzzing](checks/fuzz.log) pass.

Reconstruct the [staged control](../g2-inline-count-2026-11-20/README.md#correctness-and-reproduction) as `WORK/control`; copy the source (not the symlink) to `WORK/candidate` and apply `candidate.patch` with `patch -p1`. Verify [control](control-inputs.sha256) and [candidate](candidate-inputs.sha256) inputs. Use GCC 16.2.1 Release, `QUADTREE_BUILD_BENCHMARKS=ON`, `QUADTREE_BUILD_TESTS=ON`, `QUADTREE_BENCH_NATIVE=OFF`, and Box2D `178495ba35695aefe9c321e569d2277f06db3b90` in `WORK/box2d`. Build `WORK/control-build` and `WORK/candidate-build`. [Base](working-base.txt) · [ELF `.text` hashes](text.sha256).

Use the fusion report’s [pilot runner](../g2-fused-commit-2026-11-20/pilot.sh) with `WORK NEW_PILOT_DIR`, then its [summarizer](../g2-fused-commit-2026-11-20/summarize-pilot.py). It pins CPU 6 and restores power settings. [Environment](pilot/environment.txt) · [Run order](pilot/run-order.csv). Run the shared [verification](../g2-fused-commit-2026-11-20/verify.sh) with `WORK CHECK_OUTPUT_DIR` separately from timing.

## Handoff

Return to the staged control. Do not retune screening or exact eligibility without new evidence. Check whether node metadata locality is worth targeting: map `Node::count`, `bounds` and `firstChild` to cache lines, then attribute `crossPairs` and `walkOrdered` misses on `uniform` and `split-line-translated` to those loads. Only screen a layout change if the profile supports it; preserve node size, topology, callback prefixes and visit charges. `uniform` still needs roughly 10% less full-step time, and translated G9 has only about 3.8% unordered headroom at unchanged ordered time. G1/G2 still block 08.
