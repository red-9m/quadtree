# Handoff: all four coordinate types accepted on `db0c820`

## Status

- **Release:** `int32`, `int64`, `float` and `double` are performance-accepted on runtime `db0c820` (2026-11-28) ([validation](bench/results/release-db0c820-2026-11-28/README.md), [08 result](docs/plan/08-final-acceptance-result.md#recheck-on-db0c820), [09 result](docs/plan/09-other-coordinate-types-result.md)).
  - `int64`, `float` and `double` pass every hard gate under the pre-specified rule.
  - `int32` passes all but one hard gate. G8b `mixed-size` n=10⁵ `small-single/moved` stayed open under the rule: pooled 3.901 [3.781–4.024], limit 4. **Owner decision (2026-11-28):** `int32` accepted on `db0c820` on that evidence.
    - The `int32` code is instruction-identical to `ce92ef5`, which passed that row at 3.324.
    - A [post-hoc diagnostic](bench/results/release-db0c820-2026-11-28/diag-g8b/) gives `c13e498` the same 3.8–4.0 in the repeat-process shape.
- **Defect history:** `ce92ef5` passed its own validation. Its ordered `double` path then turned out to raise `FE_OVERFLOW`/`FE_INEXACT` on valid input, and to fault with SIGFPE when trapping is enabled ([fix evidence](bench/results/double-ordered-fp-flags-2026-11-28/README.md)). `db0c820` uses exception-free integer order keys, and `pairsRaiseNoFpFlags<T>` guards against a regression.
- **No release work is pending.** Everything below is optional or needs an owner decision.

## Repo and branches

- Work in `/home/dima/src/quadtree.git`, branch `quadtree-index`. Leave untracked `build-review/` alone.
- `db0c820` is the tested runtime. Later commits change only results and docs; `git diff db0c820 HEAD -- detail tests bench/*.cpp bench/*.h` is empty.
- Remote branches:

| Branch | Head | Notes |
|---|---|---|
| `origin/quadtree-index` | `git log origin/quadtree-index -1` | Includes `ce92ef5`, `db0c820`, both validation records, the owner decision, docs and this handoff. Portability passes on each pushed head (#24–#29). |
| `origin/validate/double-fp-flags` | `db0c820` | CI branch ([#27](https://github.com/red-9m/quadtree/actions/runs/46138155025), 9/9). Delete with owner approval. |
| `origin/validate/float-common-pairs` | `ce92ef5` | CI branch ([#24](https://github.com/red-9m/quadtree/actions/runs/46095847492), 9/9). Delete with owner approval. |
| `origin/validate/int32-ordered-common` | `c13e498` | Old CI branch. Delete with owner approval. |
| `origin/promote/quadtree-index` | `2c31609` | Stale, from the 2026-11-21 promotion. |
| `origin/main` | `f4390e5` | Not merged; owner decision. |

- **Tools:**
  - `gh` is not installed. Read CI through the public REST API: `curl -s 'https://api.github.com/repos/red-9m/quadtree/actions/runs?head_sha=<full sha>'`. The full SHA is required.
  - `collect-ci.py` in a release directory writes `ci.json`.
  - Pushes use SSH (`git@github.com:red-9m/quadtree.git`).
- **Dev helpers (not committed) in `~/.cache/qt-dev/`:**
  - `ab.sh`: quick A/B timing on CPU 6.
  - `cmp.py`: A/B summary with phase medians.
  - `exp.sh SRC TAG`: compiles the bench TU against a header tree and diffs the `int32`/`int64` `Index` symbol sizes against `ctl`.
  - `bin/`: release binaries (`base`/`c13e` = `c13e498`, `ce92`/`prev` = `ce92ef5`, `db0c`/`next` = `db0c820`).

## What `ce92ef5` + `db0c820` changed

- **Builds:** bulk `float`/`double` builds record the common-filter-bit proof, as `int64` already did. `insert`/`setFilter` clear it.
- **Unordered pairs:** the `CommonBitPairs` kernel (`commonRow`) with the exact packed overlap test (`kernelOverlap`).
- **Ordered pairs:** `orderedCommonFloat` threads nodes into build-box scratch and keeps 24-byte rank-ordinal records in build-key scratch.
  - `float` lanes are exact.
  - `double` lanes are integer order keys (`orderedLanes`: high word of the sortable image, bit operations only). Every hit is rechecked exactly with `packedCrossItemOverlap`.
  - Thread nodes keep exact `double` bounds, so work counters equal the general walk's.
- **`int32` isolation:** `orderedCommon` is `noinline`. With it, every `int32` `Index` function in the bench object is instruction-identical to `c13e498` plus that attribute.
- **Tests:**
  - `pairPrefixes<T>` (`int64`/`float`/`double`).
  - `floatCommonPairs<T>`: contact, ulp gaps, signed zeros, subnormals, extremes, all rounding modes.
  - `pairsRaiseNoFpFlags<T>`.
  - The fuzzer shares filter bit 8 on seeds with seed mod 7 = 3.
- **Evidence:**
  - [candidate](bench/results/float-common-pairs-2026-11-27/README.md)
  - [`double` fix](bench/results/double-ordered-fp-flags-2026-11-28/README.md)
  - [`ce92ef5` validation](bench/results/release-ce92ef5-2026-11-28/README.md) (superseded)
  - [`db0c820` validation](bench/results/release-db0c820-2026-11-28/README.md)

## Further tasks (owner decisions)

1. **Merge `quadtree-index` into `main`.** Do not merge or push other branches without approval.
2. **Branch cleanup:** delete the three `validate/*` branches and the stale `promote/quadtree-index`.

## Pending investigations

Ranked by value for the next release validation.

1. **G8b repeat-process bias (method).** For `int32 mixed-size small-single/moved`, the n=10³ step is about 220 ns in every process. The n=10⁵ step differs by process shape:

   | Process | n=10⁵ step |
   |---|---|
   | Full persistent matrices (`c13e498`, `ce92ef5`) | 738–747 ns |
   | `db0c820` matrix (noisy, interval 2.903–5.086) | 849 ns |
   | Two-row 45-round repeats, all binaries | 847–886 ns |

   - **Effect:** the rule's G8b repeats measure a ratio about 0.5 higher than the matrix, so an inconclusive G8b row cannot resolve to pass.
   - **Likely cause** (unverified): heap/page state, since a matrix process has already run many workloads.
   - **First step:** in one process, run the repeat arguments preceded by the matrix's earlier `mixed-size` rows. Compare with a cold two-row process, and with transparent huge pages off.
   - **Then:** fix the repeat design (for example, repeat the whole scene's persistent block) in the next pre-specified rule.
2. **FP-flag contract beyond full pairs.**
   - **Scope:** `pairsRaiseNoFpFlags<T>` covers `forEachPair` only. Extend it to region queries, `queryPartners`, moved-set pairs (`forEachPairInvolving`), cross-index pairs and `forEachNode`.
   - **Decide and document:** may builds/mutations raise `FE_INEXACT`? The bulk planner and cells compute midpoints. Then audit `detail/quadtree-index-impl.h` for any conversion or arithmetic on query paths.
3. **Build after an ordered fast-path query is slower.**
   - **Symptom:** in the candidate runs, `index-ordered` prepare (the build) rose 0.3–1.9 ms at 10⁵ for `float`/`double` (for example, `double split-line` 12.5 → 13.9 ms). `index` prepare and the pair phase did not rise.
   - **Suspect:** `orderedCommon*` writes thread nodes and records into the build scratch (`mBuildBoxes`, `mTemp`), and the next build pays for the dirty lines.
   - **Next:** check with `perf stat` (cache misses) on a build-only vs build-after-query loop. A fix would also widen float/double G9 margins.
4. **Fuzz does not catch a stale common-filter proof.**
   - **Symptom:** a mutation that never clears the proof on `setFilter` fails `index-test` (10,745 failures) but not the fuzzer. Inserts clear the proof soon after a dropped bit.
   - **Fix idea:** in shared-filter seeds, run a pair check right after a `setFilter` that drops the bit, before any insert.
5. **Bench TU code-generation coupling.**
   - **Symptom:** any header change shifts GCC inlining for every coordinate type in the single four-type bench TU. This session `int32` ordered n=100 moved 5–11%, and `int64` unordered moved 1–7%, with no source change to those paths.
   - **Options:** commit an `int32`/`int64` normalized-`objdump` identity check (as `exp.sh` did by hand), or use `noinline` boundaries for other per-type kernels. Per-type bench TUs would change the benchmark itself and need a new baseline.

## Improvements (optional, measure first)

1. **Thinnest margins on `db0c820`:**
   - `int32` G2 `uniform` 10³: 3.842 [3.718–3.970].
   - `int32` G1 `uniform` n=4: 1.851 [1.774–1.932]. It is placement-sensitive; the earlier `ce92ef5` run had `clustered` n=4 inconclusive at upper 2.020 with equal index times.
   - `float`/`double` G9 `split-line` 10⁵: upper ≤ 3.863.
   - `int64` G2 `uniform` 10⁵: 1.873 [1.811–1.937].
2. **`int64` unordered regression from inlining shifts** (1–7% at 10²–10⁴ vs `c13e498`, ungated): try isolating `commonRow`/`selfPairs` for `int64` the way `orderedCommon` was isolated.
3. **`pile` hit path in `commonRow`:** `float`/`double` unordered `pile` is 19–28% slower than at `c13e498` (as `int64` already was), from one out-of-line `reportPair` per hit.
   - **Rejected:** a `flatten` variant and an always-inline row report. They cut `pile` 25–34%, but cost 2–5% on gated rows, and one made `float mixed-size` 10⁵ unstable ([variants](bench/results/float-common-pairs-2026-11-27/variants/)).
   - **Ideas:** batch hits per row, then report.
4. **`double` prefilter precision:** order keys keep 20 mantissa bits (float keeps 23). Dense `double` scenes far from the origin get more exact rechecks. Worth it only if a scene shows recheck cost.
5. **Out of scope unless requested:** MSVC is deferred ([06](docs/plan/06-portability-result.md)), and so is the [legacy safety patch](docs/plan/legacy-safety.md).

## Timing-run lessons

- **Template:** use `run.sh`/`matrices.sh` in the latest release directory. CPU 6 with sibling 7 idle, EPP and platform profile `performance` (restored afterwards), HPET, and quiet starts (≤ 40 °C, ≤ 5% busy). `run.sh` asserts `hpet`; after a reboot the clock source came up as `hpet`, while earlier dev runs were on `tsc`.
- **Rules first:** commit the decision rule before any stage runs. Publish original verdicts and repeats separately. An aborted stage becomes `*-aborted` and is rerun from scratch; host shutdowns killed one run in each of the last two sessions.
- **Long runs:** start with `setsid nohup …` and poll with sleeps of 5 minutes or less, one tool call at a time (parallel sleeping calls ran concurrently).
- **A/B comparisons:**
  - Use equal-length binary and output paths (`base`/`cand`). Path length shifts tiny-row timing.
  - Never build while a timing run is active.
- **Code generation:** check `int32` against the accepted runtime after any header change, using `nm -S` sizes and normalized `objdump` of `Index<…, int, …>`, ignoring call/jump targets.
- **G9 fast paths** (`orderedCommon`, `orderedCommonFloat`, `orderedSelf`): keep ordered prefixes, Stop/counters, fallback and zero warm allocations.
- **No FP side effects in queries:** use integer or bit operations, never conversions, in prefilters.
- **Self-check:** before publishing a summary, check each claim about the control against the per-process gate lines. A README overstated the control's inconclusive rows this session.

## Provenance

| Runtime | Release `quadtree-bench` SHA-256 |
|---|---|
| `c13e498` | `f7d8968a68ac97be29a7d28626dfaa9874069c2d66c6392c9f0a31cceaf7394c` |
| `ce92ef5` | `f1d1694a2e13a9cb36140418363ad647eaa6aa831bebdc1bc9fd16e1afc2fba2` |
| `db0c820` | `e015f86dac364ae1e658ec3f7698013212b69c293d4fac0f28f8b672a41f6337` |

- **`db0c820` hashes:** benchmark `.text` `257d26283bbcc1441058f4186507b5f28d3d70c00581ecfd17f8a61bdb1282ab`; runtime header `81b2795b7b333e87fb9977d5fa4bda66f42e93198a2053280209984625b2d605`; `quadtree-index-fuzz` `488f2cd758586490a48c409965e1075c9275a70e7929d56ea3f0ea0bf5bd4b0b`; Box2D v3.2.0 archive `ffec747b27a2267016f8d0240d658c82d187fb2271fa0f69fcbe53fbc067180d`.
- **`ce92ef5` hashes:** benchmark `.text` `f4905252e1cc1ca1331b19b729a0266beeced286ca7e0ab2adf6bddbaae9a5be`; runtime header `8c653e4f5ed61a1154f7c34a0e09a8da7665cc04782f61d92dc468fcdb7e2217`.
- **Verified builds:** clean archive builds are in `~/.cache/quadtree-release-db0c820.G9F0` and `~/.cache/quadtree-release-ce92ef5.oEvT` (paths in `~/.cache/quadtree-release-*.current`), plus `c13e498` in `~/.cache/quadtree-release-c13e498.OIHE`.
