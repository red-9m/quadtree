# 03 · Topology materialization costs

**Direct child initialization staged, not shipped.** It removes the stack `Node` copy and consistently reduces unordered build medians in the repeated phase screen. At 15 rounds, G2 improves to 11 pass / 0 fail / 4 inconclusive; G9 passes 15/15. The nine-round G9 repeat has one inconclusive row. G2 remains open; runtime unchanged.

## Staged Release breakdown

Reattribute the [16 archived captures](../g2-node-locality-2026-11-20/raw-callchains.tar.gz) from the hash-matched staged inline-count + lookahead control. GCC 16.2.1 Release, Ryzen 9 5900HS, `int32`, 100,000 entries, two reversed blocks per engine and scene. Ranges below are percentages of sampled **build** cycles across blocks.

| Scene / engine | Node init | Dense references | Own summary | Subtree refit | Other topology¹ | Topology total |
|---|---:|---:|---:|---:|---:|---:|
| `uniform` / `index` | 6.3–7.3 | 0.5–0.7 | 1.2–1.6 | 2.8–2.9 | 4.2–4.6 | 16.0–16.1 |
| `split-line-translated` / `index` | 8.0–8.1 | 0.6–0.7 | 0.9–1.1 | 2.5–2.9 | 4.5–4.7 | 17.0–17.1 |
| `uniform` / `index-ordered` | 3.1–3.4 | 0.4 | 0.6–0.9 | 1.4–1.6 | 2.0–2.7 | 8.2–8.4 |
| `split-line-translated` / `index-ordered` | 4.1 | 0.4–0.5 | 0.5–0.6 | 1.4–2.1 | 2.3–2.7 | 9.2–9.4 |

¹ Root setup, split calculation, plan/recursive dispatch, dense loop setup/control, and attribution not separable at the instruction level. `buildDenseItems` interleaves reference stores and box/filter reduction, so their separate cycle buckets approximate the split; [per-operand store PCs](store-sites.csv) were audited. `refit` attribution uses its complete symbol range; it includes the initial copy of own bounds/filter/count, child aggregation and non-taken mutation checks. Cycle periods are not removable time.

- **Node initialization is the largest identifiable topology cost.** It accounts for 39–48% of unordered topology cycles (`detail/quadtree-index-impl.h:1894–1898` on the staged source). The [staged disassembly](control-store-instructions.asm) shows stack zeroing of a temporary `Node`, then a wide copy to the pool at each child setup. IBS in each unordered capture samples 22–37 stores at the audited stack-zero sites and 33–44 at the pool-copy sites; reported sources are almost all L1. These are operations, not bytes or writeback transactions.
- The unprofiled [15-round control](../g2-contiguous-pairs-2026-11-20/g2-comparison.csv) puts unordered build medians at 6.14 and 6.69 ms. Scaling by the sampled shares gives ~0.39–0.45 ms of `uniform` build and ~0.54 ms of translated build at node setup; even eliminating the entire bucket would not close either G2 gap. Avoiding a stack temporary is a smaller, isolated target.
- [Cycle periods](cycle-periods.csv) · [IBS data sources](ibs-data-sources.csv) · [Audited store counts](store-counts.csv) · [Verification](verify-profile.log). All 16 captures match staged fixtures, work and pair checksums, with zero warm allocations. Every topology bucket reconciles with the [prior staged build attribution](../g2-staged-traffic-2026-11-20/cycle-periods.csv).

## Direct child initialization screen

The [candidate](candidate.patch) reinitializes each trivially destructible child `Node` in its existing pool slot using aggregate initialization. Default members, child cell, parent, depth, block allocation and recursion order are unchanged. No dense entry commit, own summary, refit, mutation or sorted fallback source changed. No new allocation or capacity.

The [matched disassembly](node-init-instructions.asm) confirms stores go directly into the pool, with no stack `Node` copy. `materializeBulk<int32>` shrinks from 1,540 to 1,158 bytes; its explicit stack reservation drops from 280 to 152 bytes. The pool stores and later refit are still present.

Two reversed nine-round blocks per screen, `int32` at 100,000 across `uniform`, `split-line`, `split-line-translated`, and `mixed-size`. The [first screen](pilot/step-change.csv) showed conflicting full-step changes: unordered `split-line` −6.2% / +16.2%, ordered `split-line` −14.4% / +44.0%. Those swings include pair-phase movement despite unchanged traversal source; no cause was isolated.

The [repeat](repeat/step-change.csv) shows all eight unordered build medians down 4.7–7.4%, or ~0.29–0.50 ms. The `uniform` full-step intervals still include no change. Unordered `split-line` and `split-line-translated` resolve gains of 4.8% and 3.7% in repeat block 2 only. Ordered repeat intervals span no change except `uniform` in block 1 (−3.6%). Use the frozen slice to test the gate trade-off; do not infer it from separate phase medians.

All 64 pilot rows match the staged fixture hashes, work, results, allocations, and footprint. [First screen checks](pilot/verification.log) · [Repeat checks](repeat/verification.log).

## Frozen snapshot slice

Counts are pass / fail / inconclusive. G0 cross-checks and G3/G4/G6 pass throughout. All 68 library rows per run have zero warm allocations.

