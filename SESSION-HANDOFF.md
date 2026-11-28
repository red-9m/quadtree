# Handoff: all four coordinate types accepted on `ce92ef5`

## Status

- **Release:** `int32`, `int64`, `float` and `double` are performance-accepted on runtime `ce92ef5` (2026-11-28).
  - Every hard snapshot and persistent gate passes for every type, with no repeats and no exceptions.
  - The `int32` acceptance holds. Task 09 is done.
  - Evidence: [release validation](bench/results/release-ce92ef5-2026-11-28/README.md), [candidate evidence](bench/results/float-common-pairs-2026-11-27/README.md), [08 result](docs/plan/08-final-acceptance-result.md#recheck-on-ce92ef5), [09 result](docs/plan/09-other-coordinate-types-result.md).
- **No release work is pending.** The items below are optional or need an owner decision.

## Repo and branches

- Work in `/home/dima/src/quadtree.git`, branch `quadtree-index`. Leave untracked `build-review/` alone.
- `ce92ef5` is the tested runtime. Later commits change only results and docs.
- Remote branches:

| Branch | Head | Notes |
|---|---|---|
| `origin/quadtree-index` | see `git log origin/quadtree-index -1` | Fast-forwarded from `3043ef0` to include `ce92ef5`, the validation records, docs and this handoff. |
| `origin/validate/float-common-pairs` | `ce92ef5` | Pushed for hosted CI ([Portability #24](https://github.com/red-9m/quadtree/actions/runs/46095847492), 9/9). Can be deleted with owner approval. |
| `origin/validate/int32-ordered-common` | `c13e498` | Old CI branch; can be deleted with owner approval. |
| `origin/promote/quadtree-index` | `2c31609` | Stale, from the 2026-11-21 promotion. |
| `origin/main` | `f4390e5` | Not merged; owner decision. |

- **Tools:** `gh` is not installed. Read CI through the public REST API, for example `curl -s 'https://api.github.com/repos/red-9m/quadtree/actions/runs?head_sha=<full sha>'`; the full SHA is required. `collect-ci.py` in a release directory writes `ci.json`. Pushes use SSH (`git@github.com:red-9m/quadtree.git`).

## `ce92ef5` change

- **Builds:** bulk `float`/`double` builds record the common-filter-bit proof (as `int64` already did).
- **Unordered pairs:** use the `CommonBitPairs` kernel with the exact packed overlap test.
- **Ordered pairs:** use `orderedCommonFloat`, a compact ordered walk on float lanes.
  - Exact for `float`.
  - For `double`, lanes are rounded toward −∞ and every hit is rechecked exactly. Exact thread-node bounds keep work counters identical.
- **`int32` isolation:** `orderedCommon` is `noinline`. Without it, GCC's inlining shifts in the four-type bench TU slowed `int32` ordered n=100 by 5–11%. With it, every `int32` `Index` function is instruction-identical to `c13e498` plus the attribute.
- **Known costs** (G5/ungated, published):
  - `pile` unordered pairs are 19–28% slower for `float`/`double`, as for `int64`. Two inline-report variants were rejected; they cost 2–5% on gated rows.
  - `int64` unordered is 1–7% slower at 10²–10⁴ from compiler inlining shifts.
- **Tests:**
  - `pairPrefixes<T>` covers `int64`/`float`/`double`.
  - `floatCommonPairs<T>` covers contact, ulp gaps, signed zeros, subnormals, extremes and all rounding modes.
  - The fuzzer shares filter bit 8 on seeds where seed mod 7 = 3.

## Next (pick one; none is required)

1. **Merge `quadtree-index` into `main`:** owner decision. Do not merge or push other branches without approval.
2. **Thinnest margins on `ce92ef5`**, if more headroom is wanted:
   - `int32` G2 `uniform` 10³: 3.829 [3.712–3.950].
   - `int32` G1 n=4: upper ≤ 1.978.
   - `float`/`double` G9 `split-line*` 10⁵: upper ≤ 3.82.
3. **`pile` hit path for `commonRow`:** only worth it if it costs nothing on regular rows (see `bench/results/float-common-pairs-2026-11-27/variants/`).

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

## Provenance

| Runtime | Release `quadtree-bench` SHA-256 |
|---|---|
| `c13e498` | `f7d8968a68ac97be29a7d28626dfaa9874069c2d66c6392c9f0a31cceaf7394c` |
| `ce92ef5` | `f1d1694a2e13a9cb36140418363ad647eaa6aa831bebdc1bc9fd16e1afc2fba2` |

- **`ce92ef5` hashes:** benchmark `.text` `f4905252e1cc1ca1331b19b729a0266beeced286ca7e0ab2adf6bddbaae9a5be`; runtime header `8c653e4f5ed61a1154f7c34a0e09a8da7665cc04782f61d92dc468fcdb7e2217`; `quadtree-index-fuzz` `655d039b4e3e5b603e355e4d750e5edf776d975a31ba412511c4df4d1864872e`; Box2D v3.2.0 archive `ffec747b27a2267016f8d0240d658c82d187fb2271fa0f69fcbe53fbc067180d`.
- **Verified build:** the archive build is in `~/.cache/quadtree-release-ce92ef5.oEvT` (path in `~/.cache/quadtree-release-ce92ef5.current`). Dev builds and A/B scratch are in `~/.cache/qt-dev/`.
