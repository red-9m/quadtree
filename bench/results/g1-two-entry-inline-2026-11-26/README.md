# G1: inline two-entry pair path

**`int32 uniform n=2` now passes G1 in every run, with its upper bound at 1.578–1.760× brute.** Against `dc26b4f` in the same sessions it goes from 22.5–23.6 to 18.8–19.9 ns/scene. The 0- and 1-entry rows keep their gains. `double clustered n=1` drops from 1.79–1.87 to 1.62–1.74. This is a targeted improvement, not release acceptance.

| `int32 uniform n=2` | `dc26b4f` | Candidate |
|---|---|---|
| Combined full screen ×3 (81 rounds) | 1.875–1.908, upper ≤ 1.961 | 1.554–1.574 [≤ 1.625] |
| Combined target screen ×2 (117 rounds) | 1.847–1.872, upper ≤ 1.966 | 1.560–1.573 [≤ 1.664] |
| Isolated row ×2 (81 rounds) | **inconclusive** 1.974 [1.919–2.030]; pass 1.940 [≤ 1.995] | 1.462 [≤ 1.760]; 1.610 [≤ 1.654] |
| Per-type `int32` (45 rounds) | 1.815 [≤ 1.873] | 1.531 [≤ 1.578] |

All 24 requested rows pass in both target screens and in all four per-type runs on both binaries. In the full screens the candidate passes 23/24 n ≤ 2 rows in each of three runs; `dc26b4f` passes 22/24. All 32 rows from 8 to 64 entries pass, and all eight 4-entry rows still fail, on both binaries. Every warm library row allocates nothing. Per-run verdicts and absolute times: [gates.csv](gates.csv), [gate files](gates/), [snapshot CSVs](snapshots.tar.gz).

## Limits: `float clustered n=2` in the full screen

**Inconclusive in all three candidate full screens: 1.965–1.991 [upper 2.034–2.060], 22.6–22.9 vs 11.5 ns.** `dc26b4f` fails it in the same screens (2.087–2.150). The row passes in both target screens (≤ 1.828), the per-type run (1.612 [≤ 1.662]) and in a full screen with a longer output path (1.748 [≤ 1.806]). These inconclusive runs are not pooled into a pass.

The cause is harness heap placement, not row-specific library work. The adapter's `mInput` vector (112 bytes for two entries) is reused at the same address in every round. In the failing process its first page offset is `0xfb0`, so the second entry straddles a 4 KiB page. A [diagnostic build](input-address-diagnostic.patch) logs 81/81 rounds at `0xfb0` for that row with `--raw` and `0x2d0` without; [log](input-address.txt). The adapter then issues a page-crossing store on every scene. The [split-input probe](split-input-probe.cpp) puts the same input at fixed offsets: crossing offsets add 2.5–9 ns/scene on both binaries, for example `float` 15.98→22.86 (`dc26b4f`) and 16.67→21.57 ns (candidate) at `0xfb0`; [results](split-input-probe.txt). Brute force copies boxes from the fixture and does not see it. Placement depends on process history, including output path length:

| Full-screen output path | Candidate `float clustered n=2` | `dc26b4f` |
|---|---|---|
| 14 characters | inconclusive 2.061 [1.931–2.200] | inconclusive 2.075 [1.958–2.199] |
| 24 characters (×3) | inconclusive 1.965–1.991 | FAIL 2.087–2.150 |
| 86 characters | pass 1.748 [1.691–1.806] | pass 1.843 [1.797–1.891] |

The earlier full screen of `dc26b4f` passed this row and failed `int32 uniform n=2` instead. The adapter and fixtures are out of scope, so the library can only add margin. The remaining warm two-entry work is the identity atomic and the one data-dependent pair branch (below).

## Profile and change

Profiled in the real `quadtree-bench` process for `int32 uniform n=2`, unordered `index` only, about 25.6M warm scenes per run (`perf stat`, user mode); [profile.csv](profile.csv). Variants marked *diagnostic* break semantics and were never candidates.

| Binary (unordered `index`, `int32 uniform n=2`) | Cycles/scene | Instructions/scene | Mispredicts/scene |
|---|---:|---:|---:|
| `dc26b4f` | 108.5 | 375 | 0.4 |
| brute force (`dc26b4f`) | 53.9 | 229 | 0.2 |
| *diagnostic:* identity counter without the atomic | 106.2 | 372 | 0.4 |
| views inlined (`always_inline` view helper) | 102.3 | 329 | 0.4 |
| *diagnostic:* views inlined, pair dispatch returns before testing | 67.2 | 282 | 0.0 |
| *diagnostic:* views inlined, every candidate misses | 70.4 | 302 | 0.0 |
| *diagnostic:* views inlined, every candidate reports | 104.5 | 336 | 0.0 |
| views inlined + `flatten`, build unchanged | 89.8 | 319 | 0.4 |
| **candidate** | 91.3 | 318 | 0.4 |

