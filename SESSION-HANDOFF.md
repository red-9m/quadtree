# Handoff: `c13e498` release validation done, owner decision pending

## Repo state

- Branch `quadtree-index` in `/home/dima/src/quadtree.git`. The tested runtime is `c13e498`; later commits change only results and docs. Local commits are not pushed to `origin/quadtree-index`, which is still at `95c9178`. For hosted CI, `c13e498` was pushed as `origin/validate/int32-ordered-common`. Leave untracked `build-review/` alone.
- Validation record: [`bench/results/release-c13e498-2026-11-26/`](bench/results/release-c13e498-2026-11-26/README.md). The [rule](bench/results/release-c13e498-2026-11-26/resolution-rule.md) was committed (`13629a3`) before any stage ran.

## Result

- **`int32`:** every hard gate passes. Snapshot: G1 16/16, G2 20/20, G3, G4, G6, G9 15/15. Persistent: G6 216, G7 120, G8a 12, G8b 4, G8c 24. Neither the G1 0–4 nor the G2 small-tree exception is used. G2 `split-line-translated`/`uniform` 10⁵ were inconclusive in the matrix and pass on the two 45-round repeats.
- **All four types:** every process exits 0 (G0), zero warm allocations (G6), all persistent hard gates pass, and all 6,944 expected engine rows are present. CTest passes 22/22, and hosted [Portability #21](https://github.com/red-9m/quadtree/actions/runs/45887625703) passes 9/9 on the exact SHA.
- **Non-`int32` timing (published only):** `float` G2 `uniform` 10³ and G9 `split-line-translated` 10⁵ stay inconclusive, as do `double` G2 `uniform` 10³ and 10⁵. No row fails; `int64` passes everything.
- **Blocker:** the calibration pair `calib-a`/`calib-b` agreed on 94.3% of rows, below the 95% target. Per the rule, `int32` timing sign-off is withheld. Most of the 17 differing rows come from the tiny-size placement effect (`clustered` n=0) and a host thermal event at the end of `calib-b` (`outliers` 10⁵, Tctl 83–90 °C). A supplementary pair run after the matrices, `calib-c`/`calib-d`, reached 97.7%.

## Next

1. **Owner decision:**
   - (a) accept `calib-c`/`calib-d` and extend the `int32` sign-off to `c13e498`; then update `docs/plan/08-final-acceptance-result.md` and the plan, and promote; or
   - (b) rerun a calibration pair plus the `int32` snapshot matrix under the same rule, on a quiet host.
2. If (b): reuse the workspace `~/.cache/quadtree-release-c13e498.OIHE` (`.current` file), which holds the verified binary. Run `bash run.sh WORK OUT calib-a calib-b int32-snapshot` into a new output directory, then the repeats. Keep other agent sessions and browsers idle.

## Watch items

- **Host interruptions:** the first `int32-persistent` stage was killed by a host shutdown. It is kept as `int32-persistent-aborted/`, and the persistent half was rerun after reboot (`QT_FROM=persistent`).
- **`float clustered n=2`:** passes in this matrix (1.496 [1.440–1.554]; output dir 88 characters). The placement sensitivity is still unexplained. Don't modify adapters.
- **`int32 uniform n=4`:** passes in the matrix (1.837 [1.780–1.896]). It was inconclusive in `calib-a` only.
- **`int64 split-line-translated` 10⁵ G2:** passes (1.767 [1.707–1.830]).
- **G9 fast path** (`detail/quadtree-index-impl.h`, `orderedCommon` / `orderedSelf`): if further work touches it, keep ordered prefixes, Stop/counters, fallback and zero warm allocations.

## Provenance

| Runtime | Release `quadtree-bench` SHA-256 |
|---|---|
| `95c9178` | `546d79c7035b2f5e1f0639c09b3ebb367f40a007bbc79d79ddd005ffc8fcd82b` |
| `ec67d81` | `2090617b229d379ff82cd4ae929aa026598e33c801a69de3fe8222203ba960e2` |
| `c13e498` | `f7d8968a68ac97be29a7d28626dfaa9874069c2d66c6392c9f0a31cceaf7394c` (reproduced by the clean archive build) |

`c13e498` hashes: benchmark `.text` `369ce16eb6696e9d58748079dbac65aa920b9afdca9bbfce9a929e0f67129204`; runtime header `0264f71b739185170ffc47bd5ce7d475d8a260c2ce85ee18ef107962c056b58a`; Box2D v3.2.0 archive `ffec747b27a2267016f8d0240d658c82d187fb2271fa0f69fcbe53fbc067180d`.
