# 03 · Representative snapshots — bounded bulk-commit prefetch

**Staged, not retained.** The isolated prefetch cut full-step time 5.8–9.6% on the three failing 100,000-entry rows and moved `clustered` 100,000 from inconclusive to pass in both repeats. Six G2 failures remain. Runtime source is unchanged; this candidate does not include the integer key hoist or the rejected `crossPairs` split.

## Candidate

[prefetch.patch](prefetch.patch) adds two read-prefetch stages in the spatial-order entry commit: descriptor base and payload-pointer field 32 entries ahead, then the pointed-to payload 16 entries ahead. Both lookaheads are bounded by the remaining entries, so they only read unconsumed keys with original input indices. The hints apply to `int32`, unranked, non-projected copy builds with trivially copy-constructible payloads under GCC/Clang. No publication, placement, or traversal changes. [Disassembly](entry-commit.asm) confirms the three `prefetcht0` instructions and tail guards. Distances were fixed before timing.

## Frozen full 01 snapshot slice

Counts are pass / fail / inconclusive.

| Rounds | G2 control | G2 candidate | G1 control | G1 candidate | G9 control | G9 candidate |
|---:|---:|---:|---:|---:|---:|---:|
| 9 | [8 / 6 / 1](9-control/gates.txt) | [9 / 6 / 0](9-candidate/gates.txt) | 9 / 6 / 1 | 8 / 6 / 2 | 14 / 1 / 0 | 13 / 1 / 1 |
| 15 | [8 / 6 / 1](15-control/gates.txt) | [9 / 6 / 0](15-candidate/gates.txt) | 8 / 6 / 2 | 10 / 6 / 0 | 14 / 1 / 0 | 12 / 1 / 2 |

15-round repeat, 100,000 entries, control → candidate (ms/snapshot):

| Scene | Step estimate | Build median | Pairs median | Candidate G2 ratio to grid |
|---|---:|---:|---:|---:|
| `uniform` | 20.336 → 18.738 | 13.051 → 11.423 | 7.112 → 7.149 | 2.905 [2.817–2.995] fail |
| `split-line` | 22.391 → 20.484 | 14.991 → 13.095 | 7.198 → 7.270 | 2.890 [2.787–2.997] fail |
| `split-line-translated` | 22.162 → 20.042 | 14.908 → 13.119 | 6.885 → 6.843 | 3.008 [2.901–3.120] fail |
| `clustered` | 27.837 → 26.336 | 13.378 → 11.876 | 14.295 → 14.330 | 1.842 [1.779–1.906] pass |

On the three failing 100,000-entry rows, build medians decrease 7.7–13.0%, and every full-step change interval excludes 1× in both repeats. None of the three 10,000-entry rows resolves a full-step gain; all still fail. [All G2 rows](g2-comparison.csv) include phases, work, allocations and gate intervals. [Matched step changes](step-change.csv) compare both repeats; phase medians are descriptive. Run directories contain the raw warm samples.

G0/G3/G4/G6 pass. G1's six hard failures are unchanged; the two candidate empty rows are inconclusive at 9 rounds and pass at 15. G9 still fails at `mixed-size` 100,000. With a faster unordered denominator, `split-line-translated` 100,000 becomes inconclusive in both candidate runs; `split-line` 100,000 also becomes inconclusive at 15 rounds. No new hard failures.

## Correctness

[Exact probe](equivalence.log): byte-identical topology, dense entries, full and moved-set pair streams and counters. Covers unranked builds with/without output handles, ranked fallback, both lookahead boundaries, and the default flat threshold. Checks output handles map to their input entries, replacement invalidates old handles, and warm builds and queries allocate zero. [Full-slice verification](verification.log) matches all uncapped fixtures, checksums, work, allocation counts and memory; all 272 library rows have zero warm allocations. [All 20 CTest checks](tests.log) pass.

**Next:** stage prefetch plus the integer `makeKey` hoist together and compare against retained code on the frozen slice. Check the empty G1 rows and both newly inconclusive G9 rows. Do not include the rejected traversal patch or infer combined gains from the isolated runs.

## Reproduction

Reconstruct the [retained source](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction) and apply `prefetch.patch` in a separate tree. Build with GCC 16.2.1 Release and Box2D `178495ba35695aefe9c321e569d2277f06db3b90`. Verify [`.text` hashes](text.sha256) using `objcopy --dump-section .text=NAME.text`. Set the platform profile and CPU energy preferences to `performance`, then run from the repository root:

```sh
bash bench/results/g2-entry-prefetch-2026-11-19/verify.sh /path/to/control-source /path/to/candidate-source
bash bench/results/g2-integer-key-2026-11-19/repeat.sh /path/to/control-bench /path/to/candidate-bench /fresh/outdir
python3 bench/results/g2-integer-key-2026-11-19/summarize.py /fresh/outdir
ctest --test-dir /path/to/candidate-build --output-on-failure
```

[Environment](environment.txt) and [run order](run-order.csv): four independent processes pinned to CPU 6, with unchanged fixtures and limits.
