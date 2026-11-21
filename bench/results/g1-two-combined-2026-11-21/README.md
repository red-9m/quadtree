# 02 · Combined two-entry candidate: direct result + flat build dispatch

**The stop rule is met. Two-entry G1 cannot clear 2× within the current contract and adapter.** The best valid combination saves only 4.4–5.0 ns/scene at two entries; clearing 2× needs about 10. All two-entry rows still fail (2.281–2.382×), as do every 1- and 4-entry row and three of the four empty rows. The candidate also slows ordered 2/4/8 by 13–22 ns/scene (resolved in all 12 comparisons). Nothing staged; staged G1 stays 8/16 and Task 08 stays blocked.

## Candidate

[combined.patch](combined.patch) against the exact packed cross-item control is two parts:

- [direct.patch](../g1-two-direct-result-2026-11-21/direct.patch): tiny unordered `forEachPair` counts directly into the returned `QueryResult`.
- [build-dispatch.patch](build-dispatch.patch): for prepared builds, `buildRange` now tries the existing warm integer flat path (`buildFlatInteger`) itself, before calling the general `buildPrepared` frame.
  - The order of checks is unchanged: entry and slot capacity, small-set threshold, non-Fixed extent, workspace capacity (`InsufficientWorkspace`), the active-workspace guard, then the flat build.
  - All contract work stays: validation, rank fallback, extent publication, a fresh owner from the module counter, slot reset, handle output and allocation-free commit.
  - Ranked or ineligible input falls through to the unchanged `buildPrepared`. Unprepared builds are unchanged.
  - An earlier draft skipped the flat retry inside `buildPrepared` via a template flag. GCC then inlined the whole general path into an out-of-line `buildRange`, so the frame was not avoided; that draft was dropped. In the final build `buildRange` is inlined into the runner and calls `buildFlatInteger` directly.

The public API, adapter, runner, fixtures, brute baseline and gate limits are unchanged.

## Preflight

- [CTest](tests.log): 21/21 pass on the final source, including the build/failure, nonthrowing, death and quick fuzz tests.
- [Build probe](build-probe.cpp): 3,000 indexes run 6 builds each, covering:
  - all four extent modes and both small-set thresholds;
  - prepared and unprepared builds, workspace capacities 0–7 and short handle outputs;
  - ranks with duplicates, invalid geometry, old-handle invalidation and fresh handles;
  - extent, entries and pairs after each build.

  Control and combined transcripts are byte-identical (21,000 lines; statuses Ok/2/4/InsufficientWorkspace/6/12 = 10,749/688/294/4,538/1,552/179). The [direct-result semantics probe](../g1-two-direct-result-2026-11-21/semantics-probe.cpp) is unchanged. Both are clean under ASan and UBSan. [Log](probes.log).
- [Exact pairs](exact-pairs.log): all 16 rows match brute with zero warm allocations.
- [Codegen](codegen.log) · [callees](codegen-callees.log):
  - Ordered `LibraryIndex::pairs` matches the control.
  - Both runner steps change, because they now inline the flat dispatch.
  - The ordered step also gains a call to `buildFlatInteger` before `buildPrepared`. It returns at the first ranked entry, and the flat attempt inside `buildPrepared` runs again.
  - `buildFlatInteger` is now its own symbol; it was previously inlined into `buildPrepared`.

## Matched G1 results

Both binaries are built with `-falign-functions=64`; the control is the [layout-control](../g1-layout-control-2026-11-21/README.md) aligned control (same ELF hash). Frozen G1 workload on CPU 6, HPET, `performance`, quiet starts ≤40 °C and within 1 °C per pair. Order: 81 rounds control → candidate, then 117 rounds candidate → control. In each run, all 96 rows match between binaries: pairs, checksums, fixture hashes, candidates, node visits, memory and allocations. Library engines allocate zero when warm. [Verification](diagnostic/verification.log) · [16 rows per run](diagnostic/g1-comparison.csv) · [library A/B, n=0–8](diagnostic/library-change.csv) · [raw samples](diagnostic/).

| Run | Control G1 pass / fail / inconclusive | Combined G1 pass / fail / inconclusive |
|---:|---:|---:|
| 81 | 8 / 7 / 1 | 8 / 8 / 0 |
| 117 | 8 / 8 / 0 | 8 / 7 / 1 |

Unordered `index` change, ns/scene. Each cell is control → combined, with the saving in parentheses; every one is resolved faster except the empty rows:

