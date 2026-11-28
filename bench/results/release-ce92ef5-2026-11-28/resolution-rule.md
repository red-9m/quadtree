# Pre-specified rule: `ce92ef5` four-type release validation (written before any stage ran)

**Purpose:** [Task 09](../../../docs/plan/09-other-coordinate-types.md) acceptance of `int64`, `float` and `double`, and the `int32` release recheck, on the same run. The brief's exit: for each type every G2 row passes, G1 passes, G9 passes, G0/G3/G4/G6 hold, all on a full [08](../../../docs/plan/08-final-acceptance.md) matrix of the committed source, and `int32` does not regress.

**Candidate:** `ce92ef580b7582db1afa59528dd710d26b5abe56`, benchmark `.text` `f4905252…` ([text.sha256](text.sha256)), runtime header `8c653e4f…`. Clean `git archive` Release build, GCC 16.2.1, CMake 4.4.2, `QUADTREE_BENCH_NATIVE=OFF`, Box2D v3.2.0 archive SHA-256 `ffec747b…` as pinned in `bench/CMakeLists.txt`. Every invocation rechecks both hashes.

**Stage order** ([matrices.sh](matrices.sh), [run.sh](run.sh), unchanged from [release-c13e498](../release-c13e498-2026-11-26/README.md) except for this directory); arguments are the manifest defaults (5 rounds, seed 1, default caps), engines as in prior release runs:

1. `calib-a`, `calib-b`: two identical `int32` snapshot processes, compared with `--diff`. Target ≥ 95% `same`. Below that, timing verdicts are published but timing sign-off is withheld for every type.
2. 5-round snapshot matrices: `int32`, `float`, `double`, `int64`.
3. Snapshot repeats (below), per type.
4. 64-step persistent matrices: `int32`, `float`, `double`, `int64`.
5. Persistent repeats (below), per type, only if needed.

**Repeat rules** (same as release-c13e498). Every hard row `inconclusive` in its matrix is rerun in exactly two separate 45-round processes (`<type>-resolve`/`-resolve-2` for snapshot, `<type>-presolve`/`-presolve-2` for persistent) with identical arguments, selected by scene and size (G1 via `--small-sizes`; a G4 row adds `split-line`; G8b adds n=1000). The final verdict is the pooled interval over the two processes: geometric-mean ratio, half-width sqrt(h₁² + h₂²)/2 of the log half-widths. Pass if inside the limit, fail if entirely outside, otherwise **inconclusive, and the row stays open**. No further repeats.

**What does not change:**
- Rows that pass or fail in a matrix keep that verdict; failures are never rerun. Rows a repeat process measures but the rule did not select are published, not used.
- Original matrix verdicts and repeat verdicts are published separately.
- An aborted or killed stage is kept as `<stage>-aborted`, is not evidence, and is rerun from scratch with the same arguments.
- A nonzero benchmark exit (cross-check failure, `MISMATCH`, `NONDETERMINISTIC`, `UNORDERED`) stops the run and blocks release.
- Frozen limits, fixtures, adapters and process errors are unchanged.

**Exceptions:** `int32` only, as in the 08 brief: G1 `uniform`/`clustered` 0–4 entries with upper bound ≤ 3.5× brute, and G2 `split-line-translated` n=10² and `uniform` n=10³ with upper bound ≤ 4.5× the best baseline. The upper bound is the matrix interval for a matrix `FAIL`, else the pooled interval. **No exception for `int64`, `float` or `double`, and no G9 exception for any type.**

**Decision, per type** (each type is decided on its own rows):
- **`int32` (release recheck):** every hard snapshot and persistent gate passes or is an accepted exception, and all 15 G9 rows pass. Otherwise `int32` release acceptance does not extend to `ce92ef5` and stays on `c13e498`.
- **`int64`, `float`, `double` (Task 09):** every hard snapshot gate (G1 16, G2 20, G3, G4, G6, G9 15) and every applicable hard persistent gate passes, with no exception. A blocking or still-open row leaves that type functionally supported only, with the row published.
- **All types:** G0 (every process exits 0) and G6 (zero warm allocations) block. Calibration ≥ 95% is required for any timing sign-off.
- **Also required:** CTest on the archive build, the full differential fuzz (4 types × 5 scenes, seeds 1–40, 1500 steps) on the archive build, and hosted Portability CI on the exact candidate SHA, 9/9 jobs.

**Watch items**, recorded without changing method:
- `int32` G1 n=4 rows (`uniform`, `clustered`): thin margins on both `c13e498` and the candidate in the [candidate evidence](../float-common-pairs-2026-11-27/README.md); original matrix verdicts published as measured.
- `float clustered n=2` G1: output-path lengths are fixed by this directory and recorded. An inconclusive is not a pass.
- `float`/`double` G9 `split-line`/`split-line-translated` 10⁵: candidate upper bounds up to about 3.86 (limit 4).
