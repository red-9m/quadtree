# 03 · Cache ranks for long ordered partner rows

**Rejected.** Caching ranks and keeping introsort made the `mixed-size` 100,000-entry ordered step 3.7–3.9% slower across both repeats. G9 worsened; G2 still fails. Runtime source is unchanged.

## Candidate

Apply [cached-row-sort.patch](cached-row-sort.patch) on top of the [grouped child screen](../g2-grouped-child-screen-2026-11-20/README.md). For full ordered `int32` self queries, rows longer than 64 load `(uint64_t rank, uint32_t dense ID)` into contiguous `SpatialKey` slots in the inactive build sort buffer (`Workspace::mTemp`). Introsort compares the cached ranks, then writes back dense IDs. The existing `WorkspaceReservation::buildEntries` capacity must cover the row; otherwise the index sort runs. No new allocation, capacity requirement, workspace field, source sort, short-row algorithm, or moved-set path.

## Frozen snapshot slice

Counts are pass / fail / inconclusive. G0, G3, G4 and G6 pass in all four runs. All 68 library rows per run complete with zero warm allocations.

| Rounds | G2 screened → cached | G1 screened → cached | G9 screened → cached |
|---:|---:|---:|---:|
| 9 | 9 / 5 / 1 → 9 / 3 / 3 | 8 / 6 / 2 → 8 / 8 / 0 | 14 / 1 / 0 → 12 / 1 / 2 |
| 15 | 9 / 5 / 1 → 9 / 3 / 3 | 8 / 6 / 2 → 8 / 8 / 0 | 14 / 1 / 0 → 13 / 1 / 1 |

`mixed-size`, `index-ordered`, 100,000 entries. Times in ms; build/pairs are phase medians, not additive step estimates.

| Rounds | Step before → after | Build before → after | Pairs before → after | G9 before → after (limit 4×) |
|---:|---:|---:|---:|---:|
| 9 | 174.730 → 181.222 | 16.268 → 15.456 | 158.527 → 165.985 | 4.210 [4.089–4.335] → 4.400 [4.271–4.533] |
| 15 | 174.582 → 181.368 | 16.620 → 15.100 | 157.344 → 165.387 | 4.197 [4.078–4.320] → 4.345 [4.222–4.473] |

- The matched ordered step regression resolves in both repeats (candidate/control 1.037 [1.008–1.068] and 1.039 [1.009–1.069]). The 15-round unordered `mixed-size` interval includes no change. Rows not using the cache also moved; at 100,000, `split-line` ordered steps slowed ~4% despite no long rows. These full-step and pair-phase results reject the patch, but do not isolate sort-body cost from code generation and layout.
- Both empty G1 rows go from inconclusive to fail in both repeats; the two split-line G9 rows go from pass to inconclusive at nine rounds (`split-line-translated` also at 15). No G2 pass gained; the candidate still fails all three 100,000-entry G2 rows and leaves the corresponding 10,000-entry rows inconclusive.
- [All G2 rows, counts and phases](g2-comparison.csv) · [Matched full steps and phases](step-change.csv) · [9-round screened](9-control/gates.txt) / [cached](9-candidate/gates.txt) · [15-round screened](15-control/gates.txt) / [cached](15-candidate/gates.txt).

## Correctness

[Dense probe](dense-prefix.log): exact statuses, predicates, callbacks and counters match with full, partial and zero cache capacity, full-width ranks (including `0` and `UINT64_MAX`), node/candidate budgets and visitor stops. Prepared queries allocate nothing. The [finite-prefix](budget-prefix.log) and [full/moved-set](equivalence.log) probes also match. [Full-slice verification](verification.log) matches uncapped results, work, allocations and footprint. [CTest: 21/21](tests.log).

## Reproduction and handoff

Reconstruct the screened source and copy it, applying `patch -p1` with `cached-row-sort.patch` only to the copy. Verify the [screened input hashes](control-inputs.sha256), [candidate input hashes](candidate-inputs.sha256) and [`.text` hashes](text.sha256). Build GCC 16.2.1 Release with the same Box2D and flags as the [screening experiment](../g2-grouped-child-screen-2026-11-20/README.md#reproduction-and-handoff). Run `bash verify-dense.sh SCREENED_SOURCE CACHED_SOURCE`, the earlier prefix and full/moved probes, and CTest. Use CPU 6 and the full-slice runner in [recorded order](run-order.csv), with the [recorded environment](environment.txt); [restore the profile](restored-mode.txt) afterward. Compressed raw samples are in each run directory.

**Next:** drop this introsort patch. Use the grouped-screen build to test a long-row radix sort over the full 64-bit ranks with both reserved build buffers and the same no-scratch fallback. Attribute row-sort time once before the full slice so a change in code layout is not mistaken for a sort gain. G2 still needs an unordered improvement; retest G2 and G9 together. Release acceptance remains blocked.
