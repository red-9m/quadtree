# Current library snapshot baseline — 2026-11-18

Task 01, measurement only. This reruns the complete historical `int32` snapshot slice on the current working-tree headers, after the mutation and ordered moved-set fixes. This is **not release acceptance**. There were no library, harness, or gate-limit changes, and no persistent runs.

| Gate | Rows | Initial (pass / fail / inconclusive) | Nine-round repeat |
|---|---:|---:|---:|
| G1 | 16 | 6 / 10 / 0 | 6 / 10 / 0 |
| G2 | 15 | 5 / 7 / 3 | 8 / 6 / 1 |

Both runs covered every historical G1/G2 row. Each snapshot process exited `0`: no `MISMATCH`, `NONDETERMINISTIC`, or `UNORDERED`. All 68 library rows per run completed with zero warm allocations. Each run had eight capped baseline/prototype rows; none prevented a G1/G2 verdict. Both frozen gate evaluations exited `3`; the measured G3/G4/G6/G9 rows passed. The [gate evaluator fixture check](gate-check.log) passed.

- Initial: [CSV](snapshot.csv), [raw samples](snapshot-raw.csv.gz), [verdicts](gates.txt).
- Repeat: [CSV](repeat/snapshot.csv), [raw samples](repeat/snapshot-raw.csv.gz), [verdicts](repeat/gates.txt).

## 02 · Small snapshots — initial diagnosis

Both scenes fail at `n = 0, 1, 2, 4, 8` and pass at `16, 32, 64` in both runs. In the repeat, `index` takes about 19 ns at `n=0` and 61 ns at `n=1`, versus 5.2 ns and 6.5 ns for brute force. Neither engine tests any pairs at those sizes. At `n=2,4,8`, the two engines test the same number of candidates and match pair counts and checksums. The full-step ratios at `n=8` are 2.144× (uniform) and 2.288× (clustered).

The gate harness only splits phases for unbatched rows. A [separate batched profiling driver](g1-profile/profile-g1.cpp) used the same fixtures and unmodified `LibraryIndex` adapter, checked exact pair identities against brute, and sampled warm user-space cycles. The [phase summary](g1-profile/summary.csv) shows that replacement and its support functions account for about 78% of sampled cycles at `n=1`, 69% at `n=2`, and 51–54% at `n=4`. The direct query path accounts for about 64% at `n=0` (its early-return path, not geometry tests) and 52% at `n=8`. Adapter build accounts for about 3–5% at `n=1–8`.

Next: examine the warm replacement path at `n=1–4` and the empty dispatch separately. Preserve the strong build/failure guarantees and empty-build handle invalidation. Treat the profile as attribution, not as an absolute phase timing or gate result: source-line attribution is approximate for inlined code and cycle samples can skid. Use before/after full-step gate measurements to validate any change.

## 03 · Representative snapshots — initial diagnosis

Use the higher-round repeat when routing rows. The only hard failures are `uniform`, `split-line`, and `split-line-translated` at `10,000` and `100,000`. All `1,000` rows pass in the repeat, including `split-line` and `uniform` (initially inconclusive) and `split-line-translated` (initially failed). `clustered` at `100,000` remains **inconclusive**, at 1.957× [1.896–2.020] against a 2× limit. It is not a pass. The nine-round repeat used the same fixtures and frozen error terms; more rounds alone do not remove the harness's process error floor.

Phase medians from the repeat, in ms per snapshot. Grid was the fastest baseline on each of these rows. Counts are for `index`.

| Scene | Entries | Index build / pairs | Grid build / pairs | Candidates | Node visits |
|---|---:|---:|---:|---:|---:|
| uniform | 10,000 | 0.931 / 0.649 | 0.231 / 0.352 | 51,231 | 46,998 |
| uniform | 100,000 | 12.742 / 7.120 | 2.560 / 3.821 | 788,738 | 426,273 |
| split-line | 10,000 | 1.023 / 0.711 | 0.253 / 0.336 | 58,619 | 49,868 |
| split-line | 100,000 | 14.341 / 7.357 | 2.641 / 4.253 | 681,627 | 454,752 |
| split-line-translated | 10,000 | 1.024 / 0.682 | 0.252 / 0.336 | 55,720 | 47,889 |
| split-line-translated | 100,000 | 14.768 / 6.880 | 2.577 / 3.900 | 625,694 | 434,151 |
| clustered (inconclusive) | 100,000 | 13.016 / 14.609 | 2.552 / 11.635 | 2,082,027 | 266,973 |

