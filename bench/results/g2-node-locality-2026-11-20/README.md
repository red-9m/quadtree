# 03 · Measure node metadata locality

**Layout not retained.** On the staged control, ordered `walkOrdered` accounts for most sampled non-L1 pair loads on both fixtures, principally child `count` and `bounds`. Unordered `crossPairs` reads those fields mostly from L1. Packing the fields into fewer lines resolved no full-step gain in either engine. Runtime unchanged.

## Staged layout and profile

GCC 16.2.1 x86-64 Release, Ryzen 9 5900HS, 64-byte L1 lines. [`Node<int32>` layout](layout.txt): 120-byte stride, 8-byte alignment; `bounds` at bytes 20–35, `firstChild` at 92–95, `count` at 100–103. No edge shares a line with its node's `count` at any permitted starting residue. The three fields occupy 2–3 lines per node; count + all bounds edges occupy 7–8 distinct lines for a contiguous four-child group. These are address counts, not observed cache traffic; short-circuiting can skip edges.

Two reversed capture blocks at 100,000 `int32` entries. Non-L1 counts below are for the three requested fields at the audited instructions, not for all loads in the function. Ranges span the two blocks.

| Scene / engine | Function % of pair cycles | Field loads served by L1 | Field non-L1 samples / % of all pair non-L1 samples |
|---|---:|---:|---:|
| `uniform` / `index` | 34.8–35.7% | 97.8–98.0% | 10–12 / 35.7–37.5% |
| `split-line-translated` / `index` | 35.0–35.3% | 95.9–96.8% | 15–21 / 47.7–50.0% |
| `uniform` / `index-ordered` | 65.6–66.0% | 69.2–70.2% | 233–260 / 56.4–57.4% |
| `split-line-translated` / `index-ordered` | 68.9–69.3% | 64.0–65.7% | 314–353 / 66.7–67.8% |

- In unordered, non-L1 reads of the three fields account for at most 0.35% of **all** sampled pair loads. The high share of non-L1 loads comes from a small denominator (28–44 per capture); it does not explain `crossPairs`' cycle share.
- In ordered, `bounds` accounts for 38.5–46.8% of all pair non-L1 samples, `count` for 16.1–20.6%, `firstChild` for 0.5–1.4%. Almost all of these samples report L2 or `core, same node Any cache hit`; the latter is not treated as a precise cache level. Ordered locality was worth screening for G9 headroom, not as a standalone G2 fix.
- `crossPairs` and `walkOrdered` sites were mapped by [disassembled operands](load-instructions.asm), not just by source lines. On the staged source, see `detail/quadtree-index-impl.h:3062–3083` and `:2679–2690`. The [IBS address residues](ibs-node-residues.csv) agree with 8-byte node alignment. [Per-field and per-PC counts](ibs-summary.csv) · [Data sources](ibs-levels.csv) · [Cycle periods](cycle-periods.csv) · [Symbolized sites](ibs-sites.csv).
- IBS dispatch samples measure load operations; several loads can share one cache-line fill. Cycle shares sum `cycles:u` sample periods attributed to the pair phase; the function ranges include any code inlined into that body. Neither measure estimates removable stall time. Build, pair, and other cycle totals are separate.

## Packed layout screen

The [patch](candidate.patch) moves `bounds`, `count` and `firstChild` to offsets 0, 16 and 20 for `int32`, followed by the subtree filter and own-entry range. It changes only `Node` member order, not traversal or topology. The [layout probe](layout-candidate.txt) confirms unchanged size and alignment for all four benchmark coordinate types. The three fields use 1–2 lines per node; four siblings' count + bounds use 4–6 lines.

Two reversed nine-round unprofiled blocks, same fixture size and both engines. Pair medians in ms; full-step changes use estimates.

