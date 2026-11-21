# 03 · Audit translated G2/G9 uncertainty

**More rounds can plausibly resolve both gates without changing code.** At the 15-round candidate means and dispersion, the projected 45-round upper bounds are **1.971× G2** and **3.982× G9**. These are sensitivity calculations, not new gate verdicts.

The [audit](summarize.py) groups the untouched [frozen raw samples](../g2-packed-cross-item-2026-11-21/frozen/) by round for translated `int32` at 100,000. It reconstructs the harness means of round medians, Student-t round error, the 2% process term in quadrature, and the frozen log-scale ratio intervals. All 12 engine estimates/errors and all eight G2/G9 intervals across both binaries and round counts match the published precision. No trimming or altered thresholds.

15-round candidate uncertainty in each engine:

| Engine | Step estimate | Relative round error | Relative total error |
|---|---:|---:|---:|
| `grid` | 6.742 ms | 4.08% | 4.55% |
| `index` | 12.813 ms | 1.41% | 2.45% |
| `index-ordered` | 49.303 ms | 3.37% | 3.92% |

- The largest ordered steps are rounds 0–2 (53.04–53.91 ms): both phase times rise there. Grid has large rounds at 1 and 14 (7.93, 7.56 ms), and `index` at 2 and 3 (13.60, 13.48 ms). The largest samples do not all occur in one round or one engine. Raw timing does not identify the cause; don't remove startup rounds or replace the calibrated independent ratio with a paired ratio.
- Keeping each observed mean and sample SD fixed, upper bounds at 30 / 45 / 60 rounds are **1.979 / 1.971 / 1.967** for G2 and **3.995 / 3.982 / 3.976** for G9. With infinite rounds and the process term intact, the bounds still floor at **1.955** and **3.958**. At 30 rounds G9's projected margin is only 0.005×, which motivates one prespecified 45-round confirmation rather than selectively adding rounds. Different process means or dispersion can invalidate the projection.

[Round and phase medians](round-medians.csv) · [Error decomposition](error-decomposition.csv) · [Bounds projection](bounds-projection.csv) · [Verification](verification.log). Run `python3 summarize.py EXISTING_OUTPUT_DIR` to regenerate.

## Handoff

**Confirm the unchanged packed candidate in a fresh 45-round process.** Run the entire frozen `int32` slice so the grid reference, both library engines, G1 and cross-checks are preserved; use quiet preflight, CPU 6, the frozen process term and the same limits. This is a gate confirmation, not another source A/B, so use the already published matched controls for before/after. Record all outcomes without stopping early or pooling with the prior runs. If both G2 and G9 pass throughout the slice, stage the candidate and explicitly retain the earlier inconclusive run in the record. G1 still blocks 08.
