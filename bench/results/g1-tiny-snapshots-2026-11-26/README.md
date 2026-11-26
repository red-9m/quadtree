# G1: zero, one and two entries

**24/24 requested G1 rows pass in separate per-type processes, up from 2/24.** Both `uniform` and `clustered` pass for `int32`, `int64`, `float` and `double`, without using an exception. A second per-type run also passes 24/24. This is a targeted result, not release acceptance: the combined-type screens did not reproduce every pass.

| Entries | Control ns/scene | Candidate ns/scene | Worst upper ratio vs brute |
|---:|---:|---:|---:|
| 0 | 10.4–11.0 | 4.4–4.7 | 1.069 |
| 1 | 19.2–23.5 | 10.5–12.5 | 1.918 |
| 2 | 34.3–38.5 | 21.9–24.7 | 1.823 |

Ranges above span the eight rows at each size, on the per-type 45-round release-style slice (default batch 1024, five engines). [All 24 rows](per-type.csv) give absolute times, before/after intervals and verdicts, and an independent 117-round, 256-scene, six-engine repeat. Its worst upper bound is 1.977. [Raw per-type samples](per-type-samples.csv.gz). Across matched control/candidate rows the fixture hashes, pair counts, checksums and work counters agree; all warm library rows allocated zero.

**Limits of the result.** The [four-type 81-round screen and 117-round target repeat](g1.csv) each pass 23/24 requested rows. In the full screen `int32 uniform n=2` still fails at 2.092 [2.038–2.148] (24.9 vs 11.9 ns); it passes at 1.839 [1.780–1.900] in the target repeat. An [isolated 81-round rerun](int32-uniform-2-gates.txt) was **inconclusive: 1.983× [1.929–2.039]** (23.0 vs 11.6 ns/scene, zero warm allocations). `double clustered n=1` goes from pass to inconclusive [1.753–2.053]. Do not pool these into passes. The full screen passes all 32 rows at 8–64 entries and fails all eight at four entries. Process-error settings, fixtures, adapters, reference engines and gate limits are unchanged.

## Runtime and correctness

- Return directly for empty and singleton self-pair queries, retaining the singleton ordered-rank check. For exactly two flat, unordered entries charge filters/budget first, test the only possible pair, and construct callback views only on a hit. Tree traversal and ordered workspace rules still apply when required.
- Dispatch warm flat replacements before the general build frame, specializing the existing commit for lengths one and two. Workspace buffers grow as a transaction. Input validation, extent, owner acquisition, handle invalidation, ranked fallback and failure atomicity are unchanged. SSE2 validates both floating-point axes together; non-SSE2 retains the scalar check.
- [CTest: 22/22](ctest.log), including exception-disabled tests and differential fuzzing. The new `preparedFloatBoxes` regression exercises signed zero, finite extremes, inverted axes, infinities and NaNs in each field, and unchanged handles/extent on failure. The [unordered self-pair transcript](../g1-two-direct-result-2026-11-21/semantics-probe.cpp) is byte-identical to control under ASan/UBSan (29,600 lines, instrumentation on/off, budgets, predicates, orientation, Stop).

- Clang 22 scalar fallback (`-U__SSE2__ -fno-exceptions`): `index-test: 0 failures`. The [exact-pair probe](../g1-flat-input-2026-11-19/phase.cpp) matched brute on all 256 `int32` fixtures in each requested row and allocated nothing in its warm loop.

## Regression screen

[Five-round `int32`/`int64` snapshot screen](large-gates.txt), 100–100,000 entries: no hard failures; one row each inconclusive for G2, G4 and G9. The [45-round follow-up](resolve-gates.txt) resolves G2/G4 but leaves both `int32` split-line G9 rows inconclusive at 100,000. A separate matched process pair gives [control: pass / inconclusive](resolve-control.txt) and [candidate: pass / pass](resolve-repeat.txt). No persistent or full release recheck; retain the published acceptance decision.

## Reproduce

GCC 16.2.1, CMake Release `-O3 -DNDEBUG`, native tuning off, pinned Box2D v3.2.0; Ryzen 9 5900HS, CPU 6, `powersave` governor, `performance` energy preference, HPET clock source. Control runtime matches `fa8cfe3`; candidate implementation SHA-256 `3198189c2926b40cf9a16b1873a9c5eef6364655868cdfef863f38818d8135ed`. Benchmark executables: control `5e04ed2f880f2e7d3b1b042bc6cd401b0b74865d398d52f74f4950d5bc3bea93`, candidate `7bcd1cfe36486ab3cd318b6804e87e8042bbad610da5d55537bdbaa0c782fd93` (rebuild identical).

Build `quadtree-bench` with `-DQUADTREE_BUILD_BENCHMARKS=ON`. Run `TYPE=int32 taskset -c 6 bash screen.sh EXE OUT per-type` on control and candidate; repeat for `int64`, `float`, `double`. Use `target` with `TYPE` for the separate repeat. Without `TYPE`, `full` and `target` reproduce the combined-type screens. Gate evaluator exits 3 on failure, 4 on inconclusive. [Control](control.csv.gz), [candidate](candidate.csv.gz), [combined target repeat](target.csv.gz).
