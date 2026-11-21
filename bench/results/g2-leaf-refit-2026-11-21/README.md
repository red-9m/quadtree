# 03 · Publish clean bulk leaves without generic refit

**Do not advance leaf refit.** On the primary 10,000-entry row, the build median saves 7.6 µs in one block and 0.9 µs in the other; neither full-step gain resolves. No frozen rerun or runtime change.

The [dense/topology breakdown](../g2-commit-breakdown-2026-11-21/README.md) attributes about 5% of unordered `split-line` 10,000 build cycles to refit across all nodes. The [candidate patch](candidate.patch) changes only fresh leaves in `materializeBulk` on two-stream + source radix. Their reference links and own bounds/filter summaries already come from `buildDenseItems`. Publish `ownCount`, `itemBounds` and `itemSummary` directly to the subtree fields, skipping the dirty-item and child walks that cannot apply to a freshly initialized leaf. Empty leaves, split-retry values, parent refit, mutation refit and the sorted fallback are unchanged. Neither direct addressing nor the outline is included.

## Matched phase screen

Two reversed nine-round blocks, both engines, `uniform`, `split-line`, `split-line-translated`, and `mixed-size` at 10,000 and 100,000. CPU 6, quiet starts within 1 °C. The primary target is the `split-line` 10,000 unordered row; a build-only change with no resolved representative full-step gain is not a gate pass.

| Unordered fixture | Build median in ms, block 1 / 2 | Full-step ratio, block 1 / 2 |
|---|---:|---:|
| `split-line` 10,000 | 0.438 → 0.430 / 0.435 → 0.434 | 0.960 [0.875–1.055] / 1.003 [0.974–1.034] |
| `uniform` 100,000 | 5.429 → 5.420 / 5.423 → 5.386 | 1.013 [0.973–1.055] / 0.987 [0.949–1.026] |
| `split-line-translated` 100,000 | 5.900 → 5.931 / 5.908 → 5.946 | 1.012 [0.974–1.052] / 1.014 [0.975–1.054] |

- No ordered gain resolves. The only resolved change is a `mixed-size` ordered regression at 100,000 in block 2 (1.049 [1.010–1.090]), including a larger pair median despite unchanged query source. It does not repeat in block 1.
- [Phases and intervals](pilot/step-change.csv) · [Verification](pilot/verification.log) · [Run order](pilot/run-order.csv) · [Raw samples and environment](pilot/). All 64 rows match fixture, checksum, work, memory and allocation counts; zero warm allocations. Power settings restored.

## Correctness and reproduction

[Leaf probe](checks/leaf-state.log) compares every live node's fields, including empty leaf bounds/filters, dirty flags and oversized-leaf retry, reference links, and both exact pair streams before and after update/remove/insert and rebuild. Both boundary modes, with and without finite budgets; GCC and ASan/UBSan outputs match. [Release excerpt](leaf-publish.asm) confirms the parent still tail-calls `refit` and the leaf now writes its three fields and returns. [General checks](checks/) include CTest 21/21, full differential fuzzing and existing exact streams.

Reconstruct the [source-radix stages](../g9-two-scatter-source-radix-2026-11-21/README.md#correctness-and-reproduction) as `WORK/control` and `WORK/control-build`. Copy source to `WORK/candidate`, apply `patch -p1 --fuzz=0 < candidate.patch`, and build GCC 16.2.1 Release, tests and benchmarks on, `QUADTREE_BENCH_NATIVE=OFF`, with pinned `WORK/box2d` at `178495ba35695aefe9c321e569d2277f06db3b90`. Verify [source hashes](candidate-inputs.sha256), [control ELF](control-executable.sha256), and [`.text` hashes](text.sha256). Run `bash verify-leaf.sh WORK` and the [general checks](../g2-radix-outline-2026-11-21/verify.sh), then `bash run.sh WORK NEW_PILOT_DIR --pilot` and `python3 summarize-pilot.py NEW_PILOT_DIR`.

## Handoff

**Profile unordered pair traversal on the unchanged staged control.** Separate `selfPairs`, `crossPairs` and `itemsVsSubtree` (including nested exact eligibility) for `split-line` at 10,000 and the two 100,000-entry blockers. Reuse the [current matched Release captures](../g2-bulk-classifier-2026-11-21/README.md) if callchain attribution is sufficient. Keep actual geometry, visit charges and callback order as constraints on the next candidate. The [older traversal profile](../g2-topdown-traversal-2026-11-20/README.md) predates two-stream scatter and source radix; don't reuse its percentages as current ones. No build candidate advances from the direct-addressing or leaf-refit screens; G1/G2 still block 08.