Build accounts for about 59–68% of the two phase medians on the six hard failures, and is roughly 4.0–5.7× grid's build. Pair traversal is also slower. Profile `buildRange` first to distinguish input preparation, key generation/sort, and topology planning/commit before choosing an optimization; keep traversal counts and time in the before/after comparison. Unmeasured types and sizes remain for 08.

## Environment and reproduction

[Environment](environment.txt): Ryzen 9 5900HS, GCC 16.2.1, CMake 4.4.2, Ninja, Linux 7.3.0-1-custom, `performance` energy preference, invariant-TSC benchmark timer, HPET kernel clock source. Release flags match the historical benchmark (`-O3 -DNDEBUG` and the same warnings; no native/LTO flags). Box2D used a clean local checkout of the pinned `v3.2.0` commit (`178495ba35695aefe9c321e569d2277f06db3b90`), built Release rather than using a system package. Wall times were about 55 s and 67 s.

From the tested source, with `box2d` pointing at that checkout:

```sh
build=$(mktemp -d)
out=$(mktemp -d)
cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DQUADTREE_BUILD_BENCHMARKS=ON -DFETCHCONTENT_SOURCE_DIR_BOX2D="$box2d"
cmake --build "$build" --target quadtree-bench
ctest --test-dir "$build" -R '^bench-gate-check$' --output-on-failure
for rounds in 3 9; do
  dir="$out/$rounds"
  mkdir "$dir"
  "$build/bench/quadtree-bench" --model=snapshot --types=int32 \
    --scenes=uniform,clustered,split-line,split-line-translated,mixed-size,outliers \
    --engines=index,index-ordered,qt-loose-sort,sweep,grid,brute \
    --sizes=1000,10000,100000 --small-sizes=0,1,2,4,8,16,32,64 \
    --batch=256 --rounds="$rounds" --min-time=0.03 --min-reps=3 --max-reps=12 \
    --csv="$dir/snapshot.csv" --raw="$dir/snapshot-raw.csv"
  "$build/bench/quadtree-bench" --gate-files="$dir/snapshot.csv" > "$dir/gates.txt"
  # Gate exit 3 is expected for this revision.
done
```

**Tested source.** Start from [HEAD](source-head.txt) (`f4390e5af2aead75ceaffa586c4051d2695339a0`), apply [the tracked delta](source-tracked.patch) at the checkout root, and extract [the untracked build inputs](source-untracked.tar.gz) there ([file list](source-untracked.txt)). This covers the runtime headers, benchmark, and test sources. The reconstruction was verified against [the input checksums](source-inputs.sha256).

**G1 diagnostic only.** Compile the saved driver against the tested source and record approximately one second per scene/size:

```sh
profile=bench/results/current-snapshot-2026-11-18/g1-profile
c++ -std=gnu++20 -O3 -DNDEBUG -g -Wall -Wextra -Wpedantic -Wconversion \
  -Wsign-conversion -Wshadow -Ibench -I. "$profile/profile-g1.cpp" \
  bench/bench-memory.cpp -o "$out/profile-g1"
for scene in uniform clustered; do
  for n in 0 1 2 4 8; do
    perf record -q -e cycles:u -F 4999 -o "$out/$scene-$n.data" -- "$out/profile-g1" "$scene" "$n"
    perf script -i "$out/$scene-$n.data" -F period,ip,sym,srcline > "$out/$scene-$n.txt"
  done
done
```

The [symbol/source histogram](g1-profile/samples-by-location.csv.gz) retains sample counts and weights (`total_period`). Phase shares use period weights: adapter = `LibraryIndex::build`; replacement = `buildRange`, workspace destructor and allocator support; pairs = `forEachPair` and its inlined query code in `step` (`quadtree-index-impl.h` lines 268–294, 435–436, 1058–1082, and 1975–2120). Other includes checksum, loop overhead, and unattributed samples.
