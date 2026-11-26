# G1: four-entry flat snapshots

**All eight n=4 rows pass in five matched full screens and the long-path recheck (48/48, worst upper bound 1.939× brute).** All eight fail at `95c9178` in each screen. Index time drops from about 51–63 to 36–44 ns/scene. This is a targeted improvement, not release acceptance; no gate limits or release exceptions changed.

## G1 results

81 rounds per full screen; all four coordinate types, both scenes, 0–64 entries. The five primary runs used a 24-character output directory three times and one each at 18 and 86 characters. Values below are ranges across those five runs, not pooled intervals. [Per-run estimates and 95% intervals](gates.csv), [snapshots, raw samples and gate output](snapshots.tar.gz).

| n=4 | Control ns/scene | Candidate ns/scene | Brute ns/scene (candidate runs) | Candidate ratio | Worst upper bound |
|---|---:|---:|---:|---:|---:|
| `int32 uniform` | 54.4–57.9 | 38.3–41.0 | 21.6–22.5 | 1.752–1.822 | 1.914 |
| `int32 clustered` | 51.0–54.5 | 36.9–39.5 | 20.9–22.5 | 1.732–1.790 | 1.879 |
| `int64 uniform` | 55.8–59.4 | 42.0–44.1 | 23.5–25.1 | 1.757–1.814 | 1.876 |
| `int64 clustered` | 52.6–55.6 | 41.1–43.0 | 22.8–23.9 | 1.765–1.864 | 1.939 |
| `float uniform` | 58.0–60.5 | 37.6–38.8 | 23.3–24.4 | 1.557–1.635 | 1.693 |
| `float clustered` | 53.4–55.6 | 36.7–37.8 | 22.2–23.0 | 1.635–1.703 | 1.768 |
| `double uniform` | 60.1–62.8 | 38.5–39.5 | 23.8–24.9 | 1.566–1.618 | 1.671 |
| `double clustered` | 57.1–61.9 | 37.0–40.4 | 23.4–24.1 | 1.561–1.705 | 1.831 |

