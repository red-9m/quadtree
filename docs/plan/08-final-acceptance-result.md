# 08 · Release acceptance result

**`int32` release accepted (2026-11-21)** on release candidate `a72bc80`: runtime/benchmark code built from `64675e6`, identical code, benchmark `.text` `97d8835e…`. Scope per [the brief](08-final-acceptance.md): performance sign-off covers `int32` only. `int64`, `float` and `double` are functionally supported and remain under [Task 09](09-other-coordinate-types.md).

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

## Current candidate: validation pending

`c13e498` includes the [four-entry G1 change](../../bench/results/g1-four-entry-flat-2026-11-26/README.md) and the [ordered `int32` G9 change](../../bench/results/g9-int32-ordered-common-2026-11-26/README.md). G9 passes 15/15 in the candidate snapshot matrix; `split-line-translated` 10⁵ passes there and in three independent 45-round runs. This does not extend the historical sign-off: the full release rerun and hosted portability checks are pending. See [SESSION-HANDOFF.md](../../SESSION-HANDOFF.md) for reproducible build hashes and next steps.

## Evidence

- [Four-type release run](../../bench/results/release-acceptance-2026-11-21/README.md): the baseline for Task 09.
- [`int32` confirmation](../../bench/results/release-int32-2026-11-21/README.md): the sign-off run.
- [Rejected `int32` G2 fix attempt](../../bench/results/int32-g2-planbulk-2026-11-21/README.md): the evidence behind the G2 exception.
