# 03 · Split classifier and scatter: isolated factors

**Narrow streams staged; centers/spans not advanced.** Narrow streams cut full steps 4.6–8.3% on the three 100,000-entry G2 blockers at 15 rounds. G2 gains no pass; G9 remains 15/15 in both repeats. Runtime source is unchanged.

## Candidates

Both start from [compact boxes + packed build ranks](../g9-build-rank-2026-11-20/README.md), not shipped source. Apply these patches to separate copies:

- [Centers/spans](centers.patch): capture a 16-byte record of signed `int32` centers and unsigned spans in place of each captured box. Classification reuses these values at each level. Reconstruct exact endpoints for dense commit, the single-quadrant prefix test and the sorted fallback. No change to permutation or tag layout.
- [Narrow streams](narrow.patch): keep `Box<int32>` input; replace the 24-byte-stride entry/tag accesses with two contiguous `uint32_t` permutations and a `uint8_t` tag stream. The streams occupy 9 bytes per entry within the existing temporary key buffer; internal plan records remain in the other key buffer. Use the completed permutation for dense commit and restore typed key scratch only for the sorted fallback. No additional allocation, capacity, footprint or changes to the splitting rule or stable order.

## Factor screen

Two blocks of nine rounds, in reversed factor order. All rows are `int32`, 100,000 entries. The screen compares phases and steps, not gate ratios; each table cell lists block 1 / block 2, milliseconds.

| Scene, unordered | Control build | Centers/spans build | Narrow build |
|---|---:|---:|---:|
| `uniform` | 7.304 / 7.328 | 7.134 / 7.408 | 6.538 / 6.533 |
| `split-line` | 7.642 / 7.626 | 7.399 / 7.842 | 6.921 / 6.952 |
| `split-line-translated` | 7.878 / 7.629 | 7.523 / 7.393 | 6.902 / 7.561 |

**Advance narrow streams alone.** They cut unordered build medians 9–11% on `uniform` and `split-line` in both blocks, and their full-step gains resolve on both scenes. The translated improvement resolves only in block 1. No centers/spans unordered full-step interval excludes no change; ordered build medians do not consistently improve. Do not stack the factors. For `mixed-size`, neither factor resolves an ordered full-step change; narrow's ordered pair medians are 136.345 / 137.581 ms versus control's 137.326 / 134.726 ms. No inferred G9 pass from the pilot.

[Pilot steps and intervals](pilot/step-change.csv) · [Raw samples and run order](pilot/) · [Verification](pilot/verification.log).

## Frozen snapshot slice — narrow streams only

Counts are pass / fail / inconclusive. G0, G3, G4 and G6 pass throughout; every library row completes with zero warm allocations.

| Rounds | G2 control → narrow | G9 control → narrow | G1 control → narrow |
|---:|---:|---:|---:|
| 9 | 9 / 4 / 2 → 9 / 3 / 3 | 15 / 0 / 0 → 15 / 0 / 0 | 8 / 7 / 1 → 8 / 6 / 2 |
| 15 | 9 / 5 / 1 → 9 / 3 / 3 | 15 / 0 / 0 → 15 / 0 / 0 | 8 / 6 / 2 → 8 / 6 / 2 |

15-round `index`, 100,000 entries, milliseconds. Full-step estimates and phase medians are distinct.

| Scene | Step before → after | Build before → after | Pairs before → after | G2 after (limit 2×) |
|---|---:|---:|---:|---|
| `uniform` | 15.014 → 13.762 | 7.375 → 6.454 | 7.411 → 7.258 | 2.160 [2.098–2.225], fail |
| `split-line` | 15.109 → 14.410 | 7.608 → 7.037 | 7.403 → 7.359 | 2.054 [1.989–2.121], inconclusive |
| `split-line-translated` | 14.720 → 14.047 | 7.732 → 7.059 | 6.964 → 6.929 | 2.134 [2.052–2.220], fail |

- All three unordered full-step gains resolve in both repeats. At 15 rounds, `split-line` at 100,000 and `split-line-translated` at 10,000 move from fail to inconclusive; `uniform` at 10,000 remains inconclusive.
- The translated G9 upper bound is 3.951× at nine rounds and 3.948× at 15. Its ordered step shows no resolved change in either repeat. At 15 rounds its build median falls from 13.728 to 12.619 ms and its pair median moves from 40.302 to 40.844 ms. `mixed-size` ordered pairs are 135.013 → 134.979 ms and G9 passes at 3.652× [3.542–3.764].
- [G2 per-row phases and work](g2-comparison.csv) · [G9 per-row phases and ratios](g9-comparison.csv) · [Matched steps and intervals](step-change.csv) · [Gate counts](gate-counts.csv) · [9-round control](9-control/gates.txt) / [candidate](9-candidate/gates.txt) · [15-round control](15-control/gates.txt) / [candidate](15-candidate/gates.txt).

## Correctness and reproduction

- [Dense-order probe](dense-order.log) matches input-to-slot mapping, geometry, topology and ordered/unordered callback prefixes for both factors, including full signed spans, one-axis splits, coincident boxes and a forced sorted fallback. Both pass ASan/UBSan. The narrow build also matches [full/moved-set results](equivalence.log) and passes [full differential fuzzing](fuzz.log).
- CTest passes 21/21 on [centers](centers-tests.log) and [narrow](narrow-tests.log). [Full-slice verification](verification.log) matches uncapped results, work, allocations and footprint.

Reconstruct the [combined rank candidate](../g9-build-rank-2026-11-20/README.md#correctness-and-reproduction) as `WORK/control`; copy it to `WORK/centers` and `WORK/narrow` and apply the respective patch. Verify [control](control-inputs.sha256), [centers](centers-inputs.sha256) and [narrow](narrow-inputs.sha256) input hashes. Build the three `*-build` directories with GCC 16.2.1 Release and pinned Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. [Executable `.text` hashes](text.sha256).

Run `bash verify-order.sh WORK/control WORK/centers WORK/narrow`, the [full/moved-set comparison](../g2-topdown-build-2026-11-20/verify.sh) for control/narrow, CTest for both factors, and `WORK/narrow-build/bench/quadtree-index-fuzz`.

Run `bash run.sh WORK NEW_PILOT_DIR pilot` and `python3 summarize-pilot.py NEW_PILOT_DIR`. Point `WORK/candidate-build` to `WORK/narrow-build` and run `bash run.sh WORK NEW_OUTPUT_DIR full`, followed by the [G2/G9 summarizer](../g2-cross-screen-2026-11-20/summarize.py). The runner pins CPU 6 and restores the platform profile and every CPU energy preference ([before](power-before.txt) / [after](power-after.txt)). [Environment](environment.txt) · [Run order](run-order.csv).

## Handoff

Use narrow streams as the staged control. Do not stack the centers/spans patch. On the 15-round candidate, `uniform` and `split-line-translated` at 100,000 still need about 1.4 ms each (10% of the unordered full step) to clear their G2 upper bounds at unchanged uncertainty. The translated G9 upper bound of 3.948× leaves only 1.3% unordered headroom if ordered time stays fixed; a 10% unordered gain would require roughly 9% lower ordered time too. `mixed-size` allows about 5–6% unordered savings at unchanged ordered time across these two runs.

Profile the narrow build before another loader change: separate remaining classification/scatter from dense commit and topology. Also reprofile the translated ordered walk on the same executable; look for a way to reduce repeated source walks without changing charged visits, predicate order, or ordered prefixes. Size a combined change against the measured budgets and rerun G2/G9 together. G1/G2 still block release acceptance.
