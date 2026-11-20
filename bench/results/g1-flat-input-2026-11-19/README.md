# 02 · Small snapshots — validate contiguous input in place

**G1 remains open.** The retained change avoids copying warm, unranked, contiguous integer `BuildEntry` input through workspace descriptors when the index already has capacity and will remain flat. It validates edges and computes the extent before acquiring a fresh identity. Commit reads the stable, nonaliasing input, runs no client code, and leaves the same slot and handle layout. Ranked input falls back before any state change; projected, consuming, fixed-extent, floating-point, and custom-identity routes still use the existing implementation. Workspace and output checks are unchanged.

## Measurements

The [phase probe](phase.cpp) puts noinline wrappers around adapter build and pair dispatch, but keeps the original order (build one scene, scan it, then replace it). It verifies exact pair identities against brute and checks warm allocations. In the [weighted sample summary](phase-summary.csv), build preparation and replacement own about 52% of cycles at two entries and 39–40% at four; scan and checksum account for about 38% and 51–52%, respectively. Source samples in descriptor capture and the commit entry loop justified trying to remove the descriptor round trip. The probe changes inlining and is for attribution, not for gate verdicts.

The first variant [fused all four store writes](fused.patch) into one loop and regressed sharply. Sampled instructions in its commit were mostly GCC's runtime alias checks before the vectorized loop, even at one entry. The retained version uses separate loops for boxes, payloads, metadata, and slots. It avoids that setup cost and improves two- and four-entry steps without sacrificing larger passing batches.

| G1 run | Pass | Fail | Inconclusive |
|---|---:|---:|---:|
| [Unchanged, 18 rounds](before/gates.txt) | 7 | 6 | 3 |
| [Fused writes, 18 rounds](fused/gates.txt) | 7 | 8 | 1 |
| [Retained, 18 rounds](split/gates.txt) | 8 | 6 | 2 |
| [Retained, 27-round repeat](split-repeat/gates.txt) | 8 | 6 | 2 |
| [Retained, full snapshot slice, 18 rounds](full/gates.txt) | 9 | 6 | 1 |

On the matched 18-round G1-only runs, two-entry steps fell from 48–50 to 38–39 ns/scene; four-entry steps fell from 94–108 to 76–86 ns/scene. Both eight-entry rows passed again on the repeat. Both scenes still fail at 1, 2 and 4, and the empty cases still straddle the limit on the repeat. See the [per-row estimates and intervals](g1-comparison.csv).

[All 16 verification checks passed](tests.log), including exception-disabled tests and the quick differential fuzzer. The prepared-build test now checks contiguous integer input, failed validation with no state or handle-output changes, duplicate-rank fallback, and allocation-free replacement. The benchmark cross-checks matched, with zero warm allocations for all library rows. In the full slice, G2 remained 8 pass / 6 fail / 1 inconclusive, and G3/G4/G6/G9 passed. Full-slice G1 passed `uniform` at zero entries, but that pass did not also hold in the targeted repeat.

**Next:** stay on 02. Investigate the singleton fixed cost and candidate dispatch at two and four; do not optimize descriptor copying again. The passed eight-entry rows are regression checks, and the empty intervals are not passes.

## Reproduction

Use the prior [source inputs](../g1-flat-commit-2026-11-18/README.md#reproduction) for the unchanged build, or add `fused.patch` to those inputs for the rejected variant. For the retained build, start at [HEAD](source-head.txt), apply the [tracked delta](source-tracked.patch), extract the [untracked inputs](source-untracked.tar.gz), then verify the [source checksums](source-inputs.sha256). Use the [task 01 Release build recipe](../current-snapshot-2026-11-18/README.md#environment-and-reproduction) with the pinned Box2D checkout. See [environment](environment.txt).

`bash reproduce.sh /path/to/quadtree-bench /fresh/outdir 18` runs the matched G1 slice; use `27` for the targeted repeat or `18 full` for the full snapshot slice. Run directories contain summary CSV, compressed raw CSV, evaluator output, and exit codes. For the probe, follow the [task 01 profiling recipe](../current-snapshot-2026-11-18/README.md#environment-and-reproduction) with `phase.cpp`, on both scenes at 2 and 4 entries for the unchanged build and on `uniform` at 1, 2 and 4 for the fused build. The [weighted symbol and source histogram](profile-by-location.csv.gz) retains sample counts and period weights.
