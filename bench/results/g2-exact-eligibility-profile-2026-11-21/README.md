# 03 · Exact eligibility instructions on the staged control

**Cross exact geometry spends more sampled cycles in box setup and mode/degeneracy dispatch than in strict endpoint tests.** In the current ELF, the direct cross path loads the operands and checks boundary/degeneracy before testing separation. Move mode handling to closed-contact cases only, as a cross-item experiment, not a change to node admission.

Reused six verified unordered captures from the [staged traversal profile](../g2-staged-traversal-2026-11-21/README.md). The [direct cross helper](cross-exact.asm) is audited by PC; inline self/holder work uses source locations. Buckets include address setup where it shares an emitted block. Percent of **all sampled unordered pair cycles**, ranges across reversed blocks:

| Fixture | Cross filter | Cross charge | Cross box/setup + mode | Cross strict endpoints | Self box/setup + mode |
|---|---:|---:|---:|---:|---:|
| `split-line` 10,000 | 4.51–4.78 | 0.98–1.20 | 5.03–5.59 | 3.09–3.17 | 6.45–6.91 |
| `uniform` 100,000 | 6.26–6.96 | 2.04 | 7.79–8.68 | 3.60–4.25 | 6.82–7.06 |
| `split-line-translated` 100,000 | 4.46–4.56 | 1.25–1.28 | 5.20–5.59 | 2.38–2.46 | 7.45–7.52 |

- No sampled periods land in the identified closed-endpoint block. The [census](../g2-item-eligibility-census-2026-11-21/README.md) reports 92–96% disjoint direct-cross candidates and less than 0.73% closed contact on blockers. Neither a zero sample count nor a low contact frequency waives the geometry contract. `cycles:u` can skid; the block attribution does not establish branch latency or predict the gain.
- [Periods by origin and phase](exact-periods.csv) · [Hot PCs](hot-pcs.csv) · [Verification](verification.log). All per-origin period totals reconcile exactly; fixture, work, and no-warm-allocation checks were rerun against the staged reference. No new captures or gate verdicts.
- Reproduce using the classifier's [archived captures and `.text` twin](../g2-bulk-classifier-2026-11-21/README.md#reproduction): `python3 summarize.py CAPTURE_DIR RESULT_DIR`.

## Handoff

**Screen a packed, contact-only mode check for direct cross item eligibility.** In `crossPairs` own-range loops only, for default-predicate `int32` SSE2, compare opposite minimum/maximum lanes for strict separation first. If none separate and none are equal, accept. Only a closed contact needs the existing boundary and degenerate rules. Keep filter and candidate charging before geometry, exact prefix/report orientation, custom-predicate fallback, and other callers' original geometry. Unlike the old [packed node gate](../g2-packed-cross-2026-11-21/README.md), this changes only exact item testing; unlike the older [scalar plus hoisting screen](../g2-contiguous-pairs-2026-11-20/README.md), it does not modify ranges, visits, or filter evaluation. Check extremes, contacts, budgets, and emitted code before a both-engine phase screen.