- **The warm build was not the bottleneck.** Removing the `lock cmpxchg` saved about 2 cycles; its large sample share was skid onto the next instructions. In the samples, the build was about a third of the scene.
- **Pair dispatch cost about 40 of 108 cycles.** `visitSinglePair` was `noinline`, and `view()` was called out of line twice. Inlining only the views saved 6 cycles. Even then a reported pair cost 34 cycles more than a miss: the bench visitor ran behind a closure pointer, so its sink was read-modify-written in memory, in a frame with a stack protector. Mispredicts add the rest: 0.4 per scene for a 45 % hit rate.
- **Change 1, pair dispatch.** `visitSinglePair` becomes `[[gnu::always_inline, gnu::flatten]]`, guarded by `__has_cpp_attribute`. The single candidate, the views and the client callbacks inline into `forEachPair`, and from there into the caller. That removes the call, the 2× view construction and the closure indirection. Only this two-entry path is flattened; traversal keeps its own inlining. Overlap test, orientation by slot, predicate order, Stop, budget and counters are unchanged.
- **Change 2, warm flat build.** Validation reads each input box in place (`validBox`) instead of copying it to the stack first. The stored copy is computed from the input (`storedBox`: integers unchanged, floats `+0` to drop signed zeros). Ranks are tested after every box has been validated. This does not change any result: the general path that handles ranked input also returns `InvalidGeometry` for any invalid box. For `double n=1` this cuts 57.8→53.3 cycles and 245→227 instructions. `canonicalBox` keeps the same acceptance and result for the general path.

The ninth n=2 row still fails in some combined processes. The remaining avoidable costs, the identity atomic and the data-dependent pair branch, belong to the handle contract and the query semantics.

## Regression screens

- **0/1 entries:** every n=0 and n=1 row passes in every run on both binaries. Candidate n=1 is 0–1.2 ns faster (mean per row and screen type); n=0, whose code path did not change, ranges from 0.3 ns slower to 1.0 ns faster.
- **4 entries:** all eight rows fail in every full screen on both binaries; this is the next task. The mean index time is unchanged or lower: `int32` 53.4→52.9, `int64` 54.5→54.6, `float` 59.0→56.0, `double` 60.8→59.3 ns. A long-run `perf stat` gives `int32 uniform n=4` +4 cycles (233.7→237.7) and +6 instructions (validation loop). The tiny-pair traversal is instruction-identical; only its address moved.
- **8–64 entries:** 160/160 rows pass in each binary across the five full screens.
- **G2/G9:** the 45-round [`int32` follow-up](gates/resolve-cand-1.txt) and the 45-round [`int64` follow-up](gates/resolve64-cand-1.txt) pass all G2 and G9 rows on both binaries, twice for `int64`. The two five-round [large screens](gates/large-cand-1.txt) pass every G9 row. G2 has six inconclusive rows for the candidate and one for `dc26b4f`, at `int32` split-line n=100 and `int64` split-line n=100,000. Each is resolved as a pass above and is reported here separately. `int64 split-line-translated n=100000` stays 1.919–1.933 vs 1.845–1.847 at 45 rounds (limit 2). The unordered traversal for `int64` is instruction-identical. `buildGeneral` differs only in alignment padding, so this is placement, but it narrows the margin.

## Correctness

The contract is unchanged: validation, extent, handle invalidation, failure atomicity, pair semantics and zero warm allocations. The adapters, fixtures, process-error settings, reference engines and gate limits are also unchanged. Evidence: [verification.log](verification.log).

- [CTest 22/22](ctest.log), including exception-disabled tests, death tests and differential fuzzing.
- Clang 22 `index-test`: 0 failures with default flags and with the scalar fallback (`-U__SSE2__ -fno-exceptions`).
- **Unordered self-pair transcript:** [the semantics probe](../g1-two-direct-result-2026-11-21/semantics-probe.cpp) produces 29,600 lines, byte-identical to `dc26b4f` and to the candidate under ASan+UBSan.
- **Flat replacement transcript:** [the build probe](flat-build-probe.cpp) runs 160,000 builds of 0–6 entries, with adaptive and snapshot extents and with or without handle output. Inputs include reversed edges, NaN, ±∞, −0, extreme edges, ranks and duplicate ranks. It records the status and identity consumption, plus the extent, stored boxes and signed zeros, ranks, old/new handle validity and pair counts. The transcript is byte-identical to `dc26b4f` under GCC, GCC ASan+UBSan and the Clang scalar fallback.

## Reproduce

GCC 16.2.1, CMake 4.4.2 Release `-O3 -DNDEBUG`, native tuning off; Ryzen 9 5900HS, CPU 6, `powersave` governor, `performance` energy preference, HPET clock source. Benchmark executables: control `dc26b4f` `7bcd1cfe36486ab3cd318b6804e87e8042bbad610da5d55537bdbaa0c782fd93` (matches the published candidate), this commit `546d79c7035b2f5e1f0639c09b3ebb367f40a007bbc79d79ddd005ffc8fcd82b`. `detail/quadtree-index-impl.h` SHA-256 `af926eec56998f08c516af342270bd6035ab4f80ef0104c790ee6e93443a8c27`.

`bash reproduce.sh CONTROL_EXE CANDIDATE_EXE OUT` runs every screen above in order, using the unchanged [screen.sh](../g1-tiny-snapshots-2026-11-26/screen.sh). Gate exit 3 means fail and 4 inconclusive. The path rows used `OUT` paths of 14 and 86 characters; the other full screens used 24-character paths.
