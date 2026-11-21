# 03 · Outline same-node pair qualification on two-stream + source radix

**Do not promote the outline.** It reproduces the unordered pair-phase savings, but `uniform` clears G2 only at 15 rounds, translated never clears G2, and `mixed-size` loses a G9 pass at nine rounds. Keep two-stream + source radix staged; no runtime change.

This screens the [previous outline factor](../g2-self-outline-2026-11-20/README.md) on the exact [two-stream + source-radix control](../g9-two-scatter-source-radix-2026-11-21/README.md). The [rebased patch](candidate.patch) is the only source change: default-predicate `int32` own-node pairs in `selfPairs` use `selfPairQualifies`, then the original `reportPair`. Custom predicates, other coordinate types, cross-node traversal, ordered queries, node/candidate charges and stop behavior keep their original source paths. No hint, new factor, or runtime-tree edit.

The [Release callsites](self-pair-calls.csv) confirm two calls from unordered `selfPairs` to the [five-byte tail-jump helper](helper-tail.asm), which jumps to the existing `pairQualifies`. The unordered `selfPairs` body shrinks from 1,956 to 1,267 bytes. `selfPairs` and `crossPairs` keep their start offsets modulo 64, but `walkOrdered` moves from 0 to 16 modulo 64. This is not an instruction-layout-matched experiment; unchanged source does not imply identical generated query code. [Symbols](symbols.csv).

## Frozen phase and gate screen

Both library engines and unchanged baselines on the frozen `int32` snapshot slice, nine rounds control → candidate and 15 rounds candidate → control. CPU 6, quiet cold preflight. Launch Tctl differed by 0.125 °C and 0.375 °C within the two comparisons. Same fixtures, limits, and error term.

Counts are pass / fail / inconclusive. G0 cross-checks and G3/G4/G6 pass; each run has 68 warm allocation-free library rows, and matched uncapped checksums, work and memory agree.

| Rounds | G2 control → outline | G9 control → outline | G1 control → outline |
|---:|---:|---:|---:|
| 9 | 13 / 0 / 2 → 12 / 0 / 3 | 15 / 0 / 0 → 14 / 0 / 1 | 8 / 7 / 1 → 8 / 6 / 2 |
| 15 | 11 / 0 / 4 → 13 / 0 / 2 | 15 / 0 / 0 → 15 / 0 / 0 | 9 / 7 / 0 → 8 / 8 / 0 |

15-round `index`, times in ms/snapshot. Phase medians and full-step estimates are separate.

| Scene | Entries | Build median | Pair median | Full step | G2 outline (≤2×) |
|---|---:|---:|---:|---:|---|
| `split-line` | 10,000 | 0.437 → 0.439 | 0.724 → 0.687 | 1.189 → 1.184 | 1.952 [1.809–2.106], inconclusive |
| `uniform` | 100,000 | 5.474 → 5.370 | 7.370 → 6.930 | 13.163 → 12.391 | 1.913 [1.846–1.981], pass |
| `split-line-translated` | 100,000 | 6.050 → 5.998 | 7.018 → 6.693 | 13.406 → 13.089 | 1.965 [1.862–2.074], inconclusive |

