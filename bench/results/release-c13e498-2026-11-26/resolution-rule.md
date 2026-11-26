# Pre-specified rule: `c13e498` release validation (written before any stage ran)

**Candidate:** `c13e498fffe18736ea0a8ab871eb366cdfa5aa3c`, benchmark `.text` `369ce16e…` ([text.sha256](text.sha256)), runtime header `0264f71b…`. Clean `git archive` Release build, GCC 16.2.1, CMake 4.4.2, `QUADTREE_BENCH_NATIVE=OFF`, Box2D v3.2.0 archive SHA-256 `ffec747b…` as pinned in `bench/CMakeLists.txt`. Every invocation rechecks both hashes.

**Stage order** ([matrices.sh](matrices.sh), [run.sh](run.sh)); arguments are the manifest defaults (5 rounds, seed 1, default caps), engines as in prior release runs:

1. `calib-a`, `calib-b`: two identical `int32` snapshot processes, compared with `--diff`. Target ≥ 95% `same`. Below that, timing verdicts are published but `int32` timing sign-off is withheld.
2. 5-round snapshot matrices: `int32`, `float`, `double`, `int64`.
3. Snapshot repeats (below), per type.
4. 64-step persistent matrices: `int32`, `float`, `double`, `int64`.
5. Persistent repeats (below), per type, only if needed.

**Snapshot repeat rule.** For each type, every hard snapshot gate row with an interval that is `inconclusive` in its 5-round matrix is rerun in exactly two separate 45-round processes, `<type>-resolve` and `<type>-resolve-2`, with identical arguments. Rows are selected by scene and size from the matrix's inconclusive rows (`--small-sizes` for G1, `--sizes` otherwise; a G4 row adds `split-line`). The final verdict is the pooled interval over the two processes: geometric-mean ratio, half-width sqrt(h₁² + h₂²)/2, where hᵢ is each process's log half-width. Pass if the interval lies inside the limit, fail if it lies entirely outside, otherwise **inconclusive, and the row stays open**. No further repeats.

**Persistent repeat rule.** Every hard persistent row that is `inconclusive` in its matrix is rerun the same way, in two separate processes `<type>-presolve` and `<type>-presolve-2`, with `--rounds=45`, the same engines, and `--scenes`/`--persistent-sizes`/`--workloads`/`--modes` covering the inconclusive rows (G8b adds n=1000). Same pooled verdict.

**What does not change:**
- Rows that pass or fail in a matrix keep that verdict; failures are never rerun. Rows a repeat process measures but the rule did not select are published, not used.
- Original matrix verdicts and repeat verdicts are published separately.
- An aborted or killed stage is kept as `<stage>-aborted`, is not evidence, and is rerun from scratch with the same arguments.
- A nonzero benchmark exit (cross-check failure, `MISMATCH`, `NONDETERMINISTIC`, `UNORDERED`) stops the run and blocks release.

**Exceptions (08 brief, `int32` only):**
- G1 `uniform`/`clustered` 0–4 entries: `FAIL` or inconclusive rows with upper bound ≤ 3.5× brute are `accepted exception`.
- G2 `split-line-translated` n=10² and `uniform` n=10³: upper bound ≤ 4.5× the best baseline is `accepted exception`.
- The upper bound is the matrix interval for a matrix `FAIL`, else the pooled interval.
- No G9 exception, and no exception for `int64`, `float` or `double`.

**Release decision:**
- **`int32`:** every hard snapshot and persistent gate passes or is an accepted exception. Any blocking or still-open row keeps release open. All 15 `int32` G9 rows must pass.
- **All four types:** G0 (every process exits 0) and G6 (zero warm allocations) block. Their timing and work gates are published, not release-blocking (Task 09).
- **Also required:** CTest on the archive build and hosted Portability CI on the exact candidate SHA, 9/9 jobs.

**Watch items**, recorded without changing method:
- `int32 uniform n=4`: original matrix verdict published as measured.
- `float clustered n=2`: output-path lengths are fixed by this directory and recorded. Adapters are unchanged, and an inconclusive is not a pass.
- `int64 split-line-translated n=100000` G2: published; no `int64` exception.
