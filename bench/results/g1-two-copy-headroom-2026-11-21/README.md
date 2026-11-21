# 02 · Two-entry minimal warm-copy headroom

**Stop pursuing a build-only fix for the two-entry rows.** Even the invalid copy-only replacement saves 9.0–10.3 ns/scene yet clears neither 2× interval in either matched repeat. No margin to restore the omitted contractual work. Nothing staged; task 02 remains open and blocks 08.

## Diagnostic and checks

[Patch](probe.patch): for contiguous two-entry `int32` snapshots with two existing unranked flat entries and no handle output, copy both current boxes, payloads and filters into the warm store before the general build path. Do not validate the input, publish extent, rebuild slots or acquire a fresh owner. Keep the first cold build, the bench adapter, pair traversal and counters, the frozen runner, fixtures and baselines unchanged. This is a deliberately invalid scratch build, not an optimization candidate or a mathematical lower bound.

- [Exact payload pairs](exact-pairs.log) match brute on all 256 fixtures in each of `uniform` and `clustered` on both binaries, with zero warm allocations. The [checker source](../g1-flat-input-2026-11-19/phase.cpp) uses the unchanged adapter; its separate probe timings are not G1 evidence.
- [Direct check](copy-probe.cpp) verifies the new payloads, geometry and filters are visible in the correct callback orientation; moving the second box outside the first removes the pair without changing candidate charge. It also demonstrates stale extent and old handle validity after the scratch `Ok` return. The control publishes the correct new extent and invalidates old handles. [Direct check log](copy-probe.log).
- [Codegen log](codegen.log): ordered and unordered `LibraryIndex::pairs` instruction sequences match after normalizing branch/RIP displacements (506 and 350 instructions). The full runners change, so use full-step timing rather than subtracting individual operation costs. [Control](control-dispatch.asm.gz) · [Probe](candidate-dispatch.asm.gz).

## Matched results

Frozen G1 workload, 81 rounds control → probe, then 117 probe → control. Table is two-entry unordered full-step ns/scene. Synthetic intervals show what the evaluator calculated for the deliberately invalid binary, **not gate verdicts**. Last column is the extra saving needed if the same measured brute and relative error hold.

| Rounds | Scene | Control → probe | Saved | Synthetic ratio [95% interval] | Further saving to clear upper bound |
|---:|---|---:|---:|---:|---:|
| 81 | `clustered` | 33.7 → 24.7 | 9.0 | 2.041 [1.977–2.108] | ~1.27 ns |
| 81 | `uniform` | 35.3 → 25.0 | 10.3 | 2.066 [2.013–2.120] | ~1.42 ns |
| 117 | `clustered` | 34.2 → 24.7 | 9.5 | 2.041 [1.989–2.095] | ~1.12 ns |
| 117 | `uniform` | 34.3 → 25.1 | 9.2 | 2.024 [1.892–2.166] | ~1.92 ns |

All four speedups resolve in the matched A/B. Both staged control two-entry rows fail both runs. All 192 matched engine rows agree on fixtures, checksums, counts, work, allocations and memory. Both warm library engines allocate zero. [Target CSV](diagnostic/two-entry-headroom.csv) · [Raw samples and unchanged evaluator](diagnostic/) · [Verification](diagnostic/verification.log).

## Reproduction

Base `WORK/control` and `WORK/control-build` on the exact [packed cross-item tree and Release build](../g2-packed-cross-item-2026-11-21/README.md#checks-and-reproduction). Link `WORK/box2d`, copy to `WORK/candidate` and apply `probe.patch` only in scratch. Run `bash build.sh WORK`, `bash run.sh WORK OUT`, then `python3 summarize.py OUT`. The runner verifies inputs and `.text`, uses GCC 16.2.1 Release, pinned Box2D, CPU 6 and quiet starts ≤40 °C and within 1 °C per pair. Power restored. Regenerate assembly with `python3 ../g1-unordered-empty-2026-11-21/codegen.py WORK EXISTING_OUTPUT_DIR --unordered`. Compile `copy-probe.cpp` with `c++ -std=gnu++20 -O3 -DNDEBUG -IWORK/VARIANT`, then run it with `control` or `invalid-copy`. Compile `phase.cpp` as in the [exact-pair recipe](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction), running only `uniform 2` and `clustered 2` for each binary.

## Handoff

**Do not turn the scratch copy into a build-only candidate.** Keep the exact control; staged G1 stays 8/16. A credible two-entry design now needs measured savings on the pair/result side as well. The [historical direct result variant](../g1-direct-pair-result-2026-11-19/README.md) is an attribution lead, not an additive credit or a fix to stack blindly. Preserve validation, extent, fresh owner/handles, allocation limits and all ordered/large-snapshot gates.
