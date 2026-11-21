# 02 · Isolate the empty pair shortcut to unordered dispatch

**Rejected.** The unordered-only split loses both empty passes from the [earlier two-mode probe](../g1-small-headroom-2026-11-21/README.md) and slows ordered 4/8 in both scenes and repeats. The exact staged control is unchanged. G1 is still 8/16 and blocks 08.

## Change and emitted code

[Candidate patch](candidate.patch): only unordered `forEachPair` takes the explicit zero-size return and the outlined nonempty helper, which takes `WorkBudget`. Ordered keeps the same rank checks, workspace lifetime, `orderedSelf` call and return contract. Unordered keeps the singleton return, budget checks, tiny/flat/tree dispatch and callback order. No changes to the benchmark adapter, runner, fixture, baselines, limits or process error.

[Codegen comparison](codegen.log) · [Control assembly](control-dispatch.asm.gz) · [Candidate assembly](candidate-dispatch.asm.gz). GCC inlines the unordered adapter; the zero-size branch skips the helper, but materializes options on the stack before the branch and still transfers zero query counters through a store/load at `0xd3a5a` / `0xd3a62`. The ordered runner's 668 instructions match after normalizing branch/RIP displacements, but ordered `LibraryIndex::pairs` grows from 506 to 546 instructions and its frame from `0x1f8` to `0x298`. The library source branch is isolated; compiler effects are not. These are observed code-shape changes, not a measured attribution of the slowdown.

## Matched 81/117-round G1 screen

Both executables pass 8/16 in both rounds; both fail 0/1/2/4 in each scene. All tested intervals below resolve a slowdown. Times are full-step ns/scene (control → candidate); intervals are for candidate/control.

| Rounds | Scene | Unordered 0 | Ordered 4 | Ordered 8 |
|---:|---|---:|---:|---:|
| 81 | `clustered` | 10.7 → 12.6 [1.135–1.222] | 136.5 → 150.2 [1.069–1.132] | 350.5 → 379.3 [1.047–1.118] |
| 81 | `uniform` | 10.7 → 13.3 [1.115–1.386] | 142.4 → 158.2 [1.078–1.144] | 330.4 → 355.8 [1.045–1.110] |
| 117 | `clustered` | 10.6 → 12.6 [1.146–1.233] | 137.7 → 151.6 [1.068–1.135] | 337.2 → 376.5 [1.085–1.149] |
| 117 | `uniform` | 10.6 → 12.7 [1.155–1.243] | 143.0 → 158.9 [1.078–1.145] | 325.5 → 363.9 [1.084–1.153] |

Ordered 4 slows 10.0–11.1%; ordered 8 slows 7.7–11.8%. No broad G2/G9 rerun on this rejected candidate. [All G1 verdicts](g1/g1-comparison.csv) · [All matched step intervals](g1/step-change.csv) · [Raw data and full gate logs](g1/). [Verification](g1/verification.log) reconciles all 192 matched engine rows on fixture, pair counts, checksums, work, allocations and memory. All 32 warm library rows per process allocate zero. The [focused check](pair-check.log) passes status/rank/budgets/orientation/Stop, including [ASan/UBSan](pair-check-sanitized.log).

## Reproduction

Use the [exact packed cross-item tree and Release build](../g2-packed-cross-item-2026-11-21/README.md#checks-and-reproduction) as `WORK/control` and `WORK/control-build`. Link its Box2D checkout as `WORK/box2d`, copy the source as `WORK/candidate` and apply `candidate.patch`. `bash build.sh WORK` builds against the pinned control Box2D archive with GCC 16.2.1 and the Release flags. `bash run.sh WORK OUT` verifies inputs and `.text`, then runs unchanged G1 on CPU 6, 81 rounds control → candidate, 117 candidate → control, with quiet paired starts ≤40 °C and within 1 °C. [Run order](g1/run-order.csv); power settings restored. Run `python3 ../g1-staged-small-2026-11-21/summarize.py OUT` and `python3 codegen.py WORK EXISTING_OUTPUT_DIR`.

## Handoff

**Do not stage or stack this split.** If continuing 02, use the earlier empty-only probe as the codegen reference for the fast path and the staged binary as the ordered reference. Before timing another outline, inspect both the unordered zero path and the ordered pair adapter, not just the runner; this split demonstrates that matching the runner alone does not preserve ordered performance. Keep the build contract, adapter, baselines and limits unchanged.