- Unordered pair medians save 0.39–0.44 ms on `uniform`, 0.25–0.32 ms on translated, and 35–37 µs on `split-line` at 10,000. The `uniform` full-step gain resolves only at 15 rounds: ratio 0.941 [0.906–0.979], versus 0.969 [0.933–1.007] at nine. Both full-step intervals on the other two rows include no change. `split-line` at 10,000 moves from G2 pass to inconclusive at nine rounds; the 15-round gains in G2 pass count are `uniform` at 100,000 and translated at 10,000.
- Translated G9 still passes, but its outline upper bounds rise to 3.874× and 3.867×. At nine rounds the `mixed-size` upper bound moves from 3.996× to 4.018×. Ordered `split-line` at 100,000 regresses 6.2% at 15 rounds (1.062 [1.001–1.126]); individual 10,000-entry ordered rows regress in only one comparison each. Ordered source is unchanged, so this does not isolate the cause.
- At 15 rounds, the `uniform` empty G1 row goes from pass to fail while the matched brute force baseline drops from 7.7 to 5.0 ns; the `index` interval spans no change. Some small ordered rows (`clustered` at 4 and 8, `uniform` at 8) improve in both comparisons. None is evidence that the outline fixes the source-radix small-row regressions.
- Compared with the previous round-matched measurements of the exact control ELF, `--diff` shows 190/196 (96.9%) and 186/196 (94.9%) timed rows within error. The 15-round comparison narrowly misses the 95% target; do not treat a one-run gate change as durable. [Nine-round diff](frozen/9-identical-control-diff.txt) · [15-round diff](frozen/15-identical-control-diff.txt).
- [G2 phases and work](frozen/g2-comparison.csv) · [G9 phases and ratios](frozen/g9-comparison.csv) · [Matched steps and intervals](frozen/step-change.csv) · [Gate counts](frozen/gate-counts.csv) · [Verification](frozen/verification.log). [Environment](frozen/environment.txt), [order and start temperature](frozen/run-order.csv), and raw samples and verdicts in [frozen/](frozen/).

## Correctness and reproduction

The [unordered](checks/unordered-prefix.log) and [ordered](checks/ordered-prefix.log) probes match exact budget, predicate and stop streams. The unordered probe covers default and custom predicates, snapshots and an update, both boundary modes, and instrumented and uninstrumented queries; it matches under [ASan/UBSan](checks/unordered-sanitized.log). [Source radix](checks/source-sanitized.log) matches exact full-width order and fallback streams, including under ASan/UBSan. [Dense slots and topology](checks/dense-order.log), [full/moved-set pair identities](checks/equivalence.log), [CTest 21/21](checks/tests.log), and [differential fuzzing](checks/fuzz.log) pass.

Reconstruct the [source-radix candidate](../g9-two-scatter-source-radix-2026-11-21/README.md#correctness-and-reproduction) as `WORK/control` and `WORK/control-build`, with pinned `WORK/box2d`. Dereference-copy the control source to `WORK/candidate` and apply `patch -p1 --fuzz=0 < candidate.patch`. Build `WORK/candidate-build` with GCC 16.2.1 Release, `QUADTREE_BUILD_TESTS=ON`, `QUADTREE_BUILD_BENCHMARKS=ON`, `QUADTREE_BENCH_NATIVE=OFF`, and `FETCHCONTENT_SOURCE_DIR_BOX2D=WORK/box2d` (`178495ba35695aefe9c321e569d2277f06db3b90`). The runner verifies [control source](../g9-two-scatter-source-radix-2026-11-21/candidate-inputs.sha256), [candidate source](candidate-inputs.sha256), [control ELF](control-executable.sha256) and [both `.text` hashes](text.sha256).

Run `bash verify.sh WORK NEW_CHECK_DIR` and the [callsite inspector](../g2-self-outline-2026-11-20/inspect.py) with `WORK RESULT_DIR`. Run `bash run.sh WORK NEW_OUTPUT_DIR`, then the [G2/G9 summarizer](../g2-cross-screen-2026-11-20/summarize.py) on the output. The runner pins CPU 6, records Tctl and frequency, and restores power settings.

## Handoff

**Profile the bulk classifier on the staged two-stream + source-radix control.** For unordered `uniform` and `split-line-translated` at 100,000 and `split-line` at 10,000, separate box gathers, geometry tests, histogram increments (`++bins[b]` in `planBulk`), two-stream scatter, and dense/topology commit in the actual Release code. Use a `.text`-matched symbol twin, `cycles:u` and IBS where useful, and an ordered comparison. Match fixture hashes, checksums, work and zero warm allocations. The [earlier scatter profile](../g2-scatter-cursors-2026-11-21/README.md) predates two-stream scatter; it cannot say whether the updated classifier now dominates or whether histogram increments are a dependency worth breaking. Do not retune prefetch or stack the outline without new evidence.

At unchanged relative uncertainty and baseline times, the fresh control runs need about 0.34–0.53 ms less unordered `uniform` time and 0.38–0.69 ms less translated time at 100,000 to clear G2. These are timing budgets, not predicted build gains. Rerun G2/G9 together after a resolved change; G1/G2 still block 08.