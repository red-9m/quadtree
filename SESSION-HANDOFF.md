# Handoff: Task 09 accepted on `db0c820`; one `int32` row open for an owner decision

## Status

- **`int64`, `float`, `double`:** performance-accepted on `db0c820` (2026-11-28). Every hard snapshot and persistent gate passes ([validation](bench/results/release-db0c820-2026-11-28/README.md), [09 result](docs/plan/09-other-coordinate-types-result.md)).
- **`int32`:** acceptance holds on `c13e498`, and `ce92ef5` passed every `int32` gate. On `db0c820`, every snapshot gate passes, but G8b `mixed-size` n=10⁵ `small-single/moved` stayed **open** under the rule: matrix 3.843 [2.903–5.086]; pooled repeats 3.901 [3.781–4.024], limit 4.
  - A post-hoc diagnostic ([diag-g8b](bench/results/release-db0c820-2026-11-28/diag-g8b/)) shows `c13e498` and `ce92ef5` also measure 3.8–4.0 in the repeat-process shape.
  - The `int32` code is instruction-identical to `ce92ef5`.
- **Owner decision needed:** accept `int32` on `db0c820` on this evidence, or rerun under a new pre-specified rule. A rerun that reuses the same repeat design would likely end open again: the repeat processes run about 3.9 on every binary.
- **Defect history:** `ce92ef5` passed its own validation. Its ordered `double` path then turned out to raise `FE_OVERFLOW`/`FE_INEXACT` on valid input, and SIGFPE when trapping is enabled ([fix evidence](bench/results/double-ordered-fp-flags-2026-11-28/README.md)). `db0c820` uses exception-free integer order keys, and `pairsRaiseNoFpFlags<T>` guards against a regression.

## Repo and branches

- Work in `/home/dima/src/quadtree.git`, branch `quadtree-index`. Leave untracked `build-review/` alone.
- `db0c820` is the tested runtime. Later commits change only results and docs.
- Remote branches:

