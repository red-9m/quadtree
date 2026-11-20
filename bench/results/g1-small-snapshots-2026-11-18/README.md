# 02 · Small snapshots — prepared replacement

**G1 remains open.** This pass removes overhead from empty queries and builds, and from replacement when storage is already sufficient. It does not change the benchmark adapter, baselines, thresholds, topology, workspace requirements, or handle issuance. Each nonempty replacement still acquires a fresh identity before destroying old entries. Failed validation, allocation, or identity acquisition leaves the entries, handles, and output untouched.

## Method and evidence

- Before: [task 01 nine-round CSV](../current-snapshot-2026-11-18/repeat/snapshot.csv), [raw](../current-snapshot-2026-11-18/repeat/snapshot-raw.csv.gz), [gates](../current-snapshot-2026-11-18/repeat/gates.txt).
- After: [nine-round CSV](snapshot.csv), [raw](snapshot-raw.csv.gz), [gates](gates.txt).
- Repeat: [eighteen-round CSV](repeat/snapshot.csv), [raw](repeat/snapshot-raw.csv.gz), [gates](repeat/gates.txt). The repeat was triggered by the inconclusive `n=0` and `n=8` rows.
- [All 16 before/after comparisons](g1-comparison.csv) include absolute estimates in ns/scene, uncertainty, ratio 95% intervals, and the frozen evaluator's verdicts. The nine-round runs use identical settings. The eighteen-round run differs only in round count and output paths.

The original [task 01 invocation](../current-snapshot-2026-11-18/README.md#environment-and-reproduction) is unchanged except for using `--rounds=9` and `--rounds=18`. It measures the full build-plus-pairs step at batch 256. See [environment](environment.txt) and the CSV headers: Ryzen 9 5900HS, GCC 16.2.1, same Release flags, pinned Box2D v3.2.0. No persistent measurements or acceptance claims.

## Results and handoff

| G1 verdict | Before (9 rounds) | After (9 rounds) | Repeat (18 rounds) |
|---|---:|---:|---:|
| Pass | 6 | 6 | 6 |
| Fail | 10 | 6 | 6 |
| Inconclusive | 0 | 4 | 4 |

On the matched nine-round runs, empty snapshots improved from 18.9 to 9.8 ns/scene. Singletons improved from about 61 ns to 34–35 ns/scene, still about 5× brute. Both scenes still fail at 1, 2 and 4 entries, are inconclusive at 0 and 8, and pass at 16, 32 and 64. Both after-runs exited `0` for cross-checks and `3` for the gate report; pair counts, checksums and fixture hashes matched the baseline. All 68 library rows per run remained allocation-free once warm. The G2 verdict counts were unchanged from task 01's repeat, and all measured G3/G4/G6/G9 rows passed.

The same [attribution probe](../current-snapshot-2026-11-18/README.md#environment-and-reproduction) was rerun against the new headers. All probe cases checked exact pair identities and zero warm allocations. In the [weighted sample histogram](profile-by-location.csv.gz), `buildPrepared` still accounts for about 86% of singleton cycles, 56% at two entries and 44–46% at four. Top locations include topology reset, entry-store clear, and descriptor handling; those are diagnostic hints, not isolated costs (inlining and sample skid affect attribution). At eight entries, overlap and filter tests are prominent. See [probe output](profile.log); its timings are not gate timings.

**Next:** stay on 02. Isolate the reset and commit costs for an already-flat prepared index, where the remaining confirmed failures are, before choosing another change. Do not trade away validation, fresh handle identity, or failure-order guarantees to reach the limit. Recheck 0 and 8 with the next candidate; the repeat is not a pass.

## Verification

[16 CTest checks passed](tests.log), including the normal and exception-disabled Index/EntryStore tests, invalid-handle death tests, quick differential fuzzing, and the gate evaluator fixture. Added a prepared hard-capacity replacement test covering sized and unsized ranges, old-handle invalidation, untouched output suffixes, and empty/singleton dispatch. The identity-exhaustion test now checks both the allocation and reuse paths.

**Tested source:** start from [HEAD](source-head.txt), apply [the tracked delta](source-tracked.patch), and extract [untracked build inputs](source-untracked.tar.gz) at the checkout root. Verify [input checksums](source-inputs.sha256). Compared to task 01 only `quadtree-index.h`, `detail/quadtree-index-impl.h`, and `tests/index-test.cpp` differ among the build inputs.
