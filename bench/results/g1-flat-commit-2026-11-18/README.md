# 02 · Small snapshots — flat and singleton commits

**G1 is still open.** A nonempty replacement still validates first and acquires a fresh identity before destroying old entries. A singleton below the existing flat threshold stages one descriptor, without the general descriptor/rank loop. Unranked flat builds with trivial payload construction publish entry and slot counts once. Existing topology is reset when present, not rewritten on already-flat indexes. No change to the adapter, brute baseline, workload, or gate thresholds.

| G1 verdict | Previous full (18 rounds) | New full (18 rounds) | New G1-only repeat (27 rounds) |
|---|---:|---:|---:|
| Pass | 6 | 7 | 6 |
| Fail | 6 | 8 | 6 |
| Inconclusive | 4 | 1 | 4 |

In the matched full runs, singleton time fell from about 34 ns to 24–25 ns/scene and two-entry time from 53–54 ns to 49–50 ns. Both scenes still fail at 1, 2, and 4 entries. Both empty rows failed in the full run and were inconclusive in the repeat; the uniform eight-entry pass did not repeat. Both scenes pass at 16, 32, and 64. See [all 16 rows, absolute estimates and intervals](g1-comparison.csv).

Both new runs completed without cross-check failures and with zero warm allocations for every library row. Status, pair count, checksum, fixture hash, candidate count, and node visits matched the previous revision. The measured G2 counts were unchanged; G3/G4/G6/G9 passed in the full run. [16 verification checks passed](tests.log), including exception-disabled builds, failure injection and the quick differential fuzzer. Tests now cover topology-to-flat replacement, ranked and unranked singletons, exact flat pair handles, stale handle invalidation, untouched output suffixes, and allocation-free prepared replacement.

## Isolating the changes

The [batched exact-pair probe](../current-snapshot-2026-11-18/g1-profile/profile-g1.cpp) ran twice on uniform batches of 256, against the previous headers (`before`), with just the topology-reset guard (`reset`), with the guard and count publication (`flat`), and with the additional singleton path (`single`). [Probe results](attribution.csv) are diagnostic, not gate timings. The reset guard alone did not improve one or two entries, but reduced four-entry probe time from roughly 70 to 65 ns/scene. The flat commit reduced it further to about 58 ns; the final build with the singleton path measured about 63 ns. The singleton path lowered the one-entry probe from about 24 to 16 ns. The full gate benchmark shows the same singleton gain but little four-entry gain; do not extrapolate from the probe.

Next: keep 02 open. Profile the singleton storage/identity preparation and the remaining four-entry full step separately, then compare the complete build-plus-pairs step again. Do not count the conflicting empty or eight-entry results as passes.

## Reproduction

Use the [task 01 recipe](../current-snapshot-2026-11-18/README.md#environment-and-reproduction) with `--rounds=18`. For the G1-only repeat use `--rounds=27 --scenes=uniform,clustered --sizes=` and keep the same small sizes, batch, engines and other timing arguments. [Environment](environment.txt): Ryzen 9 5900HS, GCC 16.2.1 Release, performance energy preference and pinned Box2D v3.2.0. Gate evaluator exit `3` is expected.

- Before: [previous full results](../g1-small-snapshots-2026-11-18/repeat/gates.txt).
- After: [CSV](snapshot.csv), [raw](snapshot-raw.csv.gz), [gates](gates.txt).
- Repeat: [CSV](repeat/snapshot.csv), [raw](repeat/snapshot-raw.csv.gz), [gates](repeat/gates.txt).
- Targeted 18-round comparisons: [before](targeted/before/gates.txt), [flat](targeted/flat/gates.txt), [single](targeted/single/gates.txt). `flat` is the previous revision plus [this patch](targeted/flat.patch). For `reset`, apply only that patch's first hunk. The probe uses the [same build recipe](../current-snapshot-2026-11-18/README.md#environment-and-reproduction), without `-g` or `perf`, and sizes `1 2 4`.

For the tested source use [HEAD](source-head.txt), apply the [tracked delta](source-tracked.patch), extract [untracked build inputs](source-untracked.tar.gz), and verify the [input checksums](source-inputs.sha256).
