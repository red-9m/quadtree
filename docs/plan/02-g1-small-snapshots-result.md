# 02 · Small-snapshot G1: accepted exception

**Decision (owner, 2026-11-21):** accept the G1 failures for snapshot batches of 0, 1, 2 and 4 entries as a documented release exception. Task 02 is closed as an exception, not as a pass. The frozen G1 limit (≤ 2× brute force) is unchanged and these rows still report `FAIL`. [Task 08](08-final-acceptance.md) publishes them and does not block release on them, within the bounds below.

## Scope

- **Covered:** G1 rows (snapshot, `uniform`/`clustered`, batched) with **0–4 entries**, any coordinate type, `index` engine. A row is accepted only if its ratio interval's upper bound is **≤ 3.5×** brute; above that it is an ordinary blocking failure. The staged int32 worst case is 3.33×.
- **Not covered:** every other G1 row (8–64 entries) and every other gate. G0 correctness, G6 allocation and memory checks still apply to these rows without exception.

## Evidence

Staged packed cross-item control, frozen G1 slice, 81 and 117 matched rounds on CPU 6 ([raw data and verdicts](../../bench/results/g1-staged-small-2026-11-21/README.md#3-verify-the-full-g1-slice)). Each cell gives the absolute `index` time and the ratio interval, as 81-round / 117-round.

| Entries | `clustered` ns/scene | `clustered` ratio | `uniform` ns/scene | `uniform` ratio |
|---:|---:|---|---:|---|
| 0 | 10.5 / 10.6 | 2.03–2.18 / 2.05–2.20 | 10.5 / 10.6 | 2.03–2.18 / 2.05–2.20 |
| 1 | 19.2 / 19.2 | 2.84–2.99 / 2.84–2.99 | 19.3 / 19.2 | 2.85–3.00 / 2.79–2.94 |
| 2 | 39.0 / 33.8 | 3.12–3.33 / 2.59–2.83 | 35.2 / 35.2 | 2.86–3.12 / 2.61–2.99 |
| 4 | 57.6 / 58.5 | 2.65–2.86 / 2.65–2.93 | 61.7 / 61.0 | 2.73–2.97 / 2.74–2.91 |

All rows have exact pair results and zero warm allocations. Rows of 8–64 entries pass.

## Why accept rather than fix

The remaining gap is fixed per-rebuild overhead that the contract requires: validation, extent, a fresh owner identity and handle invalidation, plus the frozen adapter and runner. Brute force over ≤4 boxes has almost none of it.

- **Best valid candidate:** a direct pair result plus an earlier flat-build dispatch saves 4.4–5.0 ns at two entries, against roughly 10 ns needed, and regresses ordered rows. [Combined candidate](../../bench/results/g1-two-combined-2026-11-21/README.md) · [layout control](../../bench/results/g1-layout-control-2026-11-21/README.md).
- **Skipping required work is not enough either:** an invalid warm copy that skipped validation, extent and identity still ended at 2.02–2.07×. [Warm-copy headroom](../../bench/results/g1-two-copy-headroom-2026-11-21/README.md).
- **Other variants:** the extent, identity, store-fusion, slot-write, dispatch and result-copy variants each recovered ≤ about 5 ns; see the [task index](../../QUADTREE-UPDATE-PLAN.md) links.

Rejected alternatives: relaxing the frozen limit, which would mean changing the gate after measuring; and weakening the handle/identity contract or adding a tiny-snapshot API, which would trade safety for a few ns and still probably not reach 2×.

## Not shipped

None of the candidates are staged. The direct pair result (−3.3 to −4.7 ns at two entries under layout control) is valid. It remains an optional later improvement, which would need its own G1/G2/G9 regression screen.
