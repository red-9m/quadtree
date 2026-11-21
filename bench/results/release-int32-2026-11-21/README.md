# 08 · `int32` release confirmation (2026-11-21)

**`int32` passes release acceptance.** Every applicable hard gate passes. The exceptions are the owner-accepted G1 rows (0–4 entries, ≤ 3.5×) and two G2 rows (≤ 4.5×). One G2 row needed the pre-specified multi-process pooling. Scope per the [08 brief](../../../docs/plan/08-final-acceptance.md): performance sign-off covers `int32` only; [Task 09](../../../docs/plan/09-other-coordinate-types.md) owns `int64`/`float`/`double`.

## Source and setup

- **Binary:** the same build as the [four-type release run](../release-acceptance-2026-11-21/README.md), from `git archive` of `64675e6` ([source-head.txt](source-head.txt)). Its `.text` is `97d8835e…` ([text.sha256](text.sha256), checked before every stage). The runtime, benchmark and test code is identical to `a72bc80`, which passed hosted [Portability #2](https://github.com/red-9m/quadtree/actions/runs/44913550921) 9/9.
- **Machine and method:** the same machine, power settings, pinning and quiet starts as the release run; [environment](data/environment.txt) · [run order](data/run-order.csv) · [thermal](data/thermal.csv). Power settings restored.

[run.sh](run.sh) stages:

| Stage | Arguments | Time |
|---|---|---:|
| `snapshot` | `int32`, all 12 scenes, 0–64 and 10²–10⁵, `index,index-ordered,brute,sweep,grid`, 5 rounds | 2.2 min |
| `persistent` | `int32`, 4 scenes, 10³–10⁵, 10 workloads, `full`/`moved`, 64 steps, all 8 persistent engines | 33 min |
| `resolve`, `resolve-2` | 45-round reruns, each its own process, of `split-line-translated` and `uniform` at 10²/10³/10⁵ | 0.7 min each |

**Reconciliation:** 1,952 of 1,952 expected engine rows are present ([reconciliation.csv](data/reconciliation.csv)). Run exit codes are all 0: no cross-check failure and no `NONDETERMINISTIC`.

## Gate results

[gate-rows.csv](data/gate-rows.csv) holds the final per-row status, with each 5-round inconclusive row overridden by its `resolve` verdict. [summary.txt](data/summary.txt) has the counts.

| Gate | Rows | Result |
|---|---:|---|
| G1 | 16 | 8 pass; 8 accepted exception (0/1/2/4 entries, upper bounds 2.19–3.03 ≤ 3.5) |
| G2 | 20 | 17 pass; 2 accepted exception; 1 pass by pooling (below) |
| G3 / G4 | 9 / 8 | pass |
| G6 | 62 snapshot, 216 persistent | pass: zero warm allocations |
| G9 | 15 | pass |
| G7 | 120 | pass |
| G8a / G8b / G8c | 12 / 4 / 24 | pass |

**G2 exception rows** (cap 4.5×):

| Row | 5-round | `resolve` | `resolve-2` |
|---|---|---|---|
| `split-line-translated` 10² vs `sweep` | 4.206 [4.074–4.342] | 4.167 [4.050–4.287] | 4.115 [3.999–4.235] |
| `uniform` 10³ vs `grid` | 3.932 [3.821–4.046] | 3.962 [3.837–4.090] | 3.935 [3.819–4.056] |

**Pooled row:** `split-line-translated` 10⁵ vs `grid` was inconclusive in single processes because of the 2% process term. The three 45-round processes gave:
- 1.936 [1.881–1.992] (release run);
- 1.954 [1.897–2.012];
- 1.946 [1.890–2.003].

Under the [pooling rule](pooling-rule.md), committed before `resolve-2` ran, the pooled result is **1.945 [1.913–1.978] → pass** ([pooled.txt](data/pooled.txt)). The release-run process listed its two scenes in the opposite order; engines, sizes, rounds and seed were identical. The [Task 03 confirmation](../g2-packed-cross-item-confirm-2026-11-21/README.md) measured the same row at 1.925 [1.860–1.991].

**Published, not gated:**
- Soft persistent gates: G7 107 pass / 9 fail / 4 inconclusive; G8c 16 / 2 / 6.
- G5 pathology: [g5-pathology.csv](data/g5-pathology.csv).
- Library timing, work, allocations and memory: [library-rows.csv](data/library-rows.csv). The largest cold peak is 43.3 MB. 82 persistent library rows are time-capped (a prefix of 64 steps).
- Raw samples: `data/*/raw.csv.gz`.

## Reproduction

Use the release-run build (`WORK/build`, `WORK/box2d`). Run from this directory:

1. `bash run.sh WORK data`
2. `QT_RESOLVE_ARGS="--scenes=split-line-translated,uniform --sizes=100,1000,100000 --small-sizes=" bash run.sh WORK data resolve resolve-2`
3. `python3 summarize.py data`. The pooled check is the inline script that produced [pooled.txt](data/pooled.txt), applying [pooling-rule.md](pooling-rule.md).
