# 08 · `int32` G2 10²/10³: time-boxed fix attempt

**Rejected. The `int32` G2 blockers remain:** `split-line-translated` 10² (4.18× [4.06–4.30], limit 4) and `uniform` 10³ (3.93× [3.82–4.04], unresolved); both figures are from the [release run](../release-acceptance-2026-11-21/README.md#blocking-and-unresolved-rows). The one allowed attempt made every measured `index` row 0.2–9.3% slower. Nothing staged.

## Measured cost

These timings are from the release run. At `split-line-translated` n=100, `index` takes 6.0 µs: build 3.2 µs plus pairs 2.8 µs. The best baseline, `sweep`, takes 1.44 µs (build 0.74 plus pairs 0.69). Clearing 4× with its interval needs about 0.5 µs less. At `uniform` n=1000, `index` takes 74.0 µs against `grid` at 18.8 µs, so about 2 µs is needed.

[Profile of the release binary](control-profile.txt): `cycles:u` on `index` alone, `split-line-translated` 100, about 24 s.

| Function | Share of samples |
|---|---:|
| `planBulk` | 26.0% |
| `crossPairs` | 19.0% |
| `selfPairs` | 18.1% |
| `buildPrepared` | 8.5% |
| `itemsVsSubtree` | 5.0% |
| `refit` | 3.8% |
| `materializeBulk` | 3.2% |
| `buildDenseItems` | 3.1% |

Inside `planBulk`, 36% of samples sit on the stack-array tag histogram increment and the single-cursor scatter store. That suggested a store-to-load dependency chain between consecutive entries.

## Candidate

[packed-cursors.patch](packed-cursors.patch): for plan ranges ≤ 4095 entries, keep the five per-tag counts and scatter cursors as 12-bit lanes of a 64-bit register instead of stack arrays. Larger ranges keep the existing code. Output permutation, tags, plans and keys are unchanged.

- **Correctness:** [CTest](tests.log) passes 21/21 and the [full differential fuzz](fuzz.log) passes. Candidates, node visits and checksums are identical to control on every screened row.
- **Hashes:** [text.sha256](text.sha256). Control `.text` is the release binary's.

## Screen

Not a gate run. Two alternating process pairs on CPU 6: `int32`, `uniform`/`clustered`/`split-line`/`split-line-translated`/`mixed-size`, 10²–10⁴, `index,index-ordered,sweep,grid`, 15 rounds. Raw CSVs are in [screen/](screen/).

| Row | Control → candidate (`index`) | Change |
|---|---:|---:|
| `split-line-translated` 10² | 6.05 → 6.07 / 6.01 → 6.16 µs | +0.2% / +2.5% |
| `uniform` 10³ | 74.2 → 78.1 / 74.3 → 78.1 µs | +5.3% / +5.2% |
| all 15 `index` rows | — | +0.2% to +9.3% |

`index-ordered` also slowed (up to +4.1%). The profile attribution was misleading. The stack-array increments were not the bottleneck: Zen 3 likely forwards or renames them cheaply. The variable-shift lane arithmetic adds work on every entry.

## Conclusion

No measured single cost explains the 10² gap. Build and pair traversal each take about half of the time, and both are 4× the sweep's. Closing about 8% at n=100 would need structural small-tree work, the kind that took several Task 03 iterations at 10⁴–10⁵. The time box is spent. **The owner decides:** accept a G2 exception for these `int32` rows, or fund that work.
