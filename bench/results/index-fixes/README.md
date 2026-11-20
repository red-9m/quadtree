# Persistent mutation and ordered moved-set fixes

Targeted before/after measurements, not a rerun or waiver of the frozen gates.
`before.csv` and `after.csv` use the same `bench/index-regression-bench.cpp`
compiled against the headers before and after these fixes. `library.patch`
records that header delta so the baseline can be reconstructed in a scratch copy.

## Changes

- Unproductive leaves retry after about ownCount/2 successful own changes,
  rather than re-keying/sorting on every mutation. Failed operations do not
  consume the retry counter; no-op geometry/filter changes do not advance it.
- Own bounds/filter ORs extend incrementally. Removals/replacements may retain
  conservative old contributions until a periodic tight refit, avoiding another
  per-mutation leaf scan. Ancestors combine cached own summaries and children.
- Sparse ordered moved sets discover clean lower-rank source rows from S, then
  sort/traverse only that source set. Custom predicates run in rows, not source
  preparation. Both geometry passes are budgeted, and no partial source set or
  unfinished row is emitted. Tiny-S clean rows scan S directly, including piles.
- Query workspace is unchanged; nodes grow by 8 bytes (120/168 bytes for
  int32-or-float/int64-or-double). Periodic refits/retries can still have linear
  individual latency; the mutation improvement is amortized, not hard real-time.

## Method

AMD Ryzen 9 5900HS; GCC 16.2.1; portable C++20, `-O3 -DNDEBUG -fno-exceptions`,
without native/LTO flags. Three batches per row; median nanoseconds per operation.
`steady_clock` times entire batches, not individual nanosecond-scale calls.
Setup, reservation and printing are outside each batch. All timed library paths
made zero allocations; every moved query reported the expected eight pairs.
These development timings have no calibrated confidence intervals.

Fixtures are explicit integer geometry, without standard random distributions:

- Coincident: fixed root `[0,16384]^2`, n copies of `{16,16,24,24}`. Each update
  alternates one entry with `{17,16,24,24}`; centers/size prefixes remain
  unproductive. Three clear/insert/update batches, with 2n updates per batch.
- Coarse ancestors: fixed root `[0,1024]^2`, n root-sized boxes plus 64 small
  boxes at `(32 + 112*(i%8), 32 + 112*(i/8))`, extent 8. Each sample updates
  one small entry 8,192 times without changing its owning cell. Here n is the
  number of coarse entries, not the total entry count.
- Single moved: 320 columns, origins `(16*(i%320),16*(i/320))`, extent 20,
  rank `n-i-1`, S = entry n/2. Per sample: 1,024 unordered calls or 16 ordered
  calls. Small n forms a thin strip, so the three sizes are not aspect-ratio
  normalized and do not constitute a frozen scaling benchmark.

## Results

Representative rows from the CSVs:

- 16,000 coincident entries, insert: 465,162 -> 249 ns/entry.
- 16,000 coincident entries, update: 1,015,385 -> 164 ns/update.
- 2,000 coarse ancestor entries: 10,520 -> 46 ns/update.
- 8,000 coarse ancestor entries: 42,180 -> 47 ns/update.
- 100,000 entries, one moved, ordered: 24.96 ms -> 0.80 us/call.
- That ordered query's node visits: 3,648,195 -> 186; reports remain eight.
- Its candidate tests: 27 -> 28; both discovery passes are counted, not hidden.

The final row is a sparse fixture, not a universal ordered-mode speed guarantee.
For a dense moved set the full ordered traversal is retained as a fallback.

## Reproduce

The current target is `quadtree-index-regression-bench` (also registered in
CTest for allocation/result checks, without machine-dependent timing limits).
For an identical two-header comparison:

```sh
mkdir -p /tmp/qt-before/detail
cp quadtree-index.h /tmp/qt-before/
cp detail/quadtree-index-impl.h /tmp/qt-before/detail/
(cd /tmp/qt-before && patch -R -p1 < /path/to/bench/results/index-fixes/library.patch)

flags='-std=c++20 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror -fno-exceptions'
g++ $flags -I/tmp/qt-before bench/index-regression-bench.cpp -o /tmp/qt-before-bench
g++ $flags -I. bench/index-regression-bench.cpp -o /tmp/qt-after-bench
/tmp/qt-before-bench before > before.csv
/tmp/qt-after-bench after > after.csv
```

Regression tests additionally assert linear bounds on split/refit work using
compile-time test-policy probes, retry/failure/no-op behavior, coarse ancestor
updates and both ancestor/child relocation directions, source epoch reuse,
sparse/pile query work, exact candidate/node budgets, every visitor-stop prefix,
and no emission from incomplete discovery/rows. The independent oracle, allocation
failure, no-exception, mixed-TU, ASan/UBSan and standard-library-debug checks remain.
Historical G1/G2 failures in `../index-development/` have not been re-evaluated
as part of this targeted fix.
