# Library Index development measurements

**Not release acceptance.** G1 (small scenes) and G2 (snapshot step speed)
still fail. Limits in `bench/MANIFEST.md` were not changed to accommodate the
implementation. These are a subset of the required final matrix.
They predate the targeted mutation/ordered moved-set fixes measured in
[`../index-fixes/`](../index-fixes/README.md); frozen gates have not been rerun
for that follow-up.

Files: `snapshot.csv`, `persistent.csv`, their gzip-compressed raw samples, and
`gates.txt`. CSV headers contain date, CPU, timer, compiler, flags and arguments.
All 167 library rows completed with matching results and zero warm allocations;
no tested engine produced `MISMATCH`, `NONDETERMINISTIC` or `UNORDERED`.

## Environment and method

- AMD Ryzen 9 5900HS; performance energy preference; invariant-TSC timer.
- GCC 16.2.1, `Release`: `-O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wconversion
  -Wsign-conversion -Wshadow`. No native/LTO flags on the library benchmark.
- Pinned Box2D v3.2.0, built Release (not the assertion-enabled system package).
- Three interleaved rounds with the manifest's fixture/PRNG and layout shifts.
- Snapshot: three to four warm samples per round, batches of 256 small scenes.
- Persistent: 16 steps per pass, not the final required 64-step acceptance run.
- Estimates and intervals use the harness's existing 2%/3% process terms; this
  development run is not a fresh process-error calibration.
- Library allocations go through the harness's counted `QuadTree::Resource`.
  Input/handle/moved-handle adapter vectors are included in memory and timing.
  Persistent reservation uses n entry/node/mutation-descriptor slots and a
  prepared n-entry workspace; automatic growth remains enabled and observable.

## Results

Frozen gate evaluator summary (pass / fail / inconclusive):

- G1 hard: 6 / 10 / 0.
- G2 hard: 4 / 6 / 5.
- G3 hard, candidate scaling: 6 / 0 / 0.
- G4 hard, translated scenes: 6 / 0 / 0.
- G6 hard, reserved allocation paths: 133 / 0 / 0.
- G7 hard, persistent full vs dynamic tree: 54 / 0 / 0.
- G8a hard, empty moved set: 9 / 0 / 0.
- G8b hard, single-mover scaling: 3 / 0 / 0.
- G8c hard, 1% motion vs dynamic tree: 18 / 0 / 0.
- G9 hard, ordered overhead: 15 / 0 / 0.

Soft grid comparisons also fail in some scenes; see `gates.txt` rather than
inferring grid dominance from the hard dynamic-tree gates.

At 100,000 entries, unordered snapshot steps (int32):

- Uniform: 21.77 ms, 788,738 candidates, 73,905 pairs.
- Split-line: 22.35 ms, 681,627 candidates, 105,137 pairs.
- Translated split-line: 22.03 ms, 625,694 candidates, 105,137 pairs.
- Mixed-size: 56.67 ms, 2,968,141 candidates, 2,054,380 pairs.
- Outliers: 31.65 ms, 709,110 candidates, 72,320 pairs; unlike the fixed
  16-level prototype, the library does not hit the candidate cap here.

Uniform persistent moved-set steps at 1,000 / 10,000 / 100,000 entries:

- Empty set: 19.7 / 19.8 / 20.0 ns (harness floor; no full-index setup).
- One mover: 283.3 / 383.3 / 338.3 ns, including mutation and dirty-handle capture.

Snapshot peak retained/preparation accounting at 100,000 entries is about
25–26 MiB for unordered library rows, including adapter input and descriptors.
Use the CSV's separate prepare/pair times, peak/retained bytes and ordered
rows; these numbers are not a claim that handle/rank/validation overhead is free.

## Reproduce

Configure Release with `QUADTREE_BUILD_BENCHMARKS=ON` and pinned Box2D enabled,
then use the exact `# args:` headers in the CSVs. The principal invocations are:

```sh
quadtree-bench --model=snapshot --types=int32 \
  --scenes=uniform,clustered,split-line,split-line-translated,mixed-size,outliers \
  --engines=index,index-ordered,qt-loose-sort,sweep,grid,brute \
  --sizes=1000,10000,100000 --small-sizes=0,1,2,4,8,16,32,64 \
  --batch=256 --rounds=3 --min-time=0.03 --min-reps=3 --max-reps=12 \
  --csv=snapshot.csv --raw=snapshot-raw.csv

quadtree-bench --model=persistent --types=float \
  --scenes=uniform,split-line,mixed-size --engines=index,qt-loose,grid,b2tree-fat \
  --persistent-sizes=1000,10000,100000 \
  --workloads=static,small-single,small-1%,teleport-1%,churn-1%,noop-100% \
  --modes=full,moved --steps=16 --rounds=3 --max-time=30 \
  --csv=persistent.csv --raw=persistent-raw.csv

quadtree-bench --gate-files=snapshot.csv,persistent.csv  # Expected exit 3
```

Remaining work: reduce tiny-build/dispatch overhead and representative snapshot
cost, resolve inconclusive intervals, then rerun all types/scenes/motion rates,
full 64-step passes, process calibration and the complete frozen gates. No
performance hard gate is waived by passing correctness or allocation tests.
