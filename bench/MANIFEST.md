# Benchmark manifest (DRAFT)

Phase 1 gate of the [original update plan](../docs/plan/archive/QUADTREE-UPDATE-PLAN.md#10-verification-and-acceptance) (sections 10 and 11). Status:

| Step | Content | State |
|---|---|---|
| 1 | Harness, fixtures, snapshot baselines (brute force, sweep, grid) | done |
| 2 | Persistent workloads, Box2D ≥ 3.2 `b2DynamicTree` baselines, moved-set pairs | done |
| 3 | Straddling/mixed-size prototypes, ordered (QT-FR-24) vs unordered, persistent loose tree (3b) | done |
| 4 | Frozen limits, gate evaluator, published raw results | done |

Acceptance limits are frozen (see "Frozen acceptance limits"); published
results are under `bench/results/`.

The library engines `index` and `index-ordered` now run alongside the prototypes
in both models (`bench-index.h`). They use `QuadTree::Resource` allocations
counted by the harness, prepared workspace, and the library's exact budgets.
See `results/index-development/README.md`: correctness and reserved allocation
checks pass, but G1/G2 and some inconclusive timing gates remain open. These
measurements do not declare release acceptance.

Follow-up mutation/ordered moved-set fixes have targeted before/after results in
`results/index-fixes/README.md`. Run `quadtree-index-regression-bench` for those
batched fixtures; its CTest checks results/allocations, not timing thresholds.

`quadtree-index-fuzz` is a randomized differential fuzzer for the library
`Index` (int32, int64, float, double; `uniform`, `clustered`, `mixed`, `pile`,
`outliers` scenes with full-range and near-limit boxes). Seeds also vary small-set,
split/merge, depth, boundary and extent configuration. After every
insert/update/setFilter/remove/build/shrink/clear step it checks the internal
tree: own-list links, `nextItem` shortcut, counts, placement (`belongs`), exact
subtree bounds/filter composition, conservative own summaries, and pooled node
count. Every 7 steps it compares entries, unordered and ordered full pairs,
unordered and ordered moved sets (empty/single/sparse/dense, duplicate
rejection), region and partner queries with a brute-force oracle.

```sh
build-bench/bench/quadtree-index-fuzz --quick   # CTest "index-fuzz-quick", < 1 s
build-bench/bench/quadtree-index-fuzz           # 40 seeds x 1500 steps, ~12 s at -O2
build-bench/bench/quadtree-index-fuzz --types=float --scenes=outliers --seed=7 --steps=300
```

A failure prints the reproducing options and exits with 1. For memory and UB
checks, build it separately with `-fsanitize=address,undefined` (about 60 s for
`--seeds=10`). It reads private state through `#define private public`: it is a
development tool, not a portable test.

## Running

```sh
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench --target quadtree-bench
build-bench/bench/quadtree-bench --csv=results.csv --raw=samples.csv   # snapshot model, ~3 min
build-bench/bench/quadtree-bench --model=persistent --types=float --csv=persistent.csv  # ~25 min
build-bench/bench/quadtree-bench --quick       # both models, smoke run, also the CTest "bench-smoke"
build-bench/bench/quadtree-bench --list        # scenes and workloads
build-bench/bench/quadtree-bench --compare=old.csv     # run, then compare with an earlier run
build-bench/bench/quadtree-bench --diff=old.csv,new.csv  # compare two result files
```

Options: `--model=snapshot|persistent|all`, `--types=int32,int64,float,double|all`,
`--scenes=`, `--engines=`, `--sizes=`, `--small-sizes=`, `--batch=`,
`--persistent-sizes=`, `--workloads=`, `--modes=full,moved`, `--steps=`,
`--min-time=`, `--max-time=`, `--min-reps=`, `--max-reps=`, `--rounds=`,
`--process-error=`, `--candidate-limit=`, `--seed=`.

The optional benchmark build requires CMake 3.24 (the library target needs 3.21).
CMake: `-DQUADTREE_BENCH_NATIVE=ON` adds `-march=native` to the benchmark only.
Box2D (`-DQUADTREE_BENCH_BOX2D=ON`, default) is the pinned v3.2.0 source
(SHA-256 `ffec747b…180d`), built static with this project's configuration;
use `Release` (RelWithDebInfo enables Box2D asserts). `-DQUADTREE_BENCH_SYSTEM_BOX2D=ON`
uses an installed Box2D ≥ 3.2 instead; the Arch package 3.2.0-1.1 is built
without `NDEBUG` (asserts and validation active in `b2DynamicTree_MoveProxy`)
and is not a release baseline.

### Small-world broadphase (opt-in)

```sh
build-bench/bench/quadtree-bench --types=int32 --scenes=small-world-8,small-world-mixed \
  --engines=brute,index,index-ordered --small-sizes=1,2,4,8,16,32,64 \
  --sizes=128,256,512,1024,2048,4096 --csv=small-world.csv
```

Both scenes use a fixed 600×400 spawn area. `small-world-8` uses 8×8 boxes
for comparison with the consumer probe; `small-world-mixed` draws each side
independently from 4–64. The ordered adapter assigns unique ranks. The full pair
pass times counting and checksumming pairs, not writing a pair buffer, narrow
phase, or dispatch. Small-size counts in the CSV are batch totals; divide by
batch size for mean pairs per scene.

Local results at 4096 entries, seed 1 (Ryzen 9 5900HS, GCC 16.2.1, Release `-O3`,
`powersave` governor and `power` energy preference). Times are median / p95 ms for
rebuild plus the full pair pass. Both Index variants matched the brute-force
pair counts and checksums.

| Scene | Pairs | Index unordered (ms) | Index ordered (ms) | Brute force (ms) |
|---|---:|---:|---:|---:|
| `small-world-8` | 8,086 | 0.55 / 0.56 | 1.29 / 1.33 | 15.32 / 15.71 |
| `small-world-mixed` | 172,095 | 4.99 / 5.05 | 11.66 / 11.93 | 21.63 / 22.06 |

The 1 ms target at 4096 applies only to unordered `small-world-8`, and is not a
release gate. The mixed fixture is a density stress case: total box area is
about 20× the world area on average, and this seed reports 21× as many pairs as
the 8×8 fixture. Neither fixture runs by default or changes the frozen timing
gates.

## Method

- **Timer:** on x86-64 with an invariant TSC, `rdtsc` calibrated against
  `steady_clock` over 200 ms (7.8 ns per read here); otherwise `steady_clock`.
  This machine's kernel clock source is HPET: `steady_clock::now()` costs
  1.06 µs with 0.42 µs resolution, which added ~2.4 µs to every unbatched step
  before the switch (`uniform` n=100 grid: 5.2 → 1.77 µs). The timer and clock
  source are recorded in every result file.
- **Step (snapshot model):** `build(boxes)` plus full pair enumeration into a
  streaming visitor (pair count and order-independent checksum). Fixture
  generation and type conversion are excluded. Engines keep their buffers
  between steps (retained capacity).
- **Cold step:** first step on each fresh engine instance. The first round's
  cold step gives peak and retained bytes, cold allocation count,
  candidate/reference/pair counts, and the checksum; later cold steps must
  reproduce the counts.
- **Rounds:** all engines of a scene/type/size run in 5 interleaved rounds, in
  rotated order. Each round gives every engine a fresh instance, its own
  fixture copy at a shifted heap position (64-byte steps up to 64 KiB), and a
  shifted stack start (16-byte steps up to 4 KiB). Time drift and memory-layout
  effects, which otherwise stay fixed within a process, then vary between rounds.
- **Warm steps:** per round at least 3 repetitions and 0.1 s, at most 200
  repetitions or 0.8 s. Every step must reproduce the cold counts and
  checksum, otherwise the result is `NONDETERMINISTIC` (exit code 1).
- **Estimate and ±error (95%):** estimate = mean of the round medians.
  Error = √(e_r² + (p · estimate)²), with e_r the Student-t half-width over
  round medians and p the between-process term: 2% for the snapshot model
  (`--process-error`), 3% for the persistent model (`--process-error-persistent`).
- **Comparisons within a run:** `vs_best` is the geometric mean of per-round
  ratios to the fastest engine (paired, so shared drift cancels), with a
  t-interval on log ratios widened by p in quadrature.
- **Comparisons between runs:** `--compare`/`--diff` report new/old with
  √(rel_old² + rel_new²) as the log-scale 95% half-width: `faster` or `SLOWER`
  when the interval excludes 1, otherwise `same`.
- **Gate verdicts:** a ratio limit L passes when the interval's upper bound is
  ≤ L, fails when its lower bound is > L, and is inconclusive otherwise
  (rerun with more `--rounds`).
- **Also reported:** median, p95 (nearest rank) and minimum of per-scene step
  time; unbatched runs report median build and pairs time separately.
- **Small sizes (0–64):** batched; one sample runs 1024 distinct fixtures
  (`k = 0..1023`) and times the batch as a whole. Times are per scene; counts
  are batch totals. Absolute times are published; ratios on near-zero brute
  force times are not used.
- **Candidates:** entry-vs-entry overlap evaluations, counted before the
  outcome is known, repeats included. Brute force counts n(n-1)/2. The grid
  counts only pairs that pass its reporting-cell check.
- **Allocations and memory:** global `operator new`/`delete` replacement
  (`bench-memory.cpp`). Warm allocations per step and cold allocations are
  counted; peak and retained bytes come from the cold step. Aligned
  `operator new` is not counted. Library engines use a `QuadTree::Resource`
  backed by the harness's counted aligned allocation hooks. Adapter input,
  handle and moved-handle vectors are also included, not hidden as client storage.
- **Cross-check:** for every scene/type/size, all engines must report the same
  pair count and checksum (`MISMATCH`, exit code 1). Brute force is the
  reference where it runs.
- **Caps:** a step that exceeds 2·10⁸ candidates (`--candidate-limit`) is
  reported as `capped` and not timed. `pile` is capped at 4096 entries
  (n(n-1)/2 pairs). Capped results are listed, never dropped.
- **Persistent model:** see below; the same rounds, layout shifts, error and
  comparison rules apply, with one pass (initial insertion plus 64 steps) per
  engine and round.
- **Recorded metadata:** date, CPU, frequency governor, compiler and version,
  build configuration, effective flags, optimization state, seed, arguments.

## Fixtures

Fixtures are integer geometry, generated without floating point or standard
distributions; GCC/libstdc++, Clang/libstdc++ and Clang/libc++ produce
identical scene hashes, pair sets and candidate counts.

- **PRNG:** splitmix64. `next()`: `state += 0x9e3779b97f4a7c15`, then the
  finalizer `z ^= z >> 30; z *= 0xbf58476d1ce4e5b9; z ^= z >> 27;
  z *= 0x94d049bb133111eb; z ^= z >> 31`.
- **Uniform integer:** `U(lo, hi) = lo + next() % (hi - lo + 1)`.
- **Seed:** first `next()` of state
  `base ^ fnv1a64(scene) ^ (n << 20) ^ k`, with `base` from `--seed`
  (default 1) and batch member `k`. `split-line-translated` uses the
  `split-line` seed; `tiles-touch` uses `tiles`.
- **Scene hash:** `h = n`, then for each box and each of minX, minY, maxX, maxY:
  `h = mix64(h ^ value)`; batches fold member hashes with `h = mix64(h ^ member)`.
- **Coordinate mapping:** `static_cast<T>` of the integer edge. Exact for every
  scene except `outliers` in `float` (values up to 2²⁹ round to nearest).
- **Default world side:** `S(n) = 32 * ceil(sqrt(max(n, 1)))`, about 1024 square
  units per entry. Boxes are `{x, y, x + w, y + h}`.

Draws happen in the order listed.

| Scene | Boundary | Definition |
|---|---|---|
| `uniform` | default | `w = U(8,32)`, `h = U(8,32)`, `x = U(0, S-w)`, `y = U(0, S-h)` |
| `clustered` | default | `c = max(1, n/64)` centers `(U(0,S), U(0,S))`; per entry: cluster `U(0,c-1)`, `w`, `h` as uniform, `dx`, `dy` each = (sum of four `U(-64,64)`) / 2 truncated; `x = clamp(cx + dx - w/2, 0, S-w)`, same for `y` |
| `split-line` | default | Root side `W` = smallest power of two ≥ max(64, 2S); content square side `H = W/2`. Entries 0, 1: points `(0,0)` and `(W,W)` pin the data bounds to the root. Then per entry: `w`, `h` as uniform; `U(0,1) == 0`: `x = U(0,H-w)`, `y = U(0,H-h)`; else level `l = U(1, L)` with `L` the largest level ≤ 6 such that `H >> l ≥ 32`, odd `k = 2 U(0, 2^(l-1)-1) + 1`, line `k (H >> l)`, axis `U(0,1)`: vertical `x = line - U(1,w-1)`, `y = U(0,H-h)`; horizontal `y = line - U(1,h-1)`, `x = U(0,H-w)` |
| `split-line-translated` | default | `split-line` content (not the anchors) shifted by `(H/2 + 5, H/2 + 3)`: same geometry, off the root's dyadic lines |
| `mixed-size` | default | `r = U(0,999)`; `r < 5`: `w, h = U(S/8, S/4)`; `r < 20`: `w, h = U(128, max(128, min(512, S)))`; else as uniform; clamp to `S`; `x = U(0,S-w)`, `y = U(0,S-h)` |
| `long-thin` | default | World `2S`; `t = U(2,8)`, `l = U(32, min(256, 2S))`, horizontal if `U(0,1) == 0` (`w = l, h = t`) else `w = t, h = l`; `x`, `y` uniform in the world |
| `tiles` | default | `c = ceil(sqrt(n))` columns; entry `i` at `(16 (i mod c), 16 (i div c))`, size 16; edge contacts only, zero pairs |
| `tiles-touch` | touch | `tiles` with touch-inclusive overlap |
| `points` | default | `kind = U(0,3)`, `x = 8 U(0, S/8)`, `y = 8 U(0, S/8)`; kinds 0, 1: point; 2: horizontal segment to `min(x + U(8,64), S)`; 3: vertical segment |
| `pile` | default | `{512 - U(1,400), 512 - U(1,400), 512 + U(1,400), 512 + U(1,400)}` in draw order left, right, bottom, top; all pairs overlap; capped at 4096 |
| `disjoint` | default | Lattice cell 32, `c = ceil(sqrt(n))`; `x = 32 (i mod c) + U(0,15)`, `y = 32 (i div c) + U(0,15)`, `w = U(1,16)`, `h = U(1,16)`; zero pairs |
| `outliers` | default | As `uniform`, then `U(0,99) == 0` moves the origin: `x = ±U(2^28, 2^29)`, `y = ±U(2^28, 2^29)` (sign draw `U(0,1)`, then magnitude, for x then y) |
| `small-world-8` (opt-in) | default | `w = h = 8`, `x = U(0,600-w)`, `y = U(0,400-h)`; capped at 4096 |
| `small-world-mixed` (opt-in) | default | `w = U(4,64)`, `h = U(4,64)`, `x = U(0,600-w)`, `y = U(0,400-h)`; capped at 4096 |

Default sizes: 0, 1, 2, 4, 8, 16, 32, 64 (batched; `uniform`, `clustered`) and
10², 10³, 10⁴, 10⁵. Default types: `int32`, `float`; all four in `--quick`.

## Persistent model

Entries are inserted once (`init_ns`, untimed in the step estimate), then each
step applies one workload update and enumerates pairs. Per step, the update
phase (`prepare_median_ns`) and the pair phase (`pairs_median_ns`) are timed;
the step estimate is the mean over rounds of the per-pass median step time.

- **Scenes and sizes:** `uniform`, `clustered`, `split-line`, `mixed-size` at
  10³, 10⁴, 10⁵. World: `[0, S]²`, or the root `[0, W]²` for `split-line`.
- **Pass:** fresh engine; insert all entries; 64 steps (`--steps`). Every pass
  replays the identical update sequence. Pass totals (candidates, pairs,
  checksum `Σ mix64(step checksum + step)`) must match across rounds and engines.
- **Time cap:** the first pass of an engine stops after `max-time / rounds`
  (0.8 s); later passes replay exactly its number of steps. Such results are
  `time-capped`: their per-step estimates cover a prefix of the sequence and
  are ranked like complete ones.
- **Cross-check:** per step of the first pass (pair count and checksum), over
  the steps every timed engine completed. A deliberately broken moved-set
  deduplication is reported as `MISMATCH`.
- **Pair modes:** `full` (all pairs) and `moved` (pairs with at least one
  endpoint in the changed set S, each once; S = moved or inserted ids). `moved`
  runs for every workload below 100%.
- **Movers:** the first k ids of a Fisher–Yates permutation seeded with
  `scene seed ^ fnv1a64(workload) ^ 0x6d6f766572`; then per mover
  `vx = U(-4,4)`, `vy = U(-4,4)`. k = round(fraction · n), at least 1.
- **Per-step draws:** splitmix64 seeded with `scene seed ^ fnv1a64(workload) ^ 0x73746570`.

| Workload | Per step |
|---|---|
| `static` | no updates (moved mode: S = ∅) |
| `small-single` | one mover: box += (vx, vy); at the world edge it stops there and the velocity component reverses |
| `small-1%`, `small-10%`, `small-100%` | as `small-single` for k movers |
| `teleport-1%`, `teleport-10%` | k movers jump to `x = U(minX, maxX - w)`, `y = U(minY, maxY - h)` |
| `noop-100%` | every entry is updated with its unchanged box |
| `churn-1%`, `churn-10%` | k removals of `live[U(0, live-1)]` (swap-remove), then k inserts with new ids, size copied from `initial[U(0, n-1)]`, uniform position |

| Engine | Updates | Moved-set query |
|---|---|---|
| `brute` | store | S × live |
| `b2tree` | Box2D `b2DynamicTree_MoveProxy` with the exact box on every update | `OverlapAABB` per changed entry; exact refinement |
| `b2tree-fat` | tree holds boxes enlarged by 4 units (one step of maximum motion); `MoveProxy` only when the exact box leaves its enlarged box (Box2D's broad-phase scheme through the public 3.2 API, which has no `EnlargeProxy`) | as `b2tree` |
| `sweep` | changed entries removed from the sorted order, sorted, merged back: O(n + k log k) per step | scan from minX − (largest width) |
| `grid` | fixed world grid, per-cell vectors; unchanged cell range only stores the box | cells of the changed entry, reporting-cell rule |
| `grid-rebuild` | snapshot grid rebuilt after every step's updates | full mode only |

Box2D engines run for `int32` and `float` (exact float conversion); its
allocations go through `b2SetAllocator` into the counting allocator. Box2D
queries report touching boxes; every candidate is refined with the exact
QT-FR-04 test and counted. `node_visits` is Box2D's internal plus leaf visits.

## Baselines

| Engine | Definition | Applicable to |
|---|---|---|
| `brute` | All n(n-1)/2 pairs | 0–64 entries; `pile` |
| `sweep` | Sort by (minX, index); for each entry, test following entries while their minX ≤ its maxX | Everything; reference for `outliers` |
| `grid` | Uniform grid over the data bounds. Cell side: max(mean of (w + h), sqrt(area / 4n), spanX / 4n, spanY / 4n). Entries are referenced by every covered cell; a pair is evaluated only in the cell of (max minX, max minY) | Bounded worlds; not `outliers` |
| Persistent engines | See the persistent model | Persistent motion |

All engines implement the QT-FR-04 default and touch-inclusive overlap rules
independently of the library.

## Step 3: feasibility prototypes (`bench/bench-prototypes.h`)

Bench-only snapshot engines, cross-checked against the baselines in every run
(including `--quick` for all types). Root = data bounds; cells half-open,
closed at the root's far edges; split at `std::midpoint`; a leaf splits when
more than 8 entries can move down (`--leaf-capacity`; 8 beat 16, 32, 64);
depth ≤ 24 (`--max-depth`).

| Engine | Straddling strategy | Pairs |
|---|---|---|
| `qt-classic` | descend only into a child containing the box; straddlers stay (current library rule) | node entries vs ancestors overlapping the cell |
| `qt-loose` | one reference: deepest node whose cell holds the center and whose child size fits the extent; subtree bounds per node | dual traversal of node pairs with overlapping bounds |
| `qt-loose-sort` | `qt-loose` bulk-loaded: size level + Morton code of the center (16 levels), one radix sort, nodes cut by binary search | as `qt-loose` |
| `qt-refs` | every reached child while the extent fits the child size: ≤ 4 references per entry, independent of area | as `qt-classic`; a pair is evaluated only in the deeper reference cell containing (max minX, max minY): unique without a pair set |
| `qt-refs-ordered`, `grid-ordered` | QT-FR-24 reference algorithm on `qt-refs` / grid: rank order, per-entry partner query, row sort, emit; order checked on emission | |

Results (int32, ratio to the best baseline with 95% interval; `c` =
candidates per reported pair):

| Scene | n | `qt-classic` | `qt-loose` | `qt-loose-sort` | `qt-refs` |
|---|---|---|---|---|---|
| `uniform` | 10³ | 4.59 c20 | 3.73 c10 | 3.58 c11 | 4.58 c5 |
| `uniform` | 10⁵ | 3.24 c33 | 2.30 c11 | **1.60** c11 | 2.62 c5 |
| `clustered` | 10³ | 3.83 c9 | 3.15 c4 | 2.17 c4 | 4.66 c3 |
| `clustered` | 10⁵ | 1.73 c11 | 1.43 c6 | **1.17** c5 | 1.73 c4 |
| `split-line` | 10³ | 6.63 c43 | 3.83 c9 | 3.25 c9 | 5.88 c5 |
| `split-line` | 10⁴ | 6.48 c208 | 2.43 c8 | **1.73** c8 | 3.02 c5 |
| `split-line` | 10⁵ | **31.6 c1115** | 2.19 c7 | **1.36** c7 | 2.91 c4 |
| `split-line-translated` | 10⁵ | 20.9 c642 | 2.33 c6 | 1.45 c6 | 3.02 c4 |
| `mixed-size` | 10⁴ | 1.71 c7 | 1.33 c2 | **1.11** c2 | 1.58 c2 |
| `mixed-size` | 10⁵ | 0.95 c3 | 0.79 c1 | **0.66** c1 | 0.98 c1 |
| `outliers` | 10⁵ | 0.70 | 0.64 | capped | **0.61** |

Findings:

- **Straddling (QT-FR-17):** the classic rule piles split-line entries near
  the root: candidates grow ×41 and ×78 per tenfold n (limit 3: 15×) and the
  step is 32× the grid at 10⁵. Loose cells and bounded references both remove
  the pile-up: candidates per tenfold n ×7.4–11.6 (loose) and ×9.5–10.8 (refs),
  and the translated scene costs 0.92–0.98× the original (limit 4: within 1.5×).
- **Bounded references:** ≤ 4 references per entry held in every scene
  (`max_refs`), and the reference-point rule reports each pair once without a
  pair set; it needs the fewest candidates (c 4–5) but 2.2 references per entry
  make its top-down build the slowest.
- **Size disparity (QT-FR-18):** large boxes stay at coarse levels without
  blocking subdivision; on `mixed-size` 10⁵ every tree matches or beats the grid
  (0.66–0.98×), whose large boxes span many cells.
- **Build cost decides snapshot performance:** pair phases are within
  1.0–1.7× of the grid; recursive top-down builds cost 8–13 ms at 10⁵ versus the
  grid's 2.6 ms. The sorted bulk load halves the loose build (8.4 → 4.1 ms).
- **Achievable ratios (best prototype):** 10⁴–10⁵: 0.66–1.8× (inside 2×);
  10³: 2.2–3.6×; 10²: 1.0–2.6× (`qt-classic`) and 1.7–4.1× (others), on
  representative scenes; 1–4 entries: 3–6× brute force (33–70 ns absolute),
  28–77× for the sorted build.
- **Ordered (QT-FR-24):** the reference algorithm costs 2.6–3.5× its own
  unordered step (`qt-refs`; 6.8× on `mixed-size` 10⁵, 8.2× on `pile`) and
  3.3–5.2× on the grid (7.7–7.9× at 10³ `clustered`/`mixed-size`, 10.7× `pile`); the best ordered engine is 3.3–7.9× the best unordered
  baseline. Cost drivers: each pair is discovered from both endpoints (2×
  candidates), per-entry query traversal, and row sorts.
- **Prototype limits:** `qt-loose-sort` uses fixed 16-level quantization and
  degenerates on `outliers` (one cell holds the main content; candidate-capped
  at 10⁵) and has a fixed radix cost on tiny inputs. Both need adaptive depth
  and a small-set path in a real implementation.

Recommendation for the strategy choice (before the API freeze):

1. Reject the classic containment rule.
2. Primary candidate: loose cells with one reference per entry and a bulk
   load with adaptive depth; it was fastest and keeps one reference per entry
   for persistent updates. Keep bounded references as the alternative where
   candidate counts matter more than build/update cost.
3. A direct small-set path for small n (QT-NFR-11).
4. Before freezing: a persistent-model prototype (incremental update and
   moved-set queries) of the chosen strategy against `b2tree-fat` and grid
   (step 3b below).

## Step 3b: persistent loose quadtree (`bench/bench-persistent-loose.h`)

Engine `qt-loose` in the persistent model:

- **Placement:** fixed world root; one reference per entry at the deepest node
  whose cell holds its center and whose size fits its extent. An update that
  keeps the node only stores the box; otherwise the entry is unlinked and
  re-placed from the root.
- **Adaptivity:** a leaf splits when more than 8 entries fit a child; a node
  merges its subtree at ≤ 4 entries (hysteresis); child blocks of four are
  recycled through a free list.
- **Bounds:** tight bounds of own entries and of the subtree. Updates mark the
  node and its ancestors dirty; `finishUpdates()` refits dirty subtrees
  bottom-up, O(changed paths). Invariant: a dirty node has dirty ancestors;
  empty subtrees are cleaned completely. Static loose bounds (cell enlarged by
  half its size) were measured first and rejected: sibling bounds always
  overlap, giving 53 instead of 10 candidates per pair at 10³ and full
  enumeration at 6× the grid.
- **Pairs:** full mode by dual traversal; moved-set by one pruned query per
  changed entry.
- **Correctness:** cross-checked against brute force in every workload. The
  64-step check found a dirty-flag bug (an empty subtree kept a dirty
  descendant under a clean parent; a later insert left stale bounds; one pair
  missed at step 59), now fixed. CTest `bench-persistent-check` (64-step
  passes, 200 and 800 entries, every engine against brute force) reports the
  reintroduced bug as 8 `MISMATCH` rows.

Results (float, all four scenes, 10³–10⁵, all workloads; `/b2f` = ratio to
`b2tree-fat`, best = fastest of grid, `grid-rebuild`, Box2D):

| Mode | n | `qt-loose` / `b2tree-fat` | `qt-loose` / best |
|---|---|---|---|
| full | 10³ | 0.21–0.85 | 1.05–3.85 (grid) |
| full | 10⁴ | 0.19–0.48 | 1.04–2.37 |
| full | 10⁵ | 0.15–0.31 | 0.81–2.10 |
| moved | 10³ | 0.52–0.81 (single mover 0.77–1.99) | 1.19–2.68 (grid) |
| moved | 10⁴ | 0.51–0.72 (single mover 0.64–2.67) | 1.45–4.43 |
| moved | 10⁵ | 0.54–0.89 | 1.08–4.35 |

- **Moved-set scaling:** |S| = 0 costs 16–20 ns at every n; one mover costs
  0.15–0.61 µs and grows ×1.7 (`uniform`) to ×3.1 (`mixed-size`) from 10³ to 10⁵.
- **Updates at 10⁵ `uniform`:** 100% small motion 7.4 ms per step (74 ns per
  moved entry including refit; `b2tree-fat` 17.7 ms); no-op 100% 2.8 ms.
- **Memory:** initial insertion 14.8 ms at 10⁵ (Box2D 60 ms, grid 7.4 ms);
  retained 8.6 MB (Box2D 13–15 MB, grid 6.4 MB).
- **Allocations:** per-node `std::vector` growth costs median 10 and up to
  3500 allocations per step (teleports, 100% motion); the library needs pooled
  node storage (QT-NFR-03). The persistent grid has the same issue (up to 8600).

Against the provisional limits (before step 4 froze them):

- Provisional limit 7 (persistent full ≤ 2× best at 10⁴–10⁵): `clustered` and `mixed-size`
  pass (0.81–1.85); `uniform` and `split-line` are 1.42–2.37×, at the limit or
  slightly above.
- Provisional limit 8: |S| = 0 ≤ 1 µs passes; single mover 10⁵ ≤ 4× its 10³ value passes
  (×1.7–3.1); `small-1%`/`teleport-1%` moved ≤ 2× the best engine passes in
  7 of 24 rows against the grid (1.08–3.87×) and in all against Box2D
  (0.51–0.81×).

Conclusion: loose cells are a sound persistent strategy. Full enumeration is
2–7× faster than Box2D's dynamic tree at 10⁴–10⁵, moved-set queries 1.1–2×
faster, and 100% small-motion updates 2.4× cheaper. The remaining gap is to the uniform grid, which is
specialised for bounded worlds of similar sizes (it allocates heavily and
fails on `outliers`). Candidate improvements for the implementation: pooled
node storage with boxes stored next to node entries (no indirect loads), and
moved-set queries starting from the entry's own node.

## Entry-store rank set (`quadtree-rank-bench`)

Duplicate-rank detection of `detail::EntryStore` (QT-FR-24). Ranks are client
data; the hash is the public `mix64` bijection, so ranks can be chosen to share
one home bucket (`unmix64(k << 32)` gives distinct ranks whose hashes share 32
low bits).

Design (`quadtree-index.h`): linear probing with every rank within 128 buckets
of its home. Hash lookups and insertion probes are O(128); backward-shift
removal is O(n) worst-case and O(128) amortized. The scan bound follows the
moving hole, not the total number of buckets visited. An insertion beyond
the displacement bound converts the set to a splay tree in the same cells,
built from live metadata; splay operations are amortized O(log n) for any
sequence. A rehash retries hash mode, so ordinary ranks leave the tree again.
`clear()` is O(1) (epoch per bucket); the buckets are reset only when the
epoch wraps, every 2³²−1 clears.

`quadtree-rank-bench` (GCC 16 `-O3`, median of 5; ns per operation; probes =
hash probes plus splay steps per insert):

| Ranks | n | Insert | Duplicate check | Remove | Probes/insert | Mode |
|---|---|---|---|---|---|---|
| sequential | 64000 | 46 | 10 | 20 | 3.8 | hash |
| random | 64000 | 46 | 10 | 20 | 3.8 | hash |
| colliding | 1000 | 146 | 61 | 62 | 64 | tree (3 conversions) |
| colliding | 8000 | 211 | 86 | 90 | 38 | tree (6 conversions) |
| colliding | 64000 | 312 | 124 | 133 | 40 | tree (9 conversions) |
| colliding, hash only | 8000 | 6475 | 2033 | 7508 | 9399 | quadratic (52 ms for all inserts) |

- `clear()` with 2²⁰ reserved ranks and one live ranked entry: 10.7 ns
  (previously 2,097,152 bucket resets per clear).
- On ordinary ranks the fallback costs nothing: an isolated A/B with probe
  limits 128, 2³⁰ and unbounded measured 33–38 ns (GCC) and 54–61 ns (Clang)
  per insert for all three. Within one binary, per-instantiation code layout
  shifts such comparisons by 10–30%, so the benchmark compares policies only on
  colliding ranks.
- Tests (`entry-store-test`): colliding ranks stay within 8·n·log₂n probes,
  `clear()` performs no cell resets with a 2²⁰ reservation (hash and tree
  mode), the epoch wrap clears stale buckets, and a randomized oracle with a
  two-bucket probe limit exercises conversions, tree operations, rehashes,
  clears and moves. Mutations of the tree clear, shift bound, epoch reset,
  rebuild conversion, splay rotation, erase join, find bound and clear cost
  are all detected.

## Frozen acceptance limits

Frozen on 2026-11-18 (step 4), calibrated with the step 3 prototypes, before the
scalability phase. Hard gates block acceptance; soft gates are published
targets. Timing never waives correctness, allocation or memory gates.

| Gate | Kind | Applies to | Limit |
|---|---|---|---|
| G0 | hard | every engine and row | no `MISMATCH`, `NONDETERMINISTIC` or `UNORDERED` |
| G1 | hard | snapshot, `uniform`/`clustered`, 0–64 entries (batched) | ≤ 2× brute force |
| G2 | hard | snapshot, representative scenes¹, 10⁴–10⁵ | ≤ 2× the best baseline (brute, sweep, grid) |
| G2 | hard | snapshot, representative scenes¹, 10²–10³ | ≤ 4× the best baseline |
| G3 | hard | snapshot, `split-line`, `split-line-translated`, `mixed-size` | candidates / (pairs + n) grows ≤ ×1.5 per tenfold n |
| G4 | hard | snapshot, `split-line-translated` vs `split-line` | candidates and step within ×/÷ 1.5 |
| G5 | — | pathological scenes (`pile`, `outliers`, `long-thin`, `tiles`, `tiles-touch`, `points`, `disjoint`) | measured and published, no ratio gate |
| G6 | hard | candidate engines, every row | 0 allocations per warm step (after reservation) |
| G7 | hard | persistent full enumeration, every scene, size and workload | ≤ 2× the dynamic tree (best of `b2tree`, `b2tree-fat`) |
| G7 | soft | same | ≤ 4× the grid (best of `grid`, `grid-rebuild`) |
| G8a | hard | persistent moved-set, `static` (\|S\| = 0) | ≤ 1 µs per step at every n |
| G8b | hard | persistent moved-set, `small-single` | step at 10⁵ ≤ 4× its 10³ value |
| G8c | hard | persistent moved-set, `small-1%` and `teleport-1%` | ≤ 2× the dynamic tree |
| G8c | soft | same | ≤ 4× the grid |
| G9 | hard | snapshot ordered (QT-FR-24), representative scenes¹, n ≥ 10³ | ≤ 4× the same engine's unordered step |

¹ `uniform`, `clustered`, `split-line`, `split-line-translated`, `mixed-size`.

**Accepted release exception (G1, 2026-11-21).** The G1 limit is unchanged, and rows of 0–4 entries still report `FAIL`. By owner decision, release sign-off accepts them when their upper bound is ≤ 3.5× brute. The remaining gap is fixed per-rebuild work the contract requires. See [the decision record](../docs/plan/02-g1-small-snapshots-result.md).

**Accepted release exception (G2, `int32`, 2026-11-21).** The G2 limits are unchanged. By owner decision, `int32` `split-line-translated` n=10² and `uniform` n=10³ do not block release while their upper bound is ≤ 4.5× the best baseline. See [the 08 brief](../docs/plan/08-final-acceptance.md).

**Amendment before first use (G3).** The provisional form limited raw
candidates to 15× per tenfold n. The published data showed that the fixtures'
output itself grows ×8.6–44 per tenfold n (`mixed-size` large boxes scale with
the world; `split-line` lines densify), so raw counts track output, not wasted
work. The frozen form divides by (pairs + n); for linear output it equals the
old limit (15/10). It still separates the strategies: the classic rule grows
×3.3–6.5 per tenfold n on `split-line`, the loose and bounded-reference
prototypes ×0.63–1.24.

**Verdicts.** Ratios use the run's estimates with the log-scale 95% half-width
√(rel_a² + rel_b²); absolute limits use estimate ± error; candidate counts and
allocations are exact. Pass when the interval lies inside the limit, fail when
it lies entirely outside, otherwise inconclusive (rerun with more `--rounds` or
processes). The reference is the fastest timed reference engine in the same
scene, type, size, workload and mode. Time-capped results count as timed.

**Procedure.**

```sh
quadtree-bench --gate ...                       # run, then evaluate
quadtree-bench --gate-files=a.csv,b.csv         # evaluate published results
quadtree-bench --gate-snapshot=E --gate-persistent=E --gate-ordered=E:BASE ...
```

Candidate engines default to the library (`index` for both models,
`index-ordered` against `index`). Historical prototype results require explicit
`--gate-snapshot=qt-loose-sort --gate-persistent=qt-loose --gate-ordered=qt-refs-ordered:qt-refs`.
Exit code: 0 all hard gates pass, 1 cross-check failure, 3 a hard
gate fails, 4 no hard failure but some hard gate inconclusive. The evaluator is
self-tested on `bench/gate-fixture.csv` (CTest `bench-gate-check`).

**Why the moved-set and full-enumeration references are the dynamic tree.**
The grid relies on a fixed world known in advance and a cell size tuned to each
scene's mean extent, allocates up to 8600 times per step, and collapses to n²
on far outliers; a general index cannot assume any of that. The dynamic tree
shares the library's properties (unbounded world, no tuning, zero steady-state
allocations) and is the plan's required persistent-motion baseline. The grid
ratio stays published as a soft target, so losses to it remain visible.

## Published results

`bench/results/2026-11-18-ryzen9-5900hs/`: snapshot (int32, float) and
persistent (int32, float) runs over all scenes, workloads and engines, raw
samples, gate verdicts and calibration; see its README. With the prototype
candidates the hard gates fail only where the implementation work is already
known: the small-set path (G1), pooled node storage (G6), and ordered mode on
`mixed-size` 10⁵ (G9). G2–G4, G7 and G8 pass against the frozen limits.

## Error calibration (this hardware)

AMD Ryzen 9 5900HS, amd-pstate-epp, energy preference and platform profile
`performance`, kernel clock source HPET, GCC 16.2.1 `-O3`, int32, full default
snapshot run (174 rows).

- **Performance mode** versus the earlier default-mode run: median speed ratio
  1.000 (0.87–1.07 per row), median p95/median 1.027 versus 1.025. No
  measurable change.
- **Process-level effects:** without layout shifts, rows moved by up to 3× between
  identical processes while staying stable within each (e.g. `clustered` n=1
  brute 22 ns versus 7 ns; `outliers` sweep 62–68 µs). Cause: per-process stack
  and heap placement. Pinning to the preferred core (`taskset -c 2`) did not
  help (62% coverage).
- **Coverage** = share of rows where two identical processes agree within their
  combined 95% error (target ≥ 95%):

| Method | Timer | Absolute | Paired `vs_best` |
|---|---|---|---|
| Rounds only | HPET | 90.2% | 88.6% |
| Rounds + layout shifts | HPET | 78.7% | 93.0% |
| Rounds + layout shifts + 2% process term | HPET | 96.0% | 96.4% |
| Validation on two further processes (`--diff`) | HPET | 97.7% `same` | |
| Rounds + layout shifts + 1% process term | TSC | 94.3% | 93.7% |
| Rounds + layout shifts + 2% process term (default) | TSC | 97.7% | 97.3% |

- **Persistent model (TSC timer, identical invocations):** a 2% term gave 93.1%
  coverage, so the persistent term is 3%. Three identical processes then agree
  on 95.7%, 95.7% and 98.5% of 656 rows. A run with a different engine set and
  sizes agrees on only 92.4%: compare runs made with identical arguments.
- **Energy preference:** calibrated under `performance`; the harness writes a
  `WARNING` metadata line otherwise (one snapshot run under `power` was 1.9%
  slower at the median and up to 32% on single rows).
- **Resolution:** with the TSC timer the round error alone is 0.76% (median);
  the reported ±error is about 2% per row, so differences of about 3–5% are
  detectable. Re-validate p after compiler, CPU or OS changes with two `--csv`
  runs and `--diff` (expect ≥ 95% `same`).

Best snapshot baseline estimates (int32, TSC timer):

| Scene | 10² | 10³ | 10⁴ | 10⁵ |
|---|---|---|---|---|
| `uniform` | 1.77 µs grid | 19 µs grid | 554 µs grid | 6.4 ms grid |
| `clustered` | 5.42 µs sweep | 37 µs grid | 1.3 ms grid | 14.5 ms grid |
| `split-line` | 1.56 µs sweep | 22 µs grid | 573 µs grid | 7.2 ms grid |
| `split-line-translated` | 1.49 µs sweep | 21 µs grid | 580 µs grid | 7.0 ms grid |
| `mixed-size` | 2.05 µs sweep | 33 µs grid | 1.3 ms grid | 34.5 ms grid |
| `outliers` | 2.04 µs sweep | 65 µs sweep | 1.9 ms sweep | 40.3 ms sweep |

Small scenes (`uniform`, per scene): brute force 4 / 6 / 11 / 19 / 150 ns at
0 / 1 / 2 / 4 / 8 entries; sweep 0.42 / 1.14 / 2.89 µs at 16 / 32 / 64.
Observed baseline limits: the grid degrades to n² candidates in one cell on
`outliers` (capped at 10⁵); sweep candidates grow as about n^1.5 on 2D-uniform
data; brute force is the best engine only up to about 8 entries.

## Persistent results (development machine)

`--model=persistent --types=float`, all four scenes, 10³–10⁵, 26 min,
902 timed rows, no `MISMATCH` or `NONDETERMINISTIC`. At 10⁵ most engines are
time-capped (first pass over 0.8 s); their estimates cover a prefix of the
step sequence. Brute force is candidate-capped at 10⁵ in full mode.


Full enumeration at 10⁵, step time: best engine, then `b2tree` / `b2tree-fat`:

| Scene | static | small-1% | small-100% | teleport-10% | noop-100% | churn-10% |
|---|---|---|---|---|---|---|
| `uniform` | 5.1 ms grid; 35.9 ms / 39.8 ms | 5.0 ms grid; 38.4 ms / 40.7 ms | 8.2 ms grid-rebuild; 114.0 ms / 86.6 ms | 5.6 ms grid; 40.5 ms / 45.3 ms | 6.0 ms grid; 107.9 ms / 41.1 ms | 6.6 ms grid; 44.1 ms / 47.5 ms |
| `clustered` | 13.1 ms grid; 51.8 ms / 62.7 ms | 13.2 ms grid; 53.6 ms / 65.5 ms | 9.3 ms grid-rebuild; 121.8 ms / 92.9 ms | 12.6 ms grid; 62.3 ms / 72.7 ms | 14.0 ms grid; 117.8 ms / 64.3 ms | 6.8 ms grid-rebuild; 50.1 ms / 54.1 ms |
| `split-line` | 5.3 ms grid; 31.8 ms / 34.9 ms | 5.3 ms grid; 33.2 ms / 33.8 ms | 7.3 ms grid-rebuild; 108.1 ms / 71.1 ms | 6.5 ms grid; 39.4 ms / 41.3 ms | 6.5 ms grid; 93.8 ms / 35.2 ms | 4.9 ms grid-rebuild; 36.9 ms / 37.5 ms |
| `mixed-size` | 31.2 ms grid; 162.1 ms / 169.8 ms | 31.6 ms grid; 164.5 ms / 170.4 ms | 35.7 ms grid-rebuild; 246.7 ms / 237.5 ms | 35.3 ms grid; 183.3 ms / 181.2 ms | 32.9 ms grid; 238.8 ms / 176.5 ms | 34.1 ms grid; 176.5 ms / 183.1 ms |

Moved-set enumeration at 10⁵, step time: best engine, then `b2tree` / `b2tree-fat`:

| Scene | static (S=∅) | small-single | small-1% | small-10% | teleport-1% | churn-1% |
|---|---|---|---|---|---|---|
| `uniform` | 0.02 µs brute; 0.02 µs / 0.02 µs | 0.08 µs grid; 0.49 µs / 0.37 µs | 202 µs grid; 989 µs / 795 µs | 2.0 ms grid; 10.6 ms / 8.7 ms | 374 µs grid; 1.2 ms / 1.5 ms | 268 µs grid; 1.3 ms / 1.3 ms |
| `clustered` | 0.02 µs brute; 0.02 µs / 0.02 µs | 0.05 µs grid; 0.36 µs / 0.39 µs | 228 µs grid; 894 µs / 734 µs | 2.7 ms grid; 11.8 ms / 9.5 ms | 206 µs grid; 911 µs / 917 µs | 284 µs grid; 1.2 ms / 1.3 ms |
| `split-line` | 0.02 µs brute; 0.02 µs / 0.02 µs | 0.08 µs grid; 0.52 µs / 0.49 µs | 124 µs grid; 874 µs / 623 µs | 1.4 ms grid; 10.0 ms / 7.1 ms | 180 µs grid; 717 µs / 735 µs | 239 µs grid; 1.2 ms / 1.1 ms |
| `mixed-size` | 0.02 µs brute; 0.02 µs / 0.02 µs | 0.12 µs grid; 0.73 µs / 0.68 µs | 683 µs grid; 2.2 ms / 2.0 ms | 11.8 ms grid; 30.6 ms / 26.8 ms | 1.9 ms grid; 3.2 ms / 3.3 ms | 1.2 ms grid; 3.2 ms / 3.3 ms |

Moved-set scaling, `uniform`, step time at 10³ / 10⁴ / 10⁵:

| Workload | b2tree | b2tree-fat | sweep | grid |
|---|---|---|---|---|
| `static` | 0.02 µs / 0.02 µs / 0.02 µs | 0.02 µs / 0.02 µs / 0.02 µs | 0.02 µs / 0.02 µs / 0.02 µs | 0.02 µs / 0.02 µs / 0.02 µs |
| `small-single` | 0.32 µs / 0.39 µs / 0.49 µs | 0.07 µs / 0.37 µs / 0.37 µs | 3.42 µs / 39 µs / 342 µs | 0.05 µs / 0.09 µs / 0.08 µs |
| `small-1%` | 3.65 µs / 52 µs / 989 µs | 2.80 µs / 39 µs / 795 µs | 6.95 µs / 129 µs / 3.2 ms | 0.92 µs / 10 µs / 202 µs |

Observations:

- **Full enumeration:** the incremental grid is best (5 ms at 10⁵ `uniform`),
  `grid-rebuild` when everything moves. Box2D trees enumerate all pairs by one
  query per entry and are 4–7× slower; their strength is moved-set queries.
- **Moved-set:** an empty changed set costs 0.02 µs (the harness floor) for
  every engine at every n: no hidden O(n) pass. One mover costs 0.05–0.12 µs
  (grid) and 0.36–0.73 µs (Box2D) at 10⁵, growing slowly with n. The sweep merge
  is O(n) per step (×100 from 10³ to 10⁵ for one mover). At 1% movement, grid
  and Box2D time grows with |S| (×220–280 for ×100 entries, including cache
  effects).
- **Enlarged boxes:** `b2tree-fat` beats `b2tree` by 7–29% on 1–10% small
  motion (moved-set) and
  avoids `MoveProxy` on no-op updates (41 vs 108 ms full, `uniform` 10⁵); it
  loses on teleports and full enumeration (more candidates).
- **Allocations and memory:** Box2D trees 0 per warm step (capacity preset),
  13–15 MB at 10⁵. The persistent grid allocates while per-cell vectors grow
  (median 13, up to 8600 per step on `mixed-size`); sweep and `grid-rebuild`
  below 3. Initial insertion at 10⁵: `grid-rebuild` 3.2 ms, grid 8.4 ms, sweep
  9.5 ms, Box2D 66–67 ms.

## Open items

- Close the library G1/G2 failures and remaining inconclusive gates, then run
  the complete type/scene/workload matrix (development results cover a subset).
- Re-validate the 2% process term after compiler, CPU or OS changes (two `--csv`
  runs and `--diff`, expect ≥ 95% `same`).
- The `grid` persistent engine allocates when cell vectors grow (per-cell
  `std::vector`); a pooled variant would remove that baseline cost.