| Rounds | G2 control → direct init | G9 control → direct init | G1 control → direct init |
|---:|---:|---:|---:|
| 9 | 9 / 1 / 5 → 12 / 0 / 3 | 14 / 0 / 1 → 14 / 0 / 1 | 8 / 8 / 0 → 8 / 8 / 0 |
| 15 | 9 / 1 / 5 → 11 / 0 / 4 | 12 / 0 / 3 → 15 / 0 / 0 | 8 / 7 / 1 → 8 / 6 / 2 |

15-round `index`, 100,000 entries, control → direct init, ms/snapshot. Full-step estimates and phase medians are separate.

| Scene | Full step | Build median | Pair median | G2 candidate (≤2×) |
|---|---:|---:|---:|---|
| `uniform` | 14.736 → 13.197 | 6.697 → 5.824 | 7.462 → 7.323 | 2.024 [1.906–2.150], inconclusive |
| `split-line` | 16.168 → 13.668 | 7.184 → 6.277 | 7.452 → 7.374 | 1.926 [1.862–1.992], pass |
| `split-line-translated` | 13.888 → 13.272 | 6.773 → 6.331 | 6.930 → 6.946 | 2.019 [1.953–2.087], inconclusive |

- `uniform` 10,000 and `split-line` 100,000 gain G2 passes in both round counts. Nine-round translated 10,000 gains a pass that does not repeat at 15 rounds; its gate remains inconclusive. `uniform` 100,000 loses its nine-round fail but does not pass either run.
- Use the repeated pilot for the size of the build improvement. Broader effects in the frozen runs are not isolated: nine-round `split-line` 100,000 passes G2 while unordered time rises 10%, because the measured `grid` rises 51%. That same candidate row is the only inconclusive G9 row at nine rounds (4.286 [3.535–5.198]); at 15 rounds it passes (3.691 [3.584–3.801]). At 15 rounds the `uniform` and `split-line` control full-step estimates are elevated relative to the prior staged control and repeat pilot. Do not attribute the entire frozen full-step difference to removal of the stack `Node`.
- [G2 phases and work](frozen/g2-comparison.csv) · [G9 phases and ratios](frozen/g9-comparison.csv) · [Matched steps and intervals](frozen/step-change.csv) · [Gate counts](frozen/gate-counts.csv) · [All uncapped results, work, and memory match](frozen/verification.log). Raw samples, environment and run order are in [frozen/](frozen/).

## Correctness and reproduction

The direct-init candidate matches [exact dense slots, topology and callback prefixes](checks/dense-order.log) including under ASan/UBSan, [ordered](checks/ordered-prefix.log) and [unordered](checks/unordered-prefix.log) budget/predicate/stop prefixes, and [full/moved-set identities](checks/equivalence.log). [CTest 21/21](checks/tests.log) and [full differential fuzzing](checks/fuzz.log) pass.

Reconstruct the staged control and its `.text`-matched symbolization twin per the [node-locality report](../g2-node-locality-2026-11-20/README.md#correctness-and-reproduction). Run `python3 summarize.py CAPTURE_DIR DEBUG_ELF RESULT_DIR` with the extracted archive and its original `elf-path.txt`. [Environment](../g2-node-locality-2026-11-20/environment.txt) · [Capture order](../g2-node-locality-2026-11-20/run-order.csv).

Copy `WORK/control` to `WORK/candidate` (dereference symlinks) and apply `patch -p1 < candidate.patch`. Verify [control](../g2-child-bounds-only-2026-11-20/control-inputs.sha256) and [candidate](candidate-inputs.sha256) input hashes. Build `WORK/candidate-build` with GCC 16.2.1 Release, `QUADTREE_BENCH_NATIVE=OFF`, and pinned Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. Check [`.text` hashes](text.sha256). Run the fusion report's [functional checks](../g2-fused-commit-2026-11-20/verify.sh), [pilot runner](../g2-fused-commit-2026-11-20/pilot.sh), and [summarizer](../g2-fused-commit-2026-11-20/summarize-pilot.py). The pilot pins CPU 6 and restores power settings. [Pilot environment](pilot/environment.txt) · [Run order](pilot/run-order.csv) · [Raw samples](pilot/). Rerun the pilot in a second output directory for the repeat. For the frozen slice, use the [full runner](../g2-inline-count-2026-11-20/run.sh) with `WORK NEW_OUTPUT_DIR full`, followed by the [G2/G9 summarizer](../g2-cross-screen-2026-11-20/summarize.py) on that directory.

## Handoff

Use direct child initialization on the inline-count + lookahead build as the next staged control. Preserve the former control for comparisons; do not ship while G1/G2 remain open. Profile **ordered rank-table insertion** on the new control, distinct from duplicate-rank validation: attribute `hashInsert` mixing, epoch/bucket accesses, probe updates and tree conversion; count actual probes and conversions on the frozen ranked fixtures. The [previous staged profile](../g2-staged-traffic-2026-11-20/README.md#staged-release-profile) assigned ~19% of ordered build cycles to rank insertion, not all removable. Seek ordered headroom before stacking another unordered optimization; keep prepared allocation guarantees and collision fallback.

At unchanged relative uncertainty, the 15-round staged `uniform` row needs about **0.92 ms (7.0%)** less unordered full-step time, and translated needs **0.55 ms (4.2%)**. Translated G9's 3.911× upper bound permits only **2.2%** less unordered time with ordered time held fixed. Clearing its G2 gap at the same uncertainty would also need about **1.00 ms (2.0%)** less ordered time. Rerun both gates together. G1/G2 still block 08.
