# 02 · Small snapshots — singleton commit and tiny pair dispatch

**G1 remains open: 10 pass, 6 fail on the 27-round repeat.** Both scenes still fail at 1, 2 and 4 entries. Both empty rows passed on repeat. All eight rows that already passed (8–64 entries) passed in both retained runs.

## Diagnosis and changes

The [phase probe](../g1-flat-input-2026-11-19/phase.cpp) keeps warm build and pairs interleaved, checks exact pair identities against brute, and counts warm allocations. In the [baseline cycle samples](phase-summary.csv), replacement accounted for 66–67% at one entry, 40–41% at two, and 27% at four. Pair dispatch plus checksum accounted for 47% at two and 62–63% at four. Singleton samples landed around clearing the old slot/rank state and the identity compare-and-exchange. At two and four, the scan samples landed in the loop, candidate check and overlap comparisons. Source locations in the [weighted histogram](profile-by-location.csv.gz) are approximate; the noinline probe is for attribution, not gate timing.

- Prepared unranked integer replacement resets slots and ranks without zeroing the identity and counts it will immediately replace. Singleton commit writes one record directly. Validation and fresh identity acquisition still precede any state changes.
- Flat unordered sets of at most four entries skip per-candidate budget checks only when every possible pair fits. The scan checks strict overlap before testing contact under the boundary and degeneracy rules. Tighter budgets and larger sets keep the original loop.
- The [single-pair specialization](single-pair.patch) gave no repeatable two-entry step improvement, and both empty rows remained inconclusive on its repeat. The [fused tiny stores and branchless overlap check](fused-mask.patch) made the passing `clustered` eight-entry row inconclusive. Neither variant was retained.

## Measurements

| G1 run | Pass | Fail | Inconclusive |
|---|---:|---:|---:|
| [Unchanged, 18 rounds](before/gates.txt) | 8 | 6 | 2 |
| [Retained, 18 rounds](retained/gates.txt) | 9 | 6 | 1 |
| [Retained, 27 rounds](retained-repeat/gates.txt) | 10 | 6 | 0 |

At four entries, matched 18-round steps fell from 88.5 to 77.3 ns/scene (`uniform`) and from 78.4 to 71.6 ns/scene (`clustered`). The repeat measured 76.7 and 68.3 ns/scene. Singleton steps reached 20.3–20.5 ns/scene on repeat, down from 22.4–22.6 ns/scene in the unchanged run; `uniform` at one entry had a wide interval in the matched run. Two-entry steps stayed at 38–39 ns/scene on repeat. The [comparison CSV](g1-comparison.csv) gives absolute times, errors and frozen ratio intervals for every row.

[All 16 verification checks passed](tests.log), including exception-disabled builds and the quick differential fuzzer. New tests cover ranked topology → singleton replacement, failed validation with unchanged handles and output, rank reuse, slot orientation after mutation, contact, filters, predicates, Stop, and candidate budgets. The [exact-pair probe](exact-pair-probe.log) matched brute and observed zero warm allocations at all 16 sizes. [Benchmark checks](checks.txt) found no mismatches or warm allocations for either library engine; pair counts, checksums, fixture hashes, candidate counts and node visits matched the unchanged run.

**Next:** stay on 02. Compare the remaining fixed cost with the 2× budget; separate adapter preparation from replacement and query cost before trying another small-set path. Do not rerun descriptor-copy, all-size direct dispatch or single-pair experiments unchanged.

## Reproduction

Use the [previous source reconstruction](../g1-flat-input-2026-11-19/README.md#reproduction) for the unchanged build. Apply [retained.patch](retained.patch) to those inputs for the measured variant, then verify [source checksums](source-inputs.sha256). The working tree moves an existing rank-clear comment back above its function; the Release benchmark binary is byte-identical. Use the [task 01 Release recipe](../current-snapshot-2026-11-18/README.md#environment-and-reproduction) with pinned Box2D. See [environment](environment.txt).

Run `bash bench/results/g1-flat-input-2026-11-19/reproduce.sh /path/to/quadtree-bench /fresh/outdir 18`, then repeat with `27`. Each run directory retains the summary CSV, compressed raw samples and gate verdicts. For the rejected variants, apply only one experiment patch to the previous source before building. For the profile, use the task 01 `perf` recipe with the phase probe above at 1, 2 and 4 entries in both scenes. Run the same probe without `perf` at 0, 8, 16, 32 and 64 to check exact pairs and warm allocations on the remaining rows.
