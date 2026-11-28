# Ordered `double` pair queries raised floating-point exceptions (`ce92ef5`)

**Defect:** in `ce92ef5`, a valid ordered full-pair query on a `double` index with a common filter bit and reserved build scratch converted every stored endpoint to `float` (`_mm_cvtpd_ps` in `orderedCommonFloat`'s lane setup). Finite inputs outside the `float` range raise `FE_OVERFLOW`; inputs that `float` cannot represent raise `FE_INEXACT`. With those exceptions trapping, the query dies with SIGFPE. Results and counters were correct. `c13e498` and every other query path compare stored values only and raise nothing.

**Fix:** the `double` lanes are now integer order keys built with bit operations only. Each key is the high word of the sign-magnitude-to-two's-complement image of the endpoint (`orderedLanes`). The keys are monotone, so the closed lane test is still a conservative prefilter, and every lane hit is still rechecked exactly with `packedCrossItemOverlap`. `float` lanes keep their exact sign-flip encoding, now built as an integer XOR. Thread nodes, work charges, rank order, Stop and fallbacks are unchanged ([runtime.patch](runtime.patch)).

## Reproduction

[fp-trap-probe.cpp](fp-trap-probe.cpp) builds two coincident ranked points at 1e100 with reserved build scratch, then runs one ordered `forEachPair`. It runs once to check the exception flags and once with `feenableexcept(FE_OVERFLOW | FE_INEXACT | FE_INVALID)` ([probe.log](probe.log)):

| Header | Flags | Trapping |
|---|---|---|
| `c13e498` | 1 pair, none | 1 pair |
| `ce92ef5` | 1 pair, `FE_OVERFLOW FE_INEXACT` | SIGFPE before the callback |
| fix | 1 pair, none | 1 pair |

## Tests

- **New `pairsRaiseNoFpFlags<T>`** (`float`, `double`) covers both boundaries and the fast and general paths (with and without a common filter bit), unordered and ordered.
  - Inputs: two coincident points at ±max, ±denorm_min, 1/3, min/3, ±1e100, 1e-100, 1e39, and just above `float` max (or ±1e30 and 1e-30 for `float`), plus disjoint companions.
  - Checks: exactly one reported pair, and `fetestexcept(FE_ALL_EXCEPT) == 0` after each query.
  - It fails on `ce92ef5` (22 failures) and passes on the fix.
- **Existing `floatCommonPairs<T>`** still covers one-ulp gaps, contact, signed zeros, subnormals, extremes and all rounding modes. One-ulp gaps now merge in the order keys and are rejected by the exact recheck.
- **[Mutation checks](mutations.txt)** each break the test:
  - no exact recheck: 76 failures;
  - no magnitude flip for negatives: 4,059 failures;
  - max lanes not inverted: 4,115 failures.
- **[CTest](ctest.log)** 22/22. **[Clang](verification.log)** default, scalar (`-U__SSE2__ -fno-exceptions`) and ASan+UBSan: 0 failures each. **[Full differential fuzz](fuzz.log)** 20/20.
- **`int32` code generation:** every `int32` `Index` function in the four-type benchmark object is still instruction-identical to `c13e498` with `orderedCommon` marked `noinline`.

## Performance

`ce92ef5` (base) vs the fix (cand), run with [reproduce.sh](../float-common-pairs-2026-11-27/reproduce.sh) stages `matrix targets`. Equal-length paths, HPET, CPU 6, quiet starts ([data](data/), [summary](data/summary.txt), [environment](environment.txt); raw samples omitted).

- **Hard gates:** every hard snapshot gate passes on both binaries for `float`, `double` and `int64`, and on the fix for `int32`. The base `int32` matrix has one inconclusive, G1 `clustered n=4` 1.950 [1.849–2.058]; the fix passes it at 1.874 [1.811–1.940]. All 45-round target rows pass on both binaries.
- **Work:** work counters, pair counts and checksums match on every uncapped library row. Warm allocations stay zero.
- **`double` ordered (`index-ordered`) estimates:** geometric mean −2.1% (range −17% to +10%, mostly build-phase noise).
  - Pairs phase: geometric mean −0.6%, range −4.2% to +5.5% (`split-line` 10⁵).
  - The 45-round target rows match within their intervals: G9 `split-line-translated` 10⁵ is 3.669/3.680 on base vs 3.670/3.685 on the fix, and G2 `uniform` 10³ is 3.550/3.544 vs 3.495/3.527.
- **`float`, `int32`, `int64`:** geometric means within ±1.1%.
  - Single rows vary by up to about ±5%, and up to 18% on `int32 pile` 10³ ordered. That `int32` code is instruction-identical on both binaries, so this is process placement.
