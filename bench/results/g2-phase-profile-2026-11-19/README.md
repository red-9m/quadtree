# 03 · Representative snapshots — profile the retained bulk build

**`Index::makeKey` is the largest build hotspot: 44–55% of attributed build cycles on the six failing G2 rows.** Topology counting is 3–6%; do not optimize it first. No runtime or benchmark source changed.

## Retained G2 baseline

The [9-round run](baseline/gates.txt) and [15-round repeat](repeat/gates.txt) each give **8 pass, 6 fail, 1 inconclusive**. `uniform`, `split-line`, and `split-line-translated` fail at 10,000 and 100,000; `clustered` at 100,000 is still inconclusive (1.972× [1.913–2.033] and 2.004× [1.945–2.065]). Grid is the fastest baseline on every G2 row. Both timings cover the full 01 snapshot slice; the raw samples and summary CSVs are in the run directories.

Nine-round phase medians and point-estimate gaps, ms/snapshot. The gap is the full-step `index` estimate minus 2× the grid estimate, not the saving needed for the full interval to pass.

| Scene | Entries | `index` build / pairs | Grid build / pairs | Gap to 2× grid |
|---|---:|---:|---:|---:|
| `uniform` | 10,000 | 0.940 / 0.662 | 0.231 / 0.350 | 0.444 |
| `uniform` | 100,000 | 13.157 / 7.113 | 2.555 / 3.809 | 7.731 |
| `split-line` | 10,000 | 1.046 / 0.711 | 0.251 / 0.338 | 0.575 |
| `split-line` | 100,000 | 14.746 / 7.236 | 2.599 / 4.317 | 8.022 |
| `split-line-translated` | 10,000 | 1.046 / 0.685 | 0.251 / 0.341 | 0.552 |
| `split-line-translated` | 100,000 | 14.769 / 6.821 | 2.556 / 3.913 | 8.779 |
| `clustered` | 100,000 | 13.399 / 14.300 | 2.538 / 11.563 | −0.398 |

[Measured step/phase estimates, errors, work counts and allocations](g2-budget.csv) for both runs. Both snapshot runs exit `0` and all 136 library rows have zero warm allocations. G3/G4/G6 pass. G1 still has six failures per run. **New baseline observation:** G9 `mixed-size` at 100,000 fails in both runs (4.518× and 4.495× ordered/unordered, vs 4×); track it as a pre-existing failure for any G2 patch, not as a regression introduced by that patch.

## Build attribution

Sampled the original Release `index` in the actual unbatched runner. Ranges below are shares of attributed build cycle periods across both profiles of each of the three failing scenes at each size. `countNodes` and `materialize` share `cut`; callchain parents distinguish their work.

| Phase | 10,000 entries | 100,000 entries |
|---|---:|---:|
| Adapter and input capture/validation | 2.7–4.5% | 6.0–6.3% |
| `makeKey` | 53.1–55.1% | 44.2–47.0% |
| `radixSort` | 10.0–12.8% | 8.6–10.0% |
| `countNodes` | 4.9–5.6% | 3.3–4.1% |
| Entry commit | 6.1–8.3% | 21.0–24.7% |
| Topology commit (`materialize`, linking and refit) | 15.7–18.9% | 11.6–12.3% |

On `clustered` 100,000, `makeKey` is also the largest build phase (43.6–44.4%), but traversal accounts for slightly more process cycle weight than the whole build. See [phase summary](phase-summary.csv) for all buckets and both processes per row, including non-step work. Profiles contain startup and one cold build plus 29–499 warm steps per process. These are period-weighted samples, not phase wall times or removable cost. Sampling skid and optimized source mapping limit instruction attribution.

For scale, weighting the measured 100,000-entry build medians by the sampled key-generation shares gives about 5.8–6.9 ms/snapshot. The observed full-step gaps are 7.7–8.8 ms/snapshot; a key-generation optimization is a first step, not a credible single fix for all rows.

[Verification](verification.log): baseline status and fixtures match 01; uncapped rows at G2 sizes match work counts, pair checksums and pair counts. All 14 profile rows match their baseline rows with zero warm allocations. No build-path tests rerun because no runtime code changed.

**Next:** stage one `Index::makeKey` optimization: hoist the exact integer box center and spans out of the descent loop. Keep Morton digits, midpoint rounding, overflow behavior, node topology and the float path unchanged. Compare key/topology and exact pair identities, then run the full G2 slice and regression gates. Use entry commit or traversal as the next measurements if key generation alone falls short.

## Reproduction

Use the [retained source and pinned Release build](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction). Build the [debug twin](../g1-retained-runner-profile-2026-11-19/profile.sh) with the same flags plus `-g`. The profile script checks [`.text` hashes](text.sha256) and [layout](text.layout); the debug twin is never timed. Set platform profile and all CPU energy preferences to `performance` first. From the repository root:

```sh
bash bench/results/g2-phase-profile-2026-11-19/profile.sh /path/to/control-bench /path/to/debug-twin /fresh/outdir
taskset -c 6 bash bench/results/g1-flat-input-2026-11-19/reproduce.sh /path/to/control-bench /fresh/outdir/repeat 15 full
```

[Environment](environment.txt): Ryzen 9 5900HS, GCC 16.2.1 Release, Box2D `v3.2.0` at `178495ba35695aefe9c321e569d2277f06db3b90`, CPU 6, `rdtsc` timer, `tsc` kernel clock source. The [run order](run-order.csv) reverses the seven focused rows across two blocks. `perf` captures `cycles:u` at 2999 and 3469 Hz, using user DWARF callchains. Extract [raw period samples and callchains](raw-callchains.tar.gz) alongside the baseline and repeat directories to regenerate the summaries with `summarize.py`; pass the original recorded Release path as the DSO identifier and the debug twin for symbolization.
