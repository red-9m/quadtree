# Handoff: `c13e498` accepted for `int32` and promoted to `origin/quadtree-index`

## Status

- **Release:** the `int32` release acceptance holds on runtime `c13e498` (2026-11-27). Every hard gate passes and no exception is used. `int64`/`float`/`double` are functionally supported but not performance-accepted (Task 09). Evidence: [`bench/results/release-c13e498-2026-11-26/`](bench/results/release-c13e498-2026-11-26/README.md), [08 result](docs/plan/08-final-acceptance-result.md#recheck-on-c13e498).
- **No release work is pending.** The open items below are optional or need an owner decision.

## Repo and branches

- Work in `/home/dima/src/quadtree.git`, branch `quadtree-index`. Leave untracked `build-review/` alone.
- `c13e498` is the tested runtime. All later commits change only results and docs.
- Remote branches:

| Branch | Head | Notes |
|---|---|---|
| `origin/quadtree-index` | `3043ef0` | Fast-forwarded from `95c9178`. [Portability #22](https://github.com/red-9m/quadtree/actions/runs/45930658113) passed 9/9 on `f249039`. [#23](https://github.com/red-9m/quadtree/actions/runs/45933623184) on `3043ef0` (docs only) was in progress at handoff. This handoff commit is local only. |
| `origin/main` | `f4390e5` | 59 commits behind `quadtree-index`. Not merged. |
| `origin/validate/int32-ordered-common` | `c13e498` | Pushed only for hosted CI ([#21](https://github.com/red-9m/quadtree/actions/runs/45887625703), 9/9). Can be deleted with owner approval. |
| `origin/promote/quadtree-index` | `2c31609` | Stale; from the 2026-11-21 promotion. |

- **Tools:** `gh` is not installed. Read CI through the public REST API, for example `curl -s 'https://api.github.com/repos/red-9m/quadtree/actions/runs?head_sha=<sha>'`. Use `bench/results/int64-dense-pairs/collect-ci.py` to write a `ci.json` record. Pushes use SSH (`git@github.com:red-9m/quadtree.git`).

## `c13e498` evidence

- **Build:** clean `git archive`, Release, GCC 16.2.1, CMake 4.4.2, `QUADTREE_BENCH_NATIVE=OFF`. Box2D v3.2.0 is the archive pinned by `URL_HASH`. CTest passes 22/22, and the full differential fuzz passes 20/20 (4 types × 5 scenes, seeds 1–40, 1500 steps).
- **Validation:** all four types, 5-round snapshot and 64-step persistent matrices, under a rule committed before the runs (`13629a3`).
  - `int32` snapshot: G1 16/16, G2 20/20 (`split-line-translated`/`uniform` 10⁵ pass after two 45-round repeats), G9 15/15, plus G3, G4 and G6.
  - `int32` persistent: G6 216, G7 120, G8a 12, G8b 4, G8c 24.
  - All types: G0 and G6 pass, all persistent hard gates pass, and 6,944/6,944 expected rows are present.
- **Calibration:**
  - The first pair reached 94.3%, below the 95% target, so the rule withheld sign-off.
  - Owner decision: rerun under the same rule (`4d121a5`). The rerun pair reached 98.3%, and the `int32` snapshot matrix passed all hard gates without repeats (`data-rerun/`).
- **Open non-`int32` timing rows** (pooled, inconclusive, not blocking): `float` G2 `uniform` 10³ 4.023 [3.939–4.110]; `float` G9 `split-line-translated` 10⁵ 4.031 [3.950–4.113]; `double` G2 `uniform` 10³ 3.966 [3.880–4.055] and 10⁵ 1.982 [1.942–2.024]. No row fails; `int64` passes everything.

## Next (pick one; none is required for the `int32` release)

1. **Merge `quadtree-index` into `main`:** owner decision. Do not merge or push other branches without approval.
2. **Task 09:** the four open non-`int32` rows above ([brief](docs/plan/09-other-coordinate-types.md)). No exception extends beyond `int32`.
3. **Task 02 follow-up:** `float clustered n=2` input-placement sensitivity. It passed in the release matrix (1.496 [1.440–1.554]). Don't modify adapters.

MSVC is deferred ([06](docs/plan/06-portability-result.md)). The [legacy safety patch](docs/plan/legacy-safety.md) is out of scope unless requested.

## Timing-run lessons

- Use `run.sh`/`matrices.sh` in the results directory as the template: CPU 6 with sibling 7 idle, EPP and platform profile `performance` (restored afterwards), HPET, and quiet starts (≤ 40 °C, ≤ 5% busy).
- Commit the decision rule before any stage runs. Publish original verdicts and repeats separately. An aborted stage becomes `*-aborted` and is rerun from scratch.
- Start long runs with `setsid nohup …` and poll with sleeps of 5 minutes or less. A host shutdown killed one stage this session, and an aborted tool call can interrupt waiting.
- Keep other agent sessions and browsers idle during timing runs. A load spike near the end of the first `calib-b` (Tctl 83–90 °C) caused most of its misses.
- If later work touches the G9 fast path (`detail/quadtree-index-impl.h`, `orderedCommon` / `orderedSelf`), keep ordered prefixes, Stop/counters, fallback and zero warm allocations.

## Provenance

| Runtime | Release `quadtree-bench` SHA-256 |
|---|---|
| `95c9178` | `546d79c7035b2f5e1f0639c09b3ebb367f40a007bbc79d79ddd005ffc8fcd82b` |
| `ec67d81` | `2090617b229d379ff82cd4ae929aa026598e33c801a69de3fe8222203ba960e2` |
| `c13e498` | `f7d8968a68ac97be29a7d28626dfaa9874069c2d66c6392c9f0a31cceaf7394c` |

- **`c13e498` hashes:** benchmark `.text` `369ce16eb6696e9d58748079dbac65aa920b9afdca9bbfce9a929e0f67129204`; runtime header `0264f71b739185170ffc47bd5ce7d475d8a260c2ce85ee18ef107962c056b58a`; `quadtree-index-fuzz` `0324fede8144c43e91611539ca380bef7a7ca07043c6c956b15df2bc77f8cfef`; Box2D v3.2.0 archive `ffec747b27a2267016f8d0240d658c82d187fb2271fa0f69fcbe53fbc067180d`.
- **Verified build:** the workspace `~/.cache/quadtree-release-c13e498.OIHE` (path in `~/.cache/quadtree-release-c13e498.current`) holds the verified build.
- **Old binaries:** the earlier `/tmp/g14` and `/tmp/g9t` executables were lost at the reboot. Recreate them from the tested commits if needed.
