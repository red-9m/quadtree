# `float`/`double` common-filter pair kernels (Task 09)

**Every `float` and `double` snapshot hard gate passes in the candidate's 5-round matrix: G1 16, G2 20, G3 9, G4 8, G6 62 and G9 15 for each type.** That includes the four rows left inconclusive at `c13e498`. Each also passes in two separate 45-round processes. On the control in the same run, those four rows are inconclusive in 7 of their 12 results (matrix plus two 45-round processes each). The other 5 pass: `float` G2 `uniform` 10³ in the matrix and one 45-round process, and `double` G2 `uniform` 10⁵ in both 45-round processes. `int32` and `int64` pass every snapshot hard gate on both binaries. Work counters, pair counts and checksums match the control on every uncapped library row, and warm allocations stay zero. This is candidate evidence; acceptance needs the full four-type release validation of the committed source.

## Task 09 rows

The control is `c13e498` (the release binary, `f7d8968a…`); the candidate adds [runtime.patch](runtime.patch). Limits: G2 4× the best baseline at 10²–10³ and 2× at 10⁴–10⁵; G9 4× the unordered step. Each cell is a value [95% interval] from one process; nothing is pooled.

| Row | Control: matrix / 45-round ×2 | Candidate: matrix / 45-round ×2 |
|---|---|---|
| `float` G2 `uniform` 10³ | 3.880 [3.764–4.000] / 3.905 [3.794–4.019], 3.776 [3.661–3.895] | 3.362 [3.258–3.470] / 3.373 [3.275–3.474], 3.367 [3.268–3.469] |
| `float` G9 `split-line-translated` 10⁵ | 3.968 [3.847–4.092] / 3.987 [3.874–4.103], 4.020 [3.905–4.138] | 3.743 [3.527–3.972] / 3.728 [3.623–3.836], 3.725 [3.619–3.833] |
| `double` G2 `uniform` 10³ | 3.853 [3.654–4.064] / 4.052 [3.936–4.171], 4.049 [3.934–4.167] | 3.454 [3.295–3.619] / 3.501 [3.397–3.609], 3.425 [3.324–3.528] |
| `double` G2 `uniform` 10⁵ | 1.980 [1.920–2.043] / 1.943 [1.888–1.999], 1.939 [1.884–1.995] | 1.822 [1.765–1.881] / 1.848 [1.796–1.902], 1.846 [1.793–1.900] |

Other rows inconclusive on the control in this run all pass on the candidate:
- `float`: G2 `split-line-translated` 10² (3.847 → 3.388); G9 `mixed-size` 10⁵ (3.926 → 2.866).
- `double`: G2 `split-line-translated` 10⁵ (1.966 → 1.792); G9 `mixed-size` 10⁵ (3.884 → 2.868) and `split-line-translated` 10⁵ (3.906 → 3.682).

Tightest candidate rows:
- G9 `split-line`/`split-line-translated` 10⁵: upper bounds 3.72–3.97 for `float` and 3.72–3.80 for `double`.
- G2 `double` `uniform` 10⁵: upper bound 1.90.

## Snapshot matrices

Default 5-round matrices ([summary](data/summary.txt)), equal-length output paths, binaries alternating first:

| Type | Control hard gates | Candidate hard gates |
|---|---|---|
| `float` | G2 19 + 1 inconclusive, G9 13 + 2 inconclusive, rest pass | **all pass** (G1 16, G2 20, G3 9, G4 8, G6 62, G9 15) |
| `double` | G2 17 + 3 inconclusive, G9 13 + 2 inconclusive, rest pass | **all pass** |
| `int32` | all pass | all pass |
| `int64` | all pass | all pass |

Estimate change, candidate vs control, excluding `pile`:

