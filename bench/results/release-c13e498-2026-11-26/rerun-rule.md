# Rerun rule: calibration pair + `int32` snapshot matrix (owner decision, 2026-11-27; written before the rerun)

**Why:** in the [first run](README.md#calibration), `calib-a`/`calib-b` agreed on 94.3% of rows, below the 95% target, so [resolution-rule.md](resolution-rule.md) withheld `int32` timing sign-off. The owner chose option 2: rerun a calibration pair plus the `int32` snapshot matrix under the same rule. The supplementary `calib-c`/`calib-d` pair does not count toward this decision.

**Same as the first run:** candidate, workspace binary and hash checks, [run.sh](run.sh), power, CPU 6, quiet-start protocol and arguments. Output goes to `data-rerun/`. Stages ([rerun.sh](rerun.sh)):
1. `calib-a`, `calib-b`: two identical `int32` snapshot processes, then `--diff`.
2. `int32-snapshot`: the 5-round matrix.
3. `int32-resolve`, `int32-resolve-2`: two 45-round processes for every hard row inconclusive in step 2. Same selection and pooling as resolution-rule.md, with the same `int32` exceptions (G1 0–4 entries ≤ 3.5×; G2 `split-line-translated` 10² and `uniform` 10³ ≤ 4.5×) and no G9 exception.

**Decision:**
- **Calibration ≥ 95% `same`** and every `int32` snapshot hard gate passes or is an accepted exception in this rerun: `int32` timing sign-off extends to `c13e498`. The persistent stages, G0/G6 for all types, CTest and hosted CI come from the first run, which passed them all.
- **Calibration < 95%:** sign-off stays withheld, as before.
- **Any `int32` snapshot hard row blocking or still open:** release stays open on that row.
- The first run's snapshot verdicts stay published unchanged. Nothing is pooled across the two runs.
- An aborted stage is kept as `*-aborted`, is not evidence, and is rerun from scratch.