- **Isolated `int64 uniform n=4` (×2, 81 rounds):** 55.9–56.8 → 41.0–41.2 ns. Candidate upper bounds 1.799 and 1.821; control lower bounds 2.288 and 2.310.
- **n=8–64:** 160/160 pass on both binaries across the five full screens.
- **n=0/1/2:** 24/24 pass on both binaries in each target screen (117 rounds) and across the four per-type runs (45 rounds). In the five full screens both binaries have 116 passes, 4 inconclusives, and no failures out of 120. The known `float clustered n=2` [heap placement issue](../g1-two-entry-inline-2026-11-26/README.md#limits-float-clustered-n2-in-the-full-screen) remains inconclusive in all three 24-character runs (candidate 1.983–2.009, upper 2.051–2.077; control upper 2.042–2.070). It passes at both other path lengths (candidate upper ≤ 1.855). The remaining inconclusives are control `int32 clustered n=1` in one 24-character run (upper 2.040) and candidate `double clustered n=1` in the 86-character run (upper 2.048).
- **86-character recheck:** candidate 64/64, including `double clustered n=1` (upper 1.722; control 1.752). All eight n=4 rows pass again, upper ≤ 1.879. Control passes the other 56/64, with only n=4 failures. The first singleton result remains inconclusive.
- **Matching work:** scene hashes, pair counts and result checksums match between control and candidate in all matched runs. Both index engines have zero warm allocations.

## Change and attribution

- Specialize the existing warm flat replacement at `n=4`; fixed trip counts let the compiler unroll validation and storage. It still validates every box before committing, delegates ranked input, and consumes one module identity on success.
- `forEachPair` dispatches unordered 3–4-entry flat queries directly to `tinyPairs` when the budget covers all candidates. It inlines views, predicates and visitors into that helper, not into the 0–2-entry caller. In the real benchmark, forcing this whole path into the caller instead made `int32 uniform n=0` climb from 22 to 52 cycles/scene. Explicitly keeping general traversal out of line also prevents it from inflating the short paths.
- The tiny loop reuses packed endpoint tests. For `int64`, the enclosing extent must prove both axes' gaps fit in `int32` before using the existing SSE2 narrow test. Wider extents and non-SSE2 builds use exact full-width comparisons. Filter checks, dense pair order, slot orientation, predicate order, counters and Stop are unchanged; insufficient budgets still use general traversal.
- In `quadtree-bench` (`int64 uniform n=4`, unordered), the control uses 239 cycles/scene. Fixed-count build and direct dispatch with a scalar loop use 217 cycles; the candidate with narrow SSE2 uses 175 cycles. Clustered: 226 → 206 → 169 cycles. The scalar variant is diagnostic only. [Repeat profiles](profile.csv) confirm the reductions for all four types (`perf stat`, user-mode cycles, instructions and branch misses over 25.6M warm scenes).

## Larger-row regression screen

- **G2, five rounds ×2:** both binaries pass 76/80 with four inconclusives each; all are split-line or translated split-line at n=100. The 45-round `int32` and `int64` follow-ups pass every G2 row. Candidate `int64 split-line-translated n=100000` has upper bounds 1.950 and 1.983 (limit 2); control 1.958 and 1.859.
- **G9, five rounds ×2:** candidate 58 pass / 2 inconclusive, control 57 pass / 3 inconclusive, no failures. Both 45-round `int64` follow-ups pass on both binaries. At 45 rounds the first `int32 split-line-translated n=100000` comparison fails for the candidate (4.174 [4.053–4.299]), with control inconclusive (4.007 [3.890–4.128]). In that run the ordered time rises 51.57→54.07 ms and unordered time 12.87→12.95 ms. The `int32` ordered walker, traversal and general build match after normalization for [addresses and padding](layout.txt); no ordered algorithm change was found. The non-translated row is inconclusive in that run on both binaries. Both rows pass on both binaries in the second 45-round run (translated upper 3.979 candidate, 3.992 control). Keep the first results separate: G9 margin is thin on this binary.

## Correctness

- [CTest](ctest.log): 22/22, including `flatIdentityExhaustion`, differential fuzzing and exception-disabled tests. Added four-entry prepared floating builds and four-entry `int64` overlap tests around the narrow span limit, at 2³², and at the signed endpoints on both axes.
- [Verification](verification.log): Clang `index-test` with default flags and with `-U__SSE2__ -fno-exceptions`. The [0–5-entry pair transcript](semantics-probe.cpp) compares full predicate and report sequences, all candidate budgets through the pair count and unlimited, Stop and counters, for all four coordinate types, with and without instrumentation. Control, candidate, ASan+UBSan and scalar fallback transcripts match byte for byte (70,564 lines).
- The [flat-build transcript](../g1-two-entry-inline-2026-11-26/flat-build-probe.cpp) matches byte for byte (160,000 builds, including invalid boxes, ranks, handle output and identity consumption), also under ASan+UBSan and the scalar fallback.

## Reproduce

Ryzen 9 5900HS, CPU 6 with sibling 7 idle; `powersave`, EPP `performance`, HPET. GCC 16.2.1, CMake 4.4.2, `-DCMAKE_BUILD_TYPE=Release -DQUADTREE_BUILD_BENCHMARKS=ON`, native tuning off. `bash reproduce.sh CONTROL_EXE CANDIDATE_EXE OUT SHORT_OUT LONG_OUT` uses the unchanged [screen](../g1-tiny-snapshots-2026-11-26/screen.sh), with no adapter, fixture, reference engine or process-error changes. Set the three roots to lengths 12, 6 and 74 respectively to reproduce the full-screen output directory lengths.

- Control `95c9178` executable SHA-256: `546d79c7035b2f5e1f0639c09b3ebb367f40a007bbc79d79ddd005ffc8fcd82b`.
- Candidate executable SHA-256: `2090617b229d379ff82cd4ae929aa026598e33c801a69de3fe8222203ba960e2`.
- Candidate `detail/quadtree-index-impl.h` SHA-256: `455132cf46b5a5fbcd05bab54d99fe501c0209690b7ee63cf6a0876a0a7e1e0e`.
