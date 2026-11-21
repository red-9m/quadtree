# 02 · Empty and singleton full-step headroom

**The empty-only shortcut clears both empty G1 rows twice, but is not staged: ordered eight-entry steps regress 4.7–6.5%.** Skipping almost all warm singleton replacement saves 5.2–5.4 ns/scene, yet none of those synthetic intervals fit within 2×. Keep the existing control; task 02 remains open and blocks 08.

## 1. Empty-query shortcut: viable direction, ordered regression

[Empty probe](empty/probe.patch) gives the existing pair query a distinct `empty()` return and moves the nonempty path to an outlined helper. Unlike the [earlier zero-or-one wrapper](../g1-singleton-query-2026-11-19/README.md), the fast branch establishes zero size, not merely size below two. The helper retains the singleton `MissingRank` check and the same nontrivial paths. The adapter, runner, brute, fixtures, error term and limits are unchanged.

[Disassembly](boundaries.asm): control `LibraryIndex::pairs` separately stores the candidate and reference counts; the runner calls it and reloads the two adjacent words with one 16-byte load. The new wrapper lets the unordered adapter inline in the runner. There is no out-of-line adapter result transfer on empty; the remaining zero query-counter store/read is a matching-width SIMD pair, separate from the reference-count update. This proves code shape, not latency attribution; the full-step measurement includes other compiler changes.

- Both scenes pass at 81 and 117 rounds: 10.7–10.8 → **4.2–5.0 ns/scene**, saving 5.8–6.5 ns. All four matched full-step intervals resolve. The upper G1 bounds are 0.833–1.248×.
- G1 improves from 8/16 to **10/16** twice. Sizes 1, 2 and 4 still fail in both scenes. Unordered singleton also improves from 19.4–19.7 to **13.5–13.7 ns/scene**. Its 2.134–2.178× upper bounds imply another ~0.85–1.12 ns/scene with the same measured error and reference.
- Ordered size 8 slows **4.7–6.5%** in both scenes and rounds; ordered `clustered` size 4 slows 5.7–6.3% twice. Do not stage or use this as a G2/G9 control. [All 16 frozen G1 rows](screen/empty-probe-g1.csv) · [Both engines' matched timing intervals](screen/step-change.csv).

## 2. Singleton bypass: insufficient on its own, diagnostic only

[Invalid probe](singleton/probe.patch): inside the warm flat-build branch, return `Ok` when the old and new sizes are one, no handle output is requested, and the policy is Snapshot. It skips input validation, fresh owner, and publishing the new entry. It retains adapter writes, range/workspace/capacity checks, call/frame overhead and the original query/counter path. [Direct probe](probe-check.cpp) confirms stale extent despite the returned success. With one entry there is no pair, so the benchmark checksum cannot detect this violation. Never run release acceptance with this tree.

| Matched pair | Scene | `index` ns/scene before → bypass | Synthetic upper bound |
|---:|---|---:|---:|
| 81 | `clustered` | 19.8 → 14.4 | 2.328× |
| 81 | `uniform` | 19.5 → 14.2 | 2.208× |
| 117 | `clustered` | 19.5 → 14.2 | 2.221× |
| 117 | `uniform` | 19.4 → 14.2 | 2.221× |

All four bypass speedups resolve, but the matched 117-round gap is still ~1.4 ns/scene even with incorrect replacement. This is not a mathematical lower bound on replacement or a gate verdict. The two probe deltas cannot be added: their code generation and remaining work differ.

## 3. Decision

**No combined candidate or promotion.** Replacement alone did not demonstrate enough headroom, and the empty probe has a repeatable ordered cost. If continuing 02, first isolate the empty fast branch to unordered dispatch while preserving the existing ordered code path. Verify emitted code and paired ordered 4/8 steps; only then pursue the remaining ~1 ns singleton budget on that base. The still-failing 2/4 rows need separate savings. Keep the harness and public build/handle guarantees intact.

## Reproduction and evidence

Build two scratch work trees (`WORK/empty` and `WORK/singleton`); each has `control` and `control-build` linked to the [exact packed cross-item tree and Release build](../g2-packed-cross-item-2026-11-21/README.md#checks-and-reproduction), a link `box2d` to the pinned checkout, and a separate copy of the source as `candidate`. Apply the corresponding `probe.patch` only in that copy. Create `candidate-build/bench`, then run `bash build.sh WORK` (GCC 16.2.1 Release, benchmark flags and pinned control Box2D archive). `bash run.sh WORK OUT` verifies source and `.text` hashes and launches the 81/117-round G1 slice on CPU 6, control first then candidate first, with paired quiet starts within 1 °C and ≤40 °C. `python3 summarize.py OUT` generates the summaries.

The first singleton 117-round attempt never reached a matched control launch; its [abort record and unmatched raw candidate](screen/singleton/) are retained. The [cold retry](screen/singleton-retry/) reran both sides (30.500/31.375 °C) with `bash run.sh WORK OUT --retry-singleton-117`; only this pair enters the 117-round comparison. No measured matched round dropped. Power restored on every exit.

[Target ns/scene and intervals](screen/target-headroom.csv) · [Raw samples, run order, environment and gate logs](screen/) · [Work/allocation verification](screen/verification.log): 384 matched rows agree on measured checksums, fixtures, work and memory, with zero warm library allocations. [Focused check](probe-check.log) confirms empty and singleton status/ranks and two-entry budgets, orientation and Stop for the valid probe; the invalid lane proves the bypass is active. No runtime files changed.
