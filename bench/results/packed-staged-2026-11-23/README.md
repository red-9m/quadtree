# 09 · Staged packed pair tests (`7cfbebe`): recheck, INCOMPLETE

**Status: interrupted.** Commit `7cfbebe` stages the validated [packed-pairs candidate](../packed-pairs-2026-11-22/README.md). Hosted [Portability #10](https://github.com/red-9m/quadtree/actions/runs/45135921885) passes 9/9 on it. Benchmark `.text` `c01f6323…` equals the validated candidate's ([text.sha256](text.sha256)), and CTest is 21/21.

The recheck under the [pre-specified rule](resolution-rule.md) ran only the four 5-round snapshot matrices. The first persistent stage, `int32-persistent`, was killed with SIGTERM after 9 minutes (exit 143 in [run-order.csv](data/run-order.csv)). Its partial directory is kept as [`int32-persistent-aborted/`](data/int32-persistent-aborted/) and is not evidence. Power settings were restored. **No persistent matrix and no 45-round resolve has run, so the `int32` acceptance on `7cfbebe` is not yet re-established.**

## Completed: 5-round snapshot matrices ([data/](data/))

| Type | G1 | G2 | G9 | G3/G4/G6 |
|---|---|---|---|---|
| `int32` | 8 pass, 6 fail (1–4 entries), 2 inconclusive (n=0) | **20 / 0 / 0** | 15 / 0 / 0 | pass |
| `float` | 9 / 6 / 1 | 19 / 0 / 1 | 13 / 0 / 2 | pass |
| `double` | 8 / 6 / 2 | 13 / 0 / 7 | 13 / 0 / 2 | pass |
| `int64` | 8 / 7 / 1 | 14 / 2 / 4 | 15 / 0 / 0 | pass |

The 1–4-entry `int32` G1 fails are expected and covered by the accepted ≤ 3.5× exception. Every run exited 0, so there was no cross-check or determinism failure. Matrix-inconclusive rows, each needing two 45-round processes under the rule:

| Type | Inconclusive rows | `QT_RESOLVE_ARGS` |
|---|---|---|
| `int32` | G1 `clustered`/`uniform` n=0 | `--scenes=uniform,clustered --sizes= --small-sizes=0` |
| `float` | G1 `clustered` n=0; G2 `uniform` 10³; G9 `mixed-size`/`split-line-translated` 10⁵ | `--scenes=uniform,clustered,mixed-size,split-line-translated --sizes=1000,100000 --small-sizes=0` |
| `double` | G1 n=0 (both scenes); G2 `split-line` 10²/10³, `split-line-translated` 10²/10³/10⁵, `uniform` 10³/10⁵; G9 `mixed-size`/`split-line` 10⁵ | `--scenes=uniform,clustered,split-line,split-line-translated,mixed-size --sizes=100,1000,100000 --small-sizes=0` |
| `int64` | G1 `clustered` n=0; G2 `split-line` 10²/10³/10⁴, `uniform` 10³ | `--scenes=uniform,clustered,split-line --sizes=100,1000,10000 --small-sizes=0` |

Matrix failures, which keep their verdict under the rule: `int64` G2 `split-line-translated` 10² (4.29× [4.15–4.43]) and `uniform` 10⁵ (2.16×), and `int64` G1 `uniform` n=0 (2.28×, outside the 0–4 scope of any exception for `int64`).

## Resume

Work directory: the path in `~/.cache/quadtree-staged3.current`. It holds a `git archive` of `7cfbebe` in `src/`, the Release build in `build/`, and `box2d/`. Rebuild the same way if it is gone, and check `text.sha256`. The machine must be idle: the runner waits for quiet starts of ≤ 40 °C.

1. **Persistent stages (~2 h):** `bash run.sh WORK data int32-persistent float-persistent double-persistent int64-persistent`. `run.sh` refuses an existing stage directory, so keep the aborted directory under its new name.
2. **Resolves:** for each type, `QT_RESOLVE_ARGS="<args from the table>" bash run.sh WORK data <type>-resolve <type>-resolve-2`.
3. **Verdicts:** `python3 resolve.py data > data/final-status.txt`. Check `int32` against the 08 exceptions and every persistent hard gate.
4. **Report:** complete this README.
   - Hosted CI: record #10 in the [06 result](../../../docs/plan/06-portability-result.md).
   - Acceptance: re-establish `int32` on `7cfbebe` in the [08 result](../../../docs/plan/08-final-acceptance-result.md).
   - Gaps: update `INDEX.md` and the [09 closure record](../../../docs/plan/09-other-coordinate-types-result.md).
