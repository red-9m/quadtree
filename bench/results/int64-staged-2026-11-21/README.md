# 09 · Staged `int64` paths (`df2cfeb`): recheck

**Staged and verified.** Commit `df2cfeb` applies the validated [`int64` patch](../int64-bulk-plan-2026-11-21/README.md).
- **CI:** hosted [Portability #5](https://github.com/red-9m/quadtree/actions/runs/44939019888) passes 9/9.
- **`int32`:** the release status holds on the new commit. Every hard gate passes, including the accepted exceptions.
- **`int64`:** G2 is down to 1 fail (`uniform` 10⁵, 2.14×) and 5 unresolved rows; G9, G3, G4, G6 and the applicable persistent gates pass.

## Build

`git archive` of `df2cfeb` ([source-head.txt](source-head.txt) records the later HEAD, whose code is identical). Release CMake build with pinned Box2D, CTest 21/21. Benchmark `.text` `b065ce3d…` ([text.sha256](text.sha256)) equals the validated candidate's. Same machine and method as the [release run](../release-acceptance-2026-11-21/README.md); power settings restored.

## Runs ([data/](data/), [run.sh](run.sh))

| Stage | Exit (run, gates) | Hard gates |
|---|---|---|
| `int32-snapshot` (5 rounds) | 0, 3 | G1 8 pass + 8 in exception; G2 17 pass, 3 inconclusive; G3/G4/G6/G9 pass |
| `int32-persistent` | 0, 0 | G6 216, G7 120, G8a 12, G8b 4, G8c 24: all pass |
| `int64-snapshot` (5 rounds) | 0, 3 | G1 8 pass, 8 fail (0–4 entries); G2 16 / 1 / 3; G3/G4/G6 pass; G9 15 pass |
| `int64-persistent` | 0, 0 | G6 216, G8a 12, G8b 4 pass. No Box2D reference, so no hard G7/G8c. |
| `int32-resolve`, `int32-resolve-2`, `int64-resolve` | 0 | 45 rounds each, resolved by the [pre-specified rule](pooling-rule.md) |

Row counts match the release runs: 312 snapshot rows per type, 1,632 persistent rows for `int32` and 1,200 for `int64`.

### `int32` on `df2cfeb`

- `split-line-translated` 10²: passes in both 45-round processes (3.879 [3.769–3.991] and 3.851 [3.743–3.963]).
- `uniform` 10³: inconclusive, 3.900 [3.791–4.012] and 3.951 [3.836–4.069]; within the accepted ≤ 4.5× exception.
- `split-line-translated` 10⁵: 1.951 [1.889–2.015] and 1.929 [1.875–1.986]; the [pooled](data/pooled.txt) result is **1.940 [1.898–1.982], pass**.
- G1 0–4-entry rows are within the exception (upper ≤ 3.19). Everything else passes.
- Against the release `int32` confirmation, `--diff` reports snapshot 278/298 rows `same` (geometric mean 1.003) and persistent 1,548/1,577 `same` (1.001).

**The `int32` release acceptance remains valid on `df2cfeb`.**

### `int64` on `df2cfeb` (45-round `int64-resolve`)

| Row | Ratio [95%] | Limit | Status |
|---|---|---:|---|
| `uniform` 10⁵ | 2.141 [2.079–2.205] | 2 | **FAIL** |
| `split-line-translated` 10⁵ | 1.962 [1.906–2.020] | 2 | inconclusive |
| `split-line` 10⁴ | 1.958 [1.903–2.015] | 2 | inconclusive |
| `split-line` 10³ | 3.897 [3.787–4.010] | 4 | inconclusive |
| `uniform` 10³ | 4.053 [3.938–4.171] | 4 | inconclusive |
| `split-line-translated` 10² | 4.084 [3.966–4.205] | 4 | inconclusive |

All other `int64` G2 rows pass. Before staging, the release run had `int64` at 5 pass / 11 fail / 4 inconclusive. Soft persistent `int64` vs grid: G7 105 pass / 12 fail / 3 inconclusive; G8c 15 / 2 / 7.

## Next

`int64` performance acceptance still needs `uniform` 10⁵ about 7% faster. The gap to `int32` follows the 32-byte box (see the [candidate handoff](../int64-bulk-plan-2026-11-21/README.md#handoff)). It also needs a decision on the five rows near their limits, three of which have `int32` exception counterparts or equivalents.