| Scene / engine | Pairs, block 1 | Pairs, block 2 | Full step, blocks 1 / 2 |
|---|---:|---:|---:|
| `uniform` / `index` | 7.270 → 7.417 | 7.323 → 7.489 | +2.6% / +3.3% |
| `split-line-translated` / `index` | 6.941 → 7.058 | 6.966 → 7.085 | +2.4% / +2.1% |
| `uniform` / `index-ordered` | 32.149 → 32.613 | 31.784 → 32.494 | +2.4% / +0.6% |
| `split-line-translated` / `index-ordered` | 37.718 → 38.029 | 38.018 → 38.498 | +0.0% / +3.3% |

- No full-step interval shows a gain across all four scenes (`split-line` and `mixed-size` included). Ordered `split-line` slows 4.9% in block 2, ratio 1.049 [1.014–1.085]. Packing lowers the number of lines spanned but does not demonstrate lower observed misses or time; the candidate was not profiled.
- [Steps, phases and intervals](pilot/step-change.csv) · [All 32 rows match fixtures, checksums, work, allocations and footprint](pilot/verification.log). Zero warm allocations. No frozen-slice rerun; the [last matched 15-round control](../g2-contiguous-pairs-2026-11-20/15-control/gates.txt) remains G2 9 pass / 1 fail / 5 inconclusive, G9 15/15.

## Correctness and reproduction

The candidate matches exact [unordered](checks/unordered-prefix.log) and [ordered](checks/ordered-prefix.log) callback prefixes, [dense slots and topology under ASan/UBSan](checks/dense-order.log), and [full/moved-set pair identities](checks/equivalence.log). [CTest 21/21](checks/tests.log) and [full differential fuzzing](checks/fuzz.log) pass.

Reconstruct `WORK/control`, `WORK/control-build` and `WORK/box2d` per the [preceding report](../g2-child-bounds-only-2026-11-20/README.md#checks-and-reproduction). The [staged input hashes](../g2-child-bounds-only-2026-11-20/control-inputs.sha256) and [`.text` hash](text.sha256) identify the profiled source and binary; platform and CPU power settings were left unchanged. Run `bash profile.sh WORK NEW_CAPTURE_DIR`, then `python3 summarize.py NEW_CAPTURE_DIR NEW_CAPTURE_DIR/control.debug RESULT_DIR`. The runner needs CPU 6 (override `CPU`) and per-process `ibs_op` with `swfilt`. [Environment](environment.txt) · [Run order](run-order.csv) · [Raw callchains and logs](raw-callchains.tar.gz) · [16 capture checks](verification.log). For archived captures, keep the recorded ELF path in `elf-path.txt` and supply the matching symbolization twin.

Copy the staged source to `WORK/candidate` (dereference symlinks), apply `patch -p1 < candidate.patch`, and verify [candidate input hashes](candidate-inputs.sha256). Build `WORK/candidate-build` with the same flags and pinned Box2D; verify [both pilot `.text` hashes](pilot-text.sha256). Compile `layout.cpp` against each source with `c++ -std=c++20 -fno-access-control -I SOURCE layout.cpp -o layout`. Run the fusion report's [pilot runner](../g2-fused-commit-2026-11-20/pilot.sh) with `WORK NEW_PILOT_DIR`, then its [summarizer](../g2-fused-commit-2026-11-20/summarize-pilot.py). Run its [functional checks](../g2-fused-commit-2026-11-20/verify.sh) separately. The pilot pins CPU 6 and restores platform settings.

## Handoff

Keep inline counts + bounded lookahead staged. Do not tune field order or cross-child screening again without new evidence. Attribute the **staged** `planBulk` build cost on `uniform` and `split-line-translated`: separate remaining indexed gather/classify from stable scatter and measure how much traffic a different stable routing scheme could avoid. The older [narrow-build profile](../g2-narrow-profile-2026-11-20/README.md#unordered-build-attribution) predates lookahead; do not reuse its percentages as current measurements or retune the rejected [four-gather change](../g2-gather-mask-2026-11-20/README.md). `uniform` still needs about 10% less full-step time. Translated G9 permits only 3.8% unordered savings if ordered time stays fixed; account for it before advancing any new loader change. G1/G2 still block 08.