| Entries | `clustered` 81 | `uniform` 81 | `clustered` 117 | `uniform` 117 | Combined G1 ratio (all fail) | Point gap to 2× |
|---:|---:|---:|---:|---:|---|---:|
| 0 | unresolved (≤0.2) | unresolved | unresolved | unresolved | 1.963–2.140 (`uniform` 117 inconclusive) | −0.2–0.7 |
| 1 | 19.6 → 18.4 (1.2) | 19.5 → 18.4 (1.1) | 19.5 → 18.4 (1.1) | 19.5 → 18.4 (1.1) | 2.831–2.875 | 5.4–5.6 |
| 2 | 34.1 → 29.3 (4.8) | 34.3 → 29.6 (4.7) | 34.2 → 29.2 (5.0) | 34.4 → 30.0 (4.4) | 2.281–2.382 | 3.6–4.8 |
| 4 | 56.9 → 49.1 (7.8) | 60.4 → 53.2 (7.2) | 56.2 → 49.2 (7.0) | 59.6 → 52.2 (7.4) | 2.252–2.429 | 5.5–9.4 |

Rows 8–64 pass on both binaries in both runs; the 8-entry `index` change is unresolved (+1.4 to +4.7 ns faster).

Ordered `index-ordered` is **resolved slower** at n=2 (13.0–15.3 ns, +19–24%), n=4 (19.0–21.7 ns, +14–17%) and n=8 (17.4–19.9 ns, +5–6%), in both scenes and both runs. The cause is not attributed. The extra `buildFlatInteger` call and retried flat check are the visible code difference, but they do not obviously account for 13–22 ns. The 0/1-entry ordered rows are unresolved.

## Interpretation

- **Little build-side gain:** compared with the [direct result alone](../g1-layout-control-2026-11-21/README.md) (3.3–4.7 ns at two entries, same alignment), the build dispatch adds only about 0.5–1 ns there; at four entries it adds about 2–3 ns. Avoiding the general `buildPrepared` frame does not reach the 9–10 ns that the [invalid warm copy](../g1-two-copy-headroom-2026-11-21/README.md) saved. That probe skipped validation, extent, owner and slot work. Earlier isolations ([singleton phases](../g1-singleton-phases-2026-11-19/README.md), [replacement profile](../g1-replacement-profile-2026-11-19/README.md), [slot writes](../g1-two-slot-headroom-2026-11-21/README.md)) each found ≤0.7 ns for those pieces taken separately.
- **Stop rule:** the combined saving is under 10 ns/scene, so two entries cannot clear 2× within the current build contract, `LibraryIndex` adapter and runner. Two-entry ratios fall to 2.28–2.38×, but the points still sit 3.6–4.8 ns above 2×. The 1- and 4-entry rows are 5.4–9.4 ns away, and the empty rows remain borderline.
- **Not stageable:** the ordered regression alone rules this candidate out.

## Reproduction

Set `WORK` to the path stored in `/tmp/quadtree-g1-combined.current`. It links `control` (packed cross-item tree), `control-build` (the aligned control from layout control), `candidate` (`combined.patch` applied), `box2d` and `frozen-control-build`. Run from this directory:

1. `bash build.sh WORK`; it verifies [candidate-inputs.sha256](candidate-inputs.sha256) and writes `WORK/candidate-a64`.
2. `python3 ../g1-unordered-empty-2026-11-21/codegen.py WORK . --unordered`
3. `python3 ../g1-two-direct-result-2026-11-21/codegen-names.py .`
4. `python3 ../g1-layout-control-2026-11-21/codegen-callees.py WORK 'QuadTree::Index<unsigned int, int, QuadTree::Unchecked, true>::'`
5. `bash run.sh WORK diagnostic`
6. `python3 summarize.py diagnostic`

Verification steps:

- **CTest:** configure the candidate tree with `-DCMAKE_BUILD_TYPE=Release -DQUADTREE_BUILD_TESTS=ON -DQUADTREE_BUILD_BENCHMARKS=ON -DQUADTREE_BENCH_BOX2D=ON -DQUADTREE_BENCH_NATIVE=OFF -DFETCHCONTENT_SOURCE_DIR_BOX2D=WORK/box2d -DFETCHCONTENT_FULLY_DISCONNECTED=ON`, then run `ctest`.
- **Probes:** compile `build-probe.cpp` and `semantics-probe.cpp` with `-std=gnu++20 -O3 -DNDEBUG -IWORK/VARIANT`, and also with `-O1 -fsanitize=address,undefined`.
- **Exact pairs:** build `phase.cpp` per the [exact-pair recipe](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction) and run all 16 rows.

## Handoff

**Task 02 is blocked on a decision, not on another probe.** None of the valid candidates so far — direct result, flat dispatch, extent, identity inlining, store fusion, slot writes — brings 0/1/2/4 entries near 2× brute. The remaining gap is the contractual build work plus the fixed adapter and runner overhead. Moving further needs an owner decision outside this task's rules:

1. accept G1 failures for 0–4 entries, with this evidence, in the 08 sign-off;
2. revise the frozen G1 limit or the small-row definition (gate change);
3. change the contract or adapter, for example a cheaper replacement API for tiny snapshots.

Do not continue build-only or pair-only micro-probes on 02.
