# 09 · `float`/`double` snapshots: bulk planner, flat build and packed overlap

**Valid candidate, not yet staged.** [float-paths.patch](float-paths.patch) against `b23a3ba`/`96f8d4c` makes `float`/`double` snapshot steps 24–63% faster for n ≥ 10² and up to 72% faster for small sets. Work counts are unchanged and `int32`/`int64` timing is neutral.

In the matched 5-round run:
- **G1 `float`/`double`:** every 8–64-entry row passes. Every 1–4-entry row sits within the accepted ≤ 3.5× exception range, except one noisy `double` `uniform` n=1 interval.
- **G2:** from 2 pass / 18 fail to `float` 9 / 7 / 4 and `double` 10 / 8 / 2 (pass / fail / inconclusive).
- **G9:** 15/15 for both. G3, G4 and G6 pass.

Not resolved at 45 rounds yet.

## Diagnosis

From the [release run](../release-acceptance-2026-11-21/README.md), `float`/`double` builds were 2.4–4.7× slower than `int32`'s, while pairs were 1.03–1.6× slower. Every `int32` fast path was closed to floats:
- **Build:** the top-down `planBulk`, so floats used per-entry `makeKey` + key sort.
- **Commit:** the commit prefetch.
- **Ordered pairs:** the ordered radix and tree-walk paths.
- **Small sets:** the warm flat build.
- **Cross-item test:** the packed overlap test.

## Change (`detail/quadtree-index-impl.h`)

1. **`planBulk` for all coordinate types.** Floats compute the child fit with `Split::fits`' own span arithmetic (`fitSpan`) and the quadrant with `Split::quadrant`. When the extent lies within ±max/2, `planBulk<NearMid = true>` uses `(lo + hi) / 2`, which is `std::midpoint`'s overflow-free branch in libstdc++ and libc++. Integers keep exact unsigned halving.
2. **Commit prefetch and the ordered paths** (radix rank order, `walkOrdered`, radix row sort) **for every type.**
3. **Packed cross-item overlap for `float` (SSE2 `ps`) and `double` (SSE2 `pd`),** with the same separation-then-contact logic as `int32`. The ≤64-entry small-set pair loop uses it for floats.
4. **Warm flat build (`buildFlatInteger`) for floats.** A branch-free finite/ordering test has the same acceptance as `canonicalize`. Stored boxes are the canonical form, using `x + 0` to normalize −0 exactly.

Screens ([screens/](screens/), `compare.py` A/B; stage-B matched run in [screen-B-data/](screen-B-data/)):

| Stage | Result |
|---|---|
| planner only | −20% to −50% |
| + ordered paths / prefetch | further build −10–20% |
| per-entry inline midpoint guard (rejected) | build +20% |
| packed overlap | pairs −10–25% |
| extent-level `NearMid` | −9% to −22% |
| flat build + packed small loop | G1 2.6–5.4× → int-like |

## Verification ([checks/](checks/))

- **Equivalence probe** ([verify.sh](verify.sh)) for all four types: extremes and 5 scenes × 10³/10⁴/10⁵. It matches topology, handle→rect mapping, ranks and exact unordered and ordered pair sequences for full and moved queries against control (12 cases each for `float`/`double`). `int32`/`int64` dense-order streams are exact.
- **[Small-set probe](small-float-probe.cpp):** 3,000 `float`/`double` indexes × 4 builds of 0–70 entries, covering −0/+0 inputs, NaN, infinity, inverted boxes, both boundaries, prepared and unprepared builds, and Snapshot/Adaptive. Control and candidate transcripts are byte-identical (stored rect bit patterns, extent, old-handle validity, statuses, pairs, counters with and without budget), also under ASan/UBSan ([log](checks/small-float-probe.log)).
- **Budget contract** for `float`/`double`: ordered prefix and unordered subset, plain and ASan/UBSan ([log](checks/budget-contract.log)).
- **CTest:** 21/21. **Full fuzz:** passes. The CMake build's `.text` equals the screened binary ([text.sha256](text.sha256)).
- **Integer code generation changes:** `int32` `buildPrepared` and `planBulk` differ ([log](checks/int32-codegen.log)), for example because removing `planBulk`'s requires-clause changes GCC's inlining. The matched `int32`/`int64` timing below is the check.

## Matched run ([data/](data/), [run.sh](run.sh))

Each row is control → candidate, run in turn on CPU 6, 5 rounds. Every run exits 0; candidates, node visits and checksums are identical on all rows.

| Type | `--diff` geomean | G1 | G2 | G9 |
|---|---:|---|---|---|
| `float` | 0.810 (109 faster, 9 slower) | 6/8/2 → 8/6/2 | 2/18/0 → 9/7/4 | 15 → 15 |
| `double` | 0.808 (108 faster, 6 slower) | 6/8/2 → 8/4/4 | 2/18/0 → 10/8/2 | 15 → 15 |
| `int32` | 0.994 (22 faster, 3 slower) | 8/8/0 → 8/6/2 | 16/0/4 → 18/0/2 | 15 → 15 |
| `int64` | 1.004 (5 faster, 10 slower) | 8/6/2 → 8/7/1 | 14/1/5 → 15/1/4 | 15 → 15 |

The G1 counts are against the plain 2× limit; the 1–4-entry `float`/`double` rows are failures there but within the exception range.

Remaining `float`/`double` G2 rows, 5-round values:

| Scene | 10² | 10³ | 10⁴ | 10⁵ |
|---|---|---|---|---|
| `split-line` | 4.18 / 4.15 | 4.19 / 4.36 | 2.16 / 2.15 | 1.96 / 2.03 (unresolved) |
| `split-line-translated` | 4.40 / 4.56 | 4.24 / 4.35 | 2.15 / 2.13 | 2.05 / 2.10 |
| `uniform` | — | 4.44 / 4.58 | 1.95 (unresolved, `float`) | 1.96 / **2.17** |

Values are `float` / `double`; limits are 4× at 10²/10³ and 2× at 10⁴/10⁵. The gap is about 5–15%. The `double` G1 `uniform` n=1 interval (1.98 [0.77–5.08]) is noise and needs a rerun before any exception verdict.

## Handoff

Stage it as with `int64`: commit, hosted CI, and recheck `int32` acceptance plus the `int64`/`float`/`double` matrices with 45-round resolves. The remaining `float`/`double` G2 rows need about 5–15% more, mostly on the build side: `float` build at 10⁵ is about 6 ms against `int32`'s 5.4 ms, and pairs are about 1.05–1.1×.
