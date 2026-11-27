# Handoff: `int32` release acceptance holds on `c13e498`; promotion pending

## Repo state

- Branch `quadtree-index` in `/home/dima/src/quadtree.git`. The tested runtime is `c13e498`; later commits change only results and docs. Local commits are not pushed to `origin/quadtree-index`, which is still at `95c9178`. For hosted CI, `c13e498` was pushed as `origin/validate/int32-ordered-common`. Leave untracked `build-review/` alone.
- Validation record: [`bench/results/release-c13e498-2026-11-26/`](bench/results/release-c13e498-2026-11-26/README.md). Both the [rule](bench/results/release-c13e498-2026-11-26/resolution-rule.md) (`13629a3`) and the [rerun rule](bench/results/release-c13e498-2026-11-26/rerun-rule.md) (`4d121a5`) were committed before their stages ran.

## Result

- **`int32`: accepted on `c13e498`.**
  - First run, snapshot: G1 16/16, G2 20/20 (two 10⁵ rows after repeats), G9 15/15; G3, G4 and G6 pass.
  - First run, persistent: G6 216, G7 120, G8a 12, G8b 4 and G8c 24 pass.
  - No exception used.
  - The first calibration pair reached 94.3%, below the 95% target. By owner decision, the calibration pair and the `int32` snapshot matrix were rerun under the same rule. The rerun reached 98.3%, and every `int32` snapshot hard gate passed in the matrix without repeats.
- **All four types:** G0 and G6 pass, all persistent hard gates pass, and all 6,944 expected engine rows are present. CTest passes 22/22, the full differential fuzz passes 20/20 (seeds 1–40, 1500 steps), and hosted [Portability #21](https://github.com/red-9m/quadtree/actions/runs/45887625703) passes 9/9 on the exact SHA.
- **Non-`int32` timing (Task 09, not blocking):** `float` G2 `uniform` 10³ and G9 `split-line-translated` 10⁵, and `double` G2 `uniform` 10³/10⁵, remain inconclusive after pooling. No row fails, and `int64` passes everything.

## Next

1. **Promote `c13e498`**, with owner approval for any push or merge: push `quadtree-index` (runtime plus evidence commits) and follow the earlier promotion pattern (`promote/quadtree-index`, `docs/plan` records).
2. **Task 09 (optional):** the four inconclusive non-`int32` timing rows above.
3. **Task 02 follow-up (optional):** `float clustered n=2` placement sensitivity. It passed in this release matrix (1.496 [1.440–1.554]).

## Watch items

- **Host interruptions:** a host shutdown killed the first `int32-persistent` stage. It is kept as `int32-persistent-aborted/`, and the stage was rerun from scratch. Keep other agent sessions and browsers idle during timing runs; a load spike near the end of the first `calib-b` caused most of its calibration misses.
- **G9 fast path** (`detail/quadtree-index-impl.h`, `orderedCommon` / `orderedSelf`): if further work touches it, keep ordered prefixes, Stop/counters, fallback and zero warm allocations.

## Provenance

| Runtime | Release `quadtree-bench` SHA-256 |
|---|---|
| `95c9178` | `546d79c7035b2f5e1f0639c09b3ebb367f40a007bbc79d79ddd005ffc8fcd82b` |
| `ec67d81` | `2090617b229d379ff82cd4ae929aa026598e33c801a69de3fe8222203ba960e2` |
| `c13e498` | `f7d8968a68ac97be29a7d28626dfaa9874069c2d66c6392c9f0a31cceaf7394c` (clean archive build, workspace `~/.cache/quadtree-release-c13e498.OIHE`) |

`c13e498` hashes: benchmark `.text` `369ce16eb6696e9d58748079dbac65aa920b9afdca9bbfce9a929e0f67129204`; runtime header `0264f71b739185170ffc47bd5ce7d475d8a260c2ce85ee18ef107962c056b58a`; Box2D v3.2.0 archive `ffec747b27a2267016f8d0240d658c82d187fb2271fa0f69fcbe53fbc067180d`.
