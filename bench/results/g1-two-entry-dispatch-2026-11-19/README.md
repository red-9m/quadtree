# 02 · Small snapshots — two-entry pair dispatch

**Neither single-candidate path was retained.** Extracting a shared body slowed two and four entries. Reusing `testPair` gave no repeatable two-entry win and failed both empty G1 rows. Runtime and benchmark sources are unchanged; the two-entry contract tests are retained.

## Matched G1 runs

| Runtime | 27 rounds: pass / fail / inconclusive | 45 rounds: pass / fail / inconclusive |
|---|---:|---:|
| Unchanged | [9 / 6 / 1](27-control/gates.txt) | [10 / 6 / 0](45-control/gates.txt) |
| [`shared-body`](shared-body.patch) | [8 / 8 / 0](27-shared-body/gates.txt) | [8 / 8 / 0](45-shared-body/gates.txt) |
| [`reuse-test-pair`](reuse-test-pair.patch) | [8 / 8 / 0](27-reuse-test-pair/gates.txt) | [8 / 7 / 1](45-reuse-test-pair/gates.txt) |

`shared-body` calls the same candidate lambda directly at two entries and from the existing loop at three and four. GCC outlined both calls. At two entries it reached 38.0–39.1 ns/scene on repeat, up from 36.0–36.6 ns. At four it rose from 59.1–64.1 ns to 74.5–79.1 ns.

`reuse-test-pair` calls the existing `testPair(0, 1, ...)` from `tinyPairs` and leaves the general tiny loop intact. Its matched two-entry run was 0.7–1.4 ns faster, but the repeat was 35.4 ns (`clustered`) and 36.8 ns (`uniform`) versus 36.0 and 36.6 ns unchanged. The `uniform` repeat interval is inconclusive, not a pass. Both variants failed both empty gate rows: `shared-body` left absolute empty `index` timing essentially unchanged, but its `brute` timing was lower; `reuse-test-pair` also increased absolute empty `index` timing by about 0.8–0.9 ns on repeat. The empty query does not enter the new branch. All 8–64 rows passed in every run.

The [comparison CSV](g1-comparison.csv) includes absolute estimates, errors and frozen ratio intervals for all 16 rows per run. Both library engines matched counts, checksums, fixture hashes, candidates and node visits, with zero warm allocations. The [exact-pair probe](exact-pair-probe.log) matched brute identities and had zero warm allocations on all 16 rows for each candidate.

## Retained verification

[Tests](retained-tests.patch) cover two-entry contact in both boundary modes, a degenerate box, dense/slot order after removal, predicate rejection and acceptance with exact views, Stop, filtering and candidate budgets. [All 16 CTest checks passed](tests.log), including exception-disabled tests and the quick differential fuzzer.

**Next:** stay on 02. At 2 and 4 entries, isolate warm `buildPrepared` dispatch from the `buildFlatInteger` entry commit in the actual batched runner. The [runner profile](../g1-runstep-profile-2026-11-19/README.md) attributes 28–38% of cycles to replacement at these sizes; the single-candidate dispatch experiments did not close that gap.

## Reproduction

Start from the [retained source](../g1-view-construction-2026-11-19/README.md#reproduction) and build with the [baseline Release recipe](../current-snapshot-2026-11-18/README.md#environment-and-reproduction). Apply only the matching runtime patch for each candidate. Apply [retained-tests.patch](retained-tests.patch) for the final test run on the unchanged runtime and verify the [retained inputs](retained-inputs.sha256).

For each binary, run `taskset -c 6 bash bench/results/g1-flat-input-2026-11-19/reproduce.sh /path/to/quadtree-bench /fresh/outdir 27` and repeat with `45`. Run order was 27 control, 27 and 45 shared-body, 45 control, 27 and 45 reuse-test-pair. Keep the summary CSV, compressed samples and verdicts. Build [phase.cpp](../g1-flat-input-2026-11-19/phase.cpp) with the [baseline probe flags](../current-snapshot-2026-11-18/README.md#environment-and-reproduction) for the exact-pair checks.

Environment: Ryzen 9 5900HS pinned to CPU 6; GCC 16.2.1; Linux 7.3.0-1-custom; `powersave` governor, `power` energy preference, HPET clock source. Gate timer: invariant TSC. Release flags: `-O3 -DNDEBUG`, no native/LTO. Box2D `v3.2.0` at `178495ba35695aefe9c321e569d2277f06db3b90`.