| Type | `index` 10²–10³ | `index` 10⁴–10⁵ | `index-ordered` 10²–10³ | `index-ordered` 10⁴–10⁵ |
|---|---|---|---|---|
| `float` | −40% to −0.5% | −21% to −1.4% | −44% to −5% | −34% to −7% |
| `double` | −46% to 0% | −23% to −0.4% | −42% to −5% | −31% to −6% |
| `int32` | −1.3% to +1.2% | −1.0% to +2.0% | −5.3% to +4.4% | −0.8% to +3.5% |
| `int64` | +0.9% to +6.7% | −7.8% to +3.0% | −5.2% to +3.9% | −3.4% to +2.1% |

- **`pile` (published under G5, no gate):** `float`/`double` unordered pairs are slower: +27–28% at 10² and +19–21% at 10³. Ordered pairs are faster: −33% to −38% at 10², −78% to −80% at 10³.
  - Cause: every candidate overlaps, so `commonRow` makes an out-of-line `reportPair` call per pair, as `int64` already did.
  - [Variants](variants/) that inline that report cut `pile` by 25–34%. They slowed regular rows by 2–5%, including G2 rows near their limits. The always-inline variant also made `float mixed-size` 10⁵ unstable (p95 59.7 vs 35.8 ms). Both variants are rejected.
- **`int64` unordered (`index`):** +1% to +7% at 10²–10⁴. No `int64` runtime path changed. GCC's inlining decisions in the four-type benchmark translation unit shifted. All `int64` gates still pass.
- **Persistent slice** ([data](data/)): `split-line`/`mixed-size`, n=10⁴, `static`/`small-1%`/`teleport-1%`, full and moved, 64 steps, all four types.
  - Hard gates pass on both binaries for every type.
  - Soft G8c rows (vs grid) fail or are inconclusive alike on both binaries, as in earlier release runs.
  - Library work matches the control. Persistent indexes are built by insertion, so they never set the common-filter proof.

## Change

[runtime.patch](runtime.patch) changes only `detail/quadtree-index-impl.h`.

- **Proof at bulk build:** bulk snapshot builds of `float`/`double` record the common-filter-bit proof that `int64` already records. It costs one AND per captured entry. Flat builds at or below `smallSetThreshold` are unchanged. `insert` and `setFilter` clear the proof; a later bulk build recomputes it.
- **Unordered full pairs** (`forEachPair`, default predicate) use the existing `CommonBitPairs` kernel when the proof holds:
  - no per-candidate filter check;
  - contiguous rows go through `commonRow`;
  - the exact packed SSE2 overlap test (`kernelOverlap`; `int64` keeps its narrow-span test).
- **Ordered full pairs** use `orderedCommonFloat`, the floating-point counterpart of the compact ordered walk `int64` and large `int32` queries use:
  - Threaded DFS nodes in reserved build-box scratch, and 24-byte rank-ordinal records in reserved build-key scratch.
  - `float` records hold exact lanes (minX, minY, −maxX, −maxY). Strict or closed contact follows the existing per-entry threshold.
  - `double` records hold the same lanes rounded toward −∞ to `float`, so the lane test can only admit extra candidates. This holds in any rounding mode: each converted lane is checked and stepped down one float if it landed above its value. Each lane hit is rechecked exactly with `packedCrossItemOverlap`.
    - **Defect (found after acceptance):** `_mm_cvtpd_ps` raises `FE_OVERFLOW`/`FE_INEXACT` on valid finite input, so with trapping enabled an ordered `double` query faults with SIGFPE. The [fix](../double-ordered-fp-flags-2026-11-28/README.md) replaces the conversion with exception-free integer order keys.
  - `double` thread nodes keep exact bounds, so node visits and charges equal the general walk's.
  - Rank order, candidate and node charges, unfinished-row discard, Stop, and fallback all match `orderedCommon`.
