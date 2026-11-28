# 08 · Release acceptance result

**`int32` release accepted (2026-11-21)** on release candidate `a72bc80`: runtime/benchmark code built from `64675e6`, identical code, benchmark `.text` `97d8835e…`. Scope per [the brief](08-final-acceptance.md): performance sign-off covered `int32` only, with `int64`, `float` and `double` under [Task 09](09-other-coordinate-types.md).

**Current status (2026-11-28): all four coordinate types are performance-accepted on `db0c820`** ([validation](../../bench/results/release-db0c820-2026-11-28/README.md), [recheck below](#recheck-on-db0c820)).
- **`int64`, `float`, `double`:** every hard gate passes under the pre-specified rule.
- **`int32`:** one persistent G8b row stayed open under the rule; the owner accepted `int32` on `db0c820` on the recorded evidence.
- **`ce92ef5`:** passed every gate for all four types, but its ordered `double` path [raised floating-point exceptions](../../bench/results/double-ordered-fp-flags-2026-11-28/README.md). `db0c820` fixes that.

## Exit criteria

- **Hard gates (`int32`):** all pass on the [`int32` confirmation](../../bench/results/release-int32-2026-11-21/README.md) (snapshot and persistent, 1,952/1,952 rows). Owner-accepted exceptions apply:
  - G1, 0–4 entries, upper bound ≤ 3.5×: [02 result](02-g1-small-snapshots-result.md).
  - G2 `split-line-translated` 10² and `uniform` 10³, upper bound ≤ 4.5×: [brief](08-final-acceptance.md).

  G2 `split-line-translated` 10⁵ resolves to pass under the [pre-specified three-process pooling](../../bench/results/release-int32-2026-11-21/pooling-rule.md). No hard interval is left unresolved outside these.
- **Correctness:** no cross-check failure and no `NONDETERMINISTIC` result in any stage of either run. Release CTest is 21/21 and the full differential fuzz passes ([promotion record](../../bench/results/promotion-2026-11-21/README.md)).
- **Allocations and memory:** zero warm allocations on every library row of all four types (G6), with cold peaks published.
- **Portability:** hosted [Portability #2](https://github.com/red-9m/quadtree/actions/runs/44913550921) passes all nine jobs on `a72bc80` ([06 result](06-portability-result.md)). MSVC is deferred.
- **Calibration:** two identical processes agree on 98.0% of rows, so the 2% snapshot term holds ([four-type run](../../bench/results/release-acceptance-2026-11-21/README.md#calibration)). The persistent 3% term was not re-validated.

## Recheck after `df2cfeb`

`df2cfeb` enables the integer bulk planner and ordered paths for `int64`; the `int32` snapshot code is unchanged apart from alignment. A [full `int32` recheck](../../bench/results/int64-staged-2026-11-21/README.md#int32-on-df2cfeb) on that commit passes every hard gate within the same exceptions; `split-line-translated` 10⁵ passes again under the pre-specified two-process pooling. Hosted Portability #5 passes 9/9. **The `int32` acceptance holds on `df2cfeb`.**

`d52dbe1` adds the `float`/`double` fast paths; it changes `int32` build code generation but not its results. The [recheck on `d52dbe1`](../../bench/results/float-staged-2026-11-21/README.md#results) passes every `int32` hard gate. G2 now passes on all 20 rows, so neither G2 exception row is needed in that run; G1 0–4 entries remain within the accepted exception. Hosted Portability #7 passes 9/9. **The `int32` acceptance holds on `d52dbe1`.**

`7cfbebe` adds packed pair tests to `selfPairs`, `itemsVsSubtree` and `orderedSelf`. The [recheck on `7cfbebe`](../../bench/results/packed-staged-2026-11-23/README.md#results) passes every `int32` hard gate:
- **Snapshot:** G2 20/20 in the 5-round matrix, so neither G2 exception row is needed. G3, G4, G6 and G9 15/15 pass. G1 8–64 entries pass.
- **G1 0–4 entries:** within the ≤ 3.5× exception (upper ≤ 3.11). n=0 pools to 2.08× over two 45-round processes.
- **Persistent:** G6 216, G7 120, G8a 12, G8b 4 and G8c 24 pass.
- **Correctness and CI:** every process exits 0, so cross-checks and determinism are clean. Hosted Portability #10 passes 9/9.

**The `int32` acceptance holds on `7cfbebe`.**

## Recheck on `c13e498`

`c13e498` adds the [four-entry G1 change](../../bench/results/g1-four-entry-flat-2026-11-26/README.md) and the [ordered `int32` G9 change](../../bench/results/g9-int32-ordered-common-2026-11-26/README.md). The [full release validation](../../bench/results/release-c13e498-2026-11-26/README.md) covered all four types under a [pre-specified rule](../../bench/results/release-c13e498-2026-11-26/resolution-rule.md).

- **Snapshot (`int32`):** G1 16/16, G2 20/20 and G9 15/15 pass, so neither the G1 0–4 nor the G2 exception is used. G3, G4 and G6 pass.
- **Persistent (`int32`):** G6 216, G7 120, G8a 12, G8b 4 and G8c 24 pass.
- **All four types:** G0 and G6 pass.
- **CTest and CI:** CTest 22/22. Hosted [Portability #21](https://github.com/red-9m/quadtree/actions/runs/45887625703) passes 9/9 on the exact SHA.
- **Calibration:** the first pair agreed on 94.3% (target ≥ 95%), so the rule withheld sign-off. By owner decision, the calibration pair and the `int32` snapshot matrix were [rerun under the same rule](../../bench/results/release-c13e498-2026-11-26/rerun-rule.md). The rerun reached 98.3%, and every `int32` snapshot hard gate passed in the 5-round matrix with no repeats.

**The `int32` acceptance holds on `c13e498`.**

## Recheck on `ce92ef5`

`ce92ef5` adds the [common-filter pair kernels for `float`/`double`](../../bench/results/float-common-pairs-2026-11-27/README.md). The `int32` runtime paths are unchanged; `orderedCommon` becomes `noinline`, so the `int32` code generation stays independent of the new floating-point code. The [four-type release validation](../../bench/results/release-ce92ef5-2026-11-28/README.md) ran under a [pre-specified rule](../../bench/results/release-ce92ef5-2026-11-28/resolution-rule.md).

- **Snapshot, all four types:** G1 16/16, G2 20/20, G3 9, G4 8, G6 62 and G9 15/15 pass in the 5-round matrices, with no repeats and no exception rows.
- **Persistent:** `int32` and `float` pass G6 216, G7 120, G8a 12, G8b 4 and G8c 24. `double` and `int64` pass G6 216, G8a 12 and G8b 4; they have no Box2D reference.
- **G0 and G6:** every process exits 0. There are zero warm allocations on all 2,224 library rows.
- **Calibration:** 97.7% (291/298).
- **CTest, fuzz and CI:** CTest 22/22 and full differential fuzz 20/20 on the archive build. Hosted [Portability #24](https://github.com/red-9m/quadtree/actions/runs/46095847492) passes 9/9 on the exact SHA.

**The `int32` acceptance holds on `ce92ef5`.** All four types passed on it. However, `ce92ef5`'s ordered `double` queries raise `FE_OVERFLOW`/`FE_INEXACT` on valid input, and fault with SIGFPE when trapping is enabled. So its `int64`/`float`/`double` acceptance was moved to the fixed commit, `db0c820`.

## Recheck on `db0c820`

`db0c820` replaces the `double` lane conversion with exception-free integer order keys ([fix evidence](../../bench/results/double-ordered-fp-flags-2026-11-28/README.md)). Every `int32` `Index` function is instruction-identical to `ce92ef5`. The [validation](../../bench/results/release-db0c820-2026-11-28/README.md) used the same [pre-specified rule](../../bench/results/release-db0c820-2026-11-28/resolution-rule.md).

- **Snapshot, all four types:** G1 16/16, G2 20/20, G3 9, G4 8, G6 62 and G9 15/15 pass in the 5-round matrices.
- **Persistent:**
  - `float`: G6 216, G7 120, G8a 12, G8b 4, G8c 24 pass.
  - `double`, `int64`: G6 216, G8a 12, G8b 4 pass.
  - `int32`: everything passes except G8b `mixed-size` n=10⁵ `small-single/moved`. Matrix 3.843 [2.903–5.086]; pooled over the rule's two 45-round repeats 3.901 [3.781–4.024], limit 4, so the row stays **open**.
  - A post-hoc diagnostic, not used by the rule, ran the repeats' arguments on the `c13e498` and `ce92ef5` binaries. They give the same 3.8–4.0 in that process shape (the full matrices gave 3.32–3.37 there).
- **Other checks:** G0 and G6 pass. Calibration 99.0%. CTest 22/22, full fuzz 20/20, hosted [Portability #27](https://github.com/red-9m/quadtree/actions/runs/46138155025) 9/9.

**`int64`, `float` and `double` are performance-accepted on `db0c820` (Task 09).** Under the rule, the `int32` G8b row stays open as measured.

**Owner decision (2026-11-28): `int32` is accepted on `db0c820` on this evidence.** Every other `int32` hard gate passes, and the `int32` code is instruction-identical to `ce92ef5`, which passed the row. The diagnostic shows the repeat-process value also on `c13e498`. **The `int32` acceptance holds on `db0c820`.**

## Evidence

- [Four-type release run](../../bench/results/release-acceptance-2026-11-21/README.md): the baseline for Task 09.
- [Four-type validation of `ce92ef5`](../../bench/results/release-ce92ef5-2026-11-28/README.md), superseded for `double` by the fix.
- [Four-type validation of `db0c820`](../../bench/results/release-db0c820-2026-11-28/README.md): the four-type acceptance run (`int32` by owner decision on one open persistent row).
- [`int32` confirmation](../../bench/results/release-int32-2026-11-21/README.md): the sign-off run.
- [Rejected `int32` G2 fix attempt](../../bench/results/int32-g2-planbulk-2026-11-21/README.md): the evidence behind the G2 exception.