| Branch | Head | Notes |
|---|---|---|
| `origin/quadtree-index` | see `git log origin/quadtree-index -1` | Includes `ce92ef5`, `db0c820`, both validation records, docs and this handoff. |
| `origin/validate/double-fp-flags` | `db0c820` | Pushed for hosted CI ([Portability #27](https://github.com/red-9m/quadtree/actions/runs/46138155025), 9/9). Can be deleted with owner approval. |
| `origin/validate/float-common-pairs` | `ce92ef5` | Hosted CI [#24](https://github.com/red-9m/quadtree/actions/runs/46095847492), 9/9. Can be deleted with owner approval. |
| `origin/validate/int32-ordered-common` | `c13e498` | Old CI branch; can be deleted with owner approval. |
| `origin/promote/quadtree-index` | `2c31609` | Stale, from the 2026-11-21 promotion. |
| `origin/main` | `f4390e5` | Not merged; owner decision. |

- **Tools:** `gh` is not installed. Read CI through the public REST API, for example `curl -s 'https://api.github.com/repos/red-9m/quadtree/actions/runs?head_sha=<full sha>'`; the full SHA is required. `collect-ci.py` in a release directory writes `ci.json`. Pushes use SSH (`git@github.com:red-9m/quadtree.git`).

## `ce92ef5` change (with the `db0c820` fix)

- **Builds:** bulk `float`/`double` builds record the common-filter-bit proof (as `int64` already did).
- **Unordered pairs:** use the `CommonBitPairs` kernel with the exact packed overlap test.
- **Ordered pairs:** use `orderedCommonFloat`, a compact ordered walk on float lanes.
  - Exact for `float`.
  - For `double`, lanes are integer order keys: the high word of the sortable image, built with bit operations only, so they raise no FP flags (`db0c820`). Every hit is rechecked exactly, and exact thread-node bounds keep work counters identical.
- **`int32` isolation:** `orderedCommon` is `noinline`. Without it, GCC's inlining shifts in the four-type bench TU slowed `int32` ordered n=100 by 5–11%. With it, every `int32` `Index` function is instruction-identical to `c13e498` plus the attribute.
- **Known costs** (G5/ungated, published):
  - `pile` unordered pairs are 19–28% slower for `float`/`double`, as for `int64`. Two inline-report variants were rejected; they cost 2–5% on gated rows.
  - `int64` unordered is 1–7% slower at 10²–10⁴ from compiler inlining shifts.
- **Tests:**
  - `pairPrefixes<T>` covers `int64`/`float`/`double`.
  - `floatCommonPairs<T>` covers contact, ulp gaps, signed zeros, subnormals, extremes and all rounding modes.
  - The fuzzer shares filter bit 8 on seeds where seed mod 7 = 3.

## Next

1. **Owner decision on the open `int32` G8b row** (above).
2. **Merge `quadtree-index` into `main`:** owner decision. Do not merge or push other branches without approval.
3. **Thinnest margins**, if more headroom is wanted:
   - `int32` G2 `uniform` 10³: 3.829 [3.712–3.950].
   - `int32` G1 n=4: upper ≤ 1.978.
   - `float`/`double` G9 `split-line*` 10⁵: upper ≤ 3.82.
4. **`pile` hit path for `commonRow`:** only worth it if it costs nothing on regular rows (see `bench/results/float-common-pairs-2026-11-27/variants/`).

MSVC is deferred ([06](docs/plan/06-portability-result.md)). The [legacy safety patch](docs/plan/legacy-safety.md) is out of scope unless requested.

## Timing-run lessons

- **Template:** use `run.sh`/`matrices.sh` in the latest release directory: CPU 6 with sibling 7 idle, EPP and platform profile `performance` (restored afterwards), HPET, and quiet starts (≤ 40 °C, ≤ 5% busy). After a reboot the clock source came up as `hpet`; earlier dev runs this session were on `tsc`.
- **Rules first:** commit the decision rule before any stage runs. Publish original verdicts and repeats separately. An aborted stage becomes `*-aborted` and is rerun from scratch; a host shutdown killed one candidate run this session.
- **Long runs:** start with `setsid nohup …` and poll with sleeps of 5 minutes or less.
- **A/B comparisons:**
  - Give both binaries and output directories equal-length paths (`base`/`cand`). Path length shifts tiny-row timing.
  - Do not build while a timing run is active.
- **Code generation:** any change to the header can move GCC inlining for every type in the four-type bench TU. Check `int32` symbols (`nm -S`, normalized `objdump`) against the accepted runtime.
- **G9 fast path:** if later work touches it (`orderedCommon`, `orderedCommonFloat`, `orderedSelf`), keep ordered prefixes, Stop/counters, fallback and zero warm allocations.
- **No FP side effects in queries:** they must not raise FP flags; `pairsRaiseNoFpFlags<T>` checks this. Use integer or bit operations, never conversions, in prefilters.
- **G8b repeats:** the 45-round `presolve` processes (only the target rows) read about 0.5 higher than the full persistent matrix for every binary. Any new rule for that row should account for this.

## Provenance

| Runtime | Release `quadtree-bench` SHA-256 |
|---|---|
| `c13e498` | `f7d8968a68ac97be29a7d28626dfaa9874069c2d66c6392c9f0a31cceaf7394c` |
| `ce92ef5` | `f1d1694a2e13a9cb36140418363ad647eaa6aa831bebdc1bc9fd16e1afc2fba2` |
| `db0c820` | `e015f86dac364ae1e658ec3f7698013212b69c293d4fac0f28f8b672a41f6337` |

- **`ce92ef5` hashes:** benchmark `.text` `f4905252e1cc1ca1331b19b729a0266beeced286ca7e0ab2adf6bddbaae9a5be`; runtime header `8c653e4f5ed61a1154f7c34a0e09a8da7665cc04782f61d92dc468fcdb7e2217`; `quadtree-index-fuzz` `655d039b4e3e5b603e355e4d750e5edf776d975a31ba412511c4df4d1864872e`; Box2D v3.2.0 archive `ffec747b27a2267016f8d0240d658c82d187fb2271fa0f69fcbe53fbc067180d`.
- **`db0c820` hashes:** benchmark `.text` `257d26283bbcc1441058f4186507b5f28d3d70c00581ecfd17f8a61bdb1282ab`; runtime header `81b2795b7b333e87fb9977d5fa4bda66f42e93198a2053280209984625b2d605`; `quadtree-index-fuzz` `488f2cd758586490a48c409965e1075c9275a70e7929d56ea3f0ea0bf5bd4b0b`.
- **Verified builds:** the archive builds are in `~/.cache/quadtree-release-db0c820.G9F0` and `~/.cache/quadtree-release-ce92ef5.oEvT` (paths in `~/.cache/quadtree-release-*.current`). Dev builds, A/B scratch and the binaries used by the diagnostic (`~/.cache/qt-dev/bin/{c13e,ce92,db0c}`) are in `~/.cache/qt-dev/`.