- **Fallback:** moved-set queries, custom predicates, no common bit, missing build scratch, non-contiguous nodes, budgets that cannot cover a row, and non-SSE2 builds keep the existing paths.
- **`orderedCommon` is `noinline`.** Inlined, its body shifted register allocation in the general ordered path of `traversePairs`. With the new floating-point code in the translation unit, that made `int32` ordered pairs at n=100 5–11% slower in both [first-candidate](first-candidate/) runs (pairs phase +0.5–0.7 µs/query). With `noinline`, every `int32` `Index` function in the four-type benchmark object is instruction-identical to `c13e498` plus that attribute (call relocations aside), and the n=100 rows match the control ([variant run](variants/orderedcommon-noinline/), matrix above). It runs once per query.

## Correctness

- **[CTest](ctest.log): 22/22.**
  - `pairPrefixes<T>` (formerly `int64PairPrefixes`) now also covers `float` and `double`. It compares the fast path with the predicate path at every candidate budget up to 256, with node budgets and Stop, before and after filter mutations.
  - New `floatCommonPairs<T>` covers exact contact, one-ulp gaps, signed zeros, subnormals and degenerate boxes for both boundaries. Its extreme set adds scale ladders beyond the `float` range for `double`, plus ±max. Each case checks:
    - a brute-force oracle under all four rounding modes;
    - fast vs general path with counters, budgets and Stop;
    - scratch-less queries;
    - a non-contiguous node;
    - a lost common bit.
  - Mutation checks failed the test as intended: dropping the `double` recheck, always closed or always strict `float` contact, `>=` node tests for `float` and `double`, and not clearing the proof on `setFilter`. Removing the step-down only fails under the directed rounding modes, which is why they are tested.
- **[Clang](verification.log):** `index-test` default, scalar (`-U__SSE2__ -fno-exceptions`) and ASan+UBSan, 0 failures each.
- **[Differential fuzz](fuzz.log)**, 4 types × 5 scenes, seeds 1–40, 1500 steps: all ok. The fuzzer now shares filter bit 8 on every seventh seed, so snapshot builds take the common-filter kernels. Some filter changes drop the bit until the next build restores it. Other seeds draw the same random sequence as before.

## Runs and data

- **[`data/`](data/):** the final candidate, [reproduce.sh](reproduce.sh) on 2026-11-28 03:19–03:57.
  - Snapshot matrices for all four types, two 45-round target processes per binary, and the persistent slice.
  - Quiet starts (Tctl ≤ 40 °C), CPU 6, HPET.
  - Every process exits 0 and every gate evaluation exits 0 or 4.
- **[`aborted/ev3`](aborted/ev3/):** an earlier start of the same run, cut off by a host shutdown during `matrix-float-cand`. Not evidence.
- **[`first-candidate/`](first-candidate/) and [`pilot/`](pilot/):** the same change before `noinline` (`.text` `fa7022ec…`); raw samples omitted.
  - `first-candidate/`: the full run, TSC clock source. Same `float`/`double` verdicts. It shows the `int32` n=100 ordered slowdown and one `int32` G1 `clustered n=4` inconclusive: upper 2.020, with index time equal to the control (42.4 vs 42.3 ns). Brute force ran faster in that process.
  - `pilot/`: an earlier matrix-only run with unequal output path lengths (`control`/`candidate`), stopped after its matrices. Same verdicts.
- **[`variants/`](variants/):** dev A/B runs of the rejected `pile` variants and of `noinline`, 2 × 5 rounds, not gate evidence.

## Reproduce

Ryzen 9 5900HS, CPU 6 with sibling 7 idle, EPP and platform profile `performance`, quiet starts. GCC 16.2.1, CMake 4.4.2, Release, `QUADTREE_BENCH_NATIVE=OFF`. [Environment and hashes](environment.txt).

1. Copy the two executables to equal-length paths.
2. Run `bash reproduce.sh BASE_EXE CAND_EXE OUT [matrix|targets|persist]`, then `python3 summarize.py OUT`.
3. For correctness: `bash verify.sh SRC` (Clang), `ctest` and `quadtree-index-fuzz` from the candidate build.
