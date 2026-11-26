# `int32` translated split-line G9: reuse the compact ordered walk

**G9 passes all 15 rows of the `int32` snapshot matrix.** The target (`split-line-translated n=100000`) passes in the matrix and in three independent 45-round runs. Worst 95% upper bound **3.586×** unordered (limit **4×**); **3.475×** across the 45-round runs. Ordered build plus pairs improves by 7–18% in the matched target runs. No adapter, fixture, engine, process-error or gate changes.

## Target results

The control is `ec67d81` ([four-entry change](../g1-four-entry-flat-2026-11-26/README.md)); the candidate adds [the ordered runtime patch](ordered-common.patch). Runs alternate which binary goes first. Each also includes unshifted `split-line n=100000`; the candidate passes that G9 row in all three runs. Ratios and intervals are per-process, not pooled. [All G9 rows and phase medians](g9.csv) · [raw samples, result CSVs and gate output](results.tar.gz).

| Run | Control unordered / ordered (ms) | Candidate unordered / ordered (ms) | Control G9 [95% CI] | Candidate G9 [95% CI] |
|---|---:|---:|---|---|
| 1 | 12.27 / 48.32 | 13.37 / 45.00 | 3.937 [3.823–4.054], inconclusive | 3.367 [3.267–3.470], pass |
| 2 | 13.54 / 56.93 | 13.76 / 46.48 | 4.203 [4.067–4.344], FAIL | 3.378 [3.283–3.475], pass |
| 3 | 12.66 / 49.01 | 12.85 / 42.80 | 3.872 [3.748–3.999], pass | 3.332 [3.237–3.431], pass |

Ordered pair-phase medians fall from 35.51–42.51 to 29.52–32.36 ms. The faster ratio is not solely a denominator effect: absolute ordered steps are faster in all three comparisons. The 9-round pilot also passes (3.435 [3.339–3.534]). Candidate G2 passes both scenes in each target run; for the translated row its worst upper bound is 1.860× grid (limit 2×). Exact ordered work on the target is 422,813 candidates, 4,279,010 node visits and 105,137 reported pairs, checksum `dc2fa1796e7c7b83`. Zero warm allocations.

## Snapshot matrix

Default 5-round `int32` snapshot matrix, 312 rows per binary including pathologies. Candidate G9 **15/15** vs control **12 pass, 2 inconclusive, 1 fail**. The translated target is 3.470 [3.357–3.586] vs control 4.212 [4.069–4.360]. G2 20/20, G3 9/9, G4 8/8 and G6 62/62 pass on both. Candidate G1 is 15 pass, 1 inconclusive (`uniform n=4`, 1.956 [1.875–2.041]); control has two n=4 inconclusives. The 298 uncapped matched rows agree on work counters, pairs, checksums and scene hashes; 14 capped rows are published but excluded from this equality check.

Two matched 81-round four-type G1 follow-ups at output path lengths 22 and 81 pass the `int32 uniform n=4` row (candidate upper bounds 1.783 and 1.823). Overall G1 is 127/128 on both binaries; the only inconclusive is the known `float clustered n=2` placement row in the shorter-path process. No failures; G6 passes everywhere.

The 64-step `int32` persistent regression slice (`split-line` and `mixed-size`, n=10000, static and 1% small/teleport motion) passes all 24 applicable hard gates on both binaries. This is G9 acceptance evidence, not a full release rerun; the full persistent matrix and hosted portability checks remain pending.

## Change and correctness

The [control profile](profile-control.txt) (`perf record -e cycles:u -F 799` on the real ordered benchmark) attributed 52.74% of sampled cycles to `walkOrdered`. In the [candidate profile](profile-candidate.txt), the fast walk inlines into `traversePairs`; the recursive walk no longer appears above 1%. The large ordered `int32` default-predicate query can now use the existing `int64` compact DFS walk and rank-ordinal rows. It requires a tree, at least 4,096 entries, reserved build-key scratch, both root spans fitting in signed 32-bit gaps, and a filter bit shared by every entry's category and mask. The bit proof is local to the query, not added to builds or cached across mutation.

The tree copy uses only reserved build-box scratch and requires contiguous node entries. Otherwise the common path uses the incremental walk, also used when a row's remaining budgets cannot cover its worst-case charges. Wide spans, no common filter bit, absent build-key scratch, moved-set queries, custom predicates and non-SSE2 builds keep the general path. Rank order, candidate/node charges, unfinished-row discard, Stop, and workspace capacities are unchanged.

The extended `orderedIntegerRows` test compares ordered output prefixes and counters against the general predicate path at 4,200 entries for both integer widths, including full-width ranks, dense and sparse rows, finite candidate/node budgets, visitor Stop, translated geometry near the signed limit, insufficient optional scratch and fallback after mutations. It checks zero warm allocations. [CTest](ctest.log): 22/22. [Clang](verification.log): default, `-U__SSE2__ -fno-exceptions`, and ASan+UBSan, all with zero failures.

## Reproduce

Ryzen 9 5900HS, CPU 6 (sibling 7 idle), `powersave`, EPP `performance`, HPET. GCC 16.2.1 and CMake 4.4.2 Release (`-O3 -DNDEBUG`), native tuning off, pinned Box2D v3.2.0. Build with `-DQUADTREE_BUILD_BENCHMARKS=ON`; reverse [the runtime patch](ordered-common.patch) in a separate source tree for the control. [Environment and source hashes](environment.txt).

- `bash reproduce.sh CONTROL_EXE CANDIDATE_EXE OUT`: three paired 45-round runs and the default `int32` snapshot matrix on both binaries, with frozen limits and process errors.
- `bash regress.sh CONTROL_EXE CANDIDATE_EXE OUT LONG_OUT`: G1 at two output path lengths and a 64-step representative `int32` persistent slice.
