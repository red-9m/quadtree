# 04 · Prepared moved-set setup before traversal

**No per-call O(n) pass detected.** With `|S| = 1`, setup stayed at 16.8 ns unordered and 18.4 ns ordered from 1,000 to 100,000 entries. The explicit membership reset, measured separately as an O(n) control, went from 29.9 ns to 3.03 µs. This is a diagnostic, not an acceptance-gate run.

Median ns/call, unordered / ordered except reset:

| Entries | Empty | One handle | Fixed 10 handles | 1% of entries | Explicit reset |
|---:|---:|---:|---:|---:|---:|
| 1,000 | 1.75 / 1.75 | 16.83 / 18.39 | 32.99 / 40.68 | 32.95 / 40.76 | 29.87 |
| 10,000 | 1.74 / 1.75 | 16.83 / 18.37 | 33.86 / 43.20 | 252.63 / 359.90 | 305.59 |
| 100,000 | 1.74 / 1.75 | 16.82 / 18.40 | 33.46 / 44.83 | 3,674.25 / 4,858.15 | 3,029.72 |

At fixed 10 handles, the small ordered increase does not track the 100× entry-count increase. At 1%, both n and moved count change; those rows alone do not distinguish the two effects. The empty row includes probe overhead; no baseline subtraction. [Raw batches](samples.csv) · [Medians and observed ranges](summary.csv).

## What is measured

[measure.cpp](measure.cpp) builds an `int32` tree with `Checked` handles and prepares the query workspace before timing. Fixtures follow the single-moved grid recipe: 320 columns, spacing 16, box width/height 20, rank `n - i` so every rank is positive. Sixteen deterministic moved lists sample handles at stride 97; same-size prefixes exercise distinct submission cardinalities without generating handles inside a batch.

`nodeVisits = 0` denies the first root visit before bounds, filters or entry work. Empty calls return `Ok`; nonempty calls return `BudgetExhausted`. Untimed checks cover every list per batch: all five work counters stay zero; any result callback aborts even during timing. All batches check status, zero allocation and unchanged retained workspace. The ordered row measures validation, membership and entry to sparse source discovery, **not** the cost of finding or sorting sources. The explicit reset control clears membership between batches, not between query calls; the rare automatic epoch wrap is outside this measurement.

Three pinned processes; nine batches each, rotated case order and alternating n order. `steady_clock` surrounds batches, not individual calls. Timed calls include wrapper/driver overhead; a `noinline` wrapper and compiler memory barrier keep the empty path in the loop. Min/max are observed batch ranges, not confidence intervals. No runtime or frozen harness changes. [Targeted CTest](ctest.log): 4/4.

## Reproduce

From the repository root, on the setup in [environment.txt](environment.txt), set the platform profile and CPU energy preferences to `performance` for timing. The original settings were [restored](restored-mode.txt) afterward.

```sh
p=bench/results/moved-setup-2026-11-20
w=$(mktemp -d)
g++ -std=gnu++20 -O3 -DNDEBUG -fno-exceptions -Wall -Wextra -Wpedantic \
  -Wconversion -Wsign-conversion -Wshadow -Werror -I. "$p/measure.cpp" -o "$w/measure"
for run in 1 2 3; do taskset -c 6 "$w/measure" "$run" > "$w/run$run.csv" || exit; done
awk 'FNR == 1 && NR != 1 {next} {print}' "$w"/run[123].csv > "$w/samples.csv"
python3 "$p/summarize.py" "$w/samples.csv"
ctest --test-dir /path/to/release-build --output-on-failure \
  -R '^(index-test(-noexcept)?|index-fuzz-quick|index-regression-bench-smoke)$'
```
