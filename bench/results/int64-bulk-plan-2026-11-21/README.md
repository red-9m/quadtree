# 09 · `int64` snapshots: use the integer bulk planner and ordered paths

**Valid candidate, not yet staged.** `int64` now runs the top-down bulk planner, commit prefetch and ordered-pair fast paths that Task 03 had limited to `int32`. `int64` `index` is 5–34% faster and `index-ordered` 14–32% faster on every G2 row; work counts are identical. `int64` G2 moves from 5 pass / 11 fail / 4 inconclusive to 14 pass / 1 fail / 5 inconclusive in the matched 5-round run. After the 45-round rerun, one row still fails: `uniform` 10⁵ at 2.09× [2.02–2.15]. G9 now passes 15/15. The `int32` snapshot code is unchanged apart from alignment padding.

## Diagnosis

From the [release run](../release-acceptance-2026-11-21/README.md), `int64` matched `int32` in candidates and node visits on every G2 row. Its pair phase was only 1.03–1.33× slower, but the build phase was 1.4–2.5× slower. The cause: `buildPrepared` used `planBulk` (the top-down plan) only for `std::same_as<T, std::int32_t>`, so `int64` fell back to `makeBuildKey` + radix sort + `countNodes`. The ordered self-pair fast paths (radix rank order, `walkOrdered`) had the same `int32`-only guard. Once the build got faster, those ordered paths pushed G9 over 4× on three 10⁵ rows ([stage-A run](screen-A-data/)).

## Change

[int64-paths.patch](int64-paths.patch) (`detail/quadtree-index-impl.h`): seven guards change from `std::same_as<T, std::int32_t>` to `IntegerCoordinate<T>`.

| Location | What it enables for `int64` |
|---|---|
| `planBulk` requires-clause | the top-down bulk planner |
| bulk-plan dispatch in `buildPrepared` | the planner on snapshot builds |
| spatial commit prefetch | prefetch while committing entries |
| three branches in `orderedSelf` | radix rank ordering, ordered tree walk, radix row sort |

The algorithms already worked on integers through `detail::span`, `std::midpoint` and 64-bit unsigned spans. The packed SSE2 cross-item test stays `int32`-only; plain SSE2 has no 64-bit compare.

Two other ideas were screened and rejected ([screens](screens/); `compare.py` prints the A/B):

| Variant | Screen file | Result |
|---|---|---|
| A: planner only | `control-1` → `candidate-1` | the speedup above, before the ordered fix |
| B: branch-free scalar cross-item overlap | `candidate-2` → `candidate2-2` | +4–20% slower |
| C: separation-first inline overlap | `candidate-3` → `candidate3-3` | −18% to +10%, mixed |
| final: A + ordered paths | `candidate-4` → `candidate4-4` | ordered rows −9% to −26% |

## Verification ([checks/](checks/))

- **`int64` vs control** ([verify.sh](verify.sh), [log](checks/verify-int64.log)): [equivalence probe](equivalence.cpp) matches on extremes and 5 scenes × 10³/10⁵ (plus `uniform` 10⁴). It compares topology (`forEachNode` cells, bounds, counts, summaries), handle-to-rect mapping, ranks, and exact unordered and ordered pair sequences, for full and moved-set queries.
  - The exact dense-slot stream from the [dense-order probe](dense-order.cpp) differs. Within a leaf, the planner assigns dense slots in a different order from the key sort; that order is not a contract. The candidate's dense-order stream is identical under ASan/UBSan.
- **Budget contract** ([budget-contract.cpp](budget-contract.cpp), [log](checks/budget-contract.log)): 762,210 checks, also clean under ASan/UBSan, covering 6 scenes × 10²–10⁴ × both boundaries, full and moved, ordered and unordered. Every budget-limited ordered result is an exact prefix of the complete sequence. Unordered results are duplicate-free subsets.
- **`int32` unchanged**:
  - The equivalence and dense-order probes match exactly ([log](checks/verify-int32.log); dense-order SHA `941f3592…`, the Task 03 value).
  - [int32-codegen.py](int32-codegen.py) compares all 47 `int32` library/adapter symbols after removing alignment nops: 45 identical. The 2 that differ are the ordered `PersistentRunner::pass`, a persistent inlining choice ([log](checks/int32-codegen.log)).
- **CTest:** [21/21](checks/tests.log). **Full differential fuzz:** [passes](checks/fuzz.log). The CMake build's `.text` equals the screened candidate ([text.sha256](text.sha256)).

## Matched results ([data/](data/))

[run.sh](run.sh), CPU 6, `performance`, quiet starts. `control` is the release binary (`.text` `97d8835e…`). Order: control → candidate for `int64`, then the same for `int32`, each a full snapshot matrix at 5 rounds. Afterwards, a candidate-only 45-round `resolve` of `uniform`/`split-line`/`split-line-translated` at 10²–10⁵. Every run exits 0 with matching checksums, candidates and node visits.

| Gate (`int64`) | Control | Candidate | Candidate after 45-round resolve |
|---|---|---|---|
| G1 | 8 pass, 8 fail (0–4 entries, upper ≤ 3.23) | 8 pass, 8 fail (0–4 entries, upper ≤ 3.13) | — |
| G2 | 5 / 11 / 4 | 14 / 1 / 5 | 1 fail, 4 unresolved |
| G9 | 14 / 0 / 1 | 15 / 0 / 0 | 15 pass |

Remaining `int64` G2 rows, 45-round values:

| Row | Ratio [95%] | Limit |
|---|---|---:|
| `uniform` 10⁵ | **2.085 [2.021–2.152] FAIL** | 2 |
| `split-line-translated` 10⁵ | 2.034 [1.975–2.094] | 2 |
| `split-line` 10⁴ | 1.958 [1.903–2.015] | 2 |
| `uniform` 10³ | 3.993 [3.853–4.139] | 4 |
| `split-line-translated` 10² | 4.064 [3.950–4.182] | 4 |

The last two are the `int32` exception rows; `int64` sits at the same ratios as `int32` there. All of G3, G4 and G6 pass.

**`int32` sanity:** the matched `int32` diff is 280 of 298 rows `same`, 13 slower and 5 faster, geometric mean 1.001. The slower rows include unchanged baselines (`grid` +6–8%, `sweep` +5–9%) and identical ordered code, so they come from layout or noise, not code. Candidate `int32` G2 is 18 pass / 2 inconclusive (the two exception rows).

## Handoff

- **Stage it:** this is a valid change that closes most of `int64` G2 and all of `int64` G9. Promoting it needs the usual checks on a commit: hosted CI and `int32` G1/G2/G9 rows rechecked under the release method.
- **`uniform` 10⁵ still needs about 5–8%.** The remaining build gap (6.4 vs 5.4 ms) and pair gap (7.5 vs 6.7 ms) against `int32` match the doubled box size (32 B vs 16 B). Closing it likely needs compact per-node or relative-coordinate data during planning and traversal, a design change.
- **Decision:** whether the `int32` G2 exception for `split-line-translated` 10² and `uniform` 10³ should also cover `int64`.
