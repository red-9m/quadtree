# 02 · Two-entry direct pair result

**Not retained; nothing staged.** Writing tiny-path counters straight into the returned `QueryResult` saved 2.1–3.4 ns/scene for two entries and 5.5–6.0 ns/scene for four. Every two-entry row still fails G1 (point ratios 2.375–2.581). Identical ordered code also moved by up to 4.7 ns/scene faster and 14.9 ns/scene slower. A layout effect of that size can mask the two-entry gain, so this run does not attribute it to removing the copy alone. Staged G1 stays 8/16; Task 02 remains open and still blocks 08.

## Candidate and preflight

[Patch](direct.patch), applied in a scratch copy of the exact packed cross-item control:

- `Context` becomes `QueryContext<QueryResult>` through an alias.
- `QueryContext<QueryResult&>` counts into a caller-owned result.
- In unordered `forEachPair`, the existing tiny branch (≤4 entries, no root, budget covers every pair) returns `tinyPairResult(...)`. That helper has one named `QueryResult`, one context bound to it, and one `return`.

Other branches, the public API, the build path, pair orientation, budgets, filters, predicates, and Stop handling are unchanged. The bench adapter, runner, fixtures, brute baseline, and gate limits are also unchanged. This differs from the [historical direct result](../g1-direct-pair-result-2026-11-19/README.md): the result stays in a separate helper instead of an inline block, and an alias replaces the derived `Context`.

- **Copy removed:** In control, the unordered adapter copies the 48-byte owned result on the stack (`0x60→0xe0`, three `movdqa`/`movaps` pairs) before building `bench::Counters`. In the candidate, the tiny path jumps straight to the counter stores, and the status is a constant `Ok`. Because the result no longer escapes, GCC also drops the dead `filterRejections` and `reports` increments the adapter never reads. The non-tiny `selfPairs` branch still copies. Evidence: [control](control-dispatch.asm.gz) and [candidate](candidate-dispatch.asm.gz) assembly.
- **Ordered code unchanged:** ordered `LibraryIndex::pairs` (506 instructions) and the ordered `Runner::step` (668) match after normalizing branch/RIP displacements and renaming `QueryContext<QueryResult>` to `Context`. The same holds for all 28 `Index<unsigned, int, Unchecked, true>` member symbols. The only differences are the intended unordered adapter (350 → 347) and one alignment nop in `cut`. `PersistentRunner<…, true>::pass` changes one `std::vector` inlining decision; it is not on the G1 path. Evidence: [codegen](codegen.log) and [callee check](codegen-callees.log).
- **Semantics:** a [deterministic transcript](semantics-probe.cpp) covers 800 flat 0–5-entry indexes, both Instrumented settings, snapshot and persistent builds, both boundaries, and random filters. Each index runs every combination of four budgets (unlimited, exact, one short, zero), Stop after 0, 1, or 2 reports, and three predicate modes. Control and candidate transcripts are byte-identical across 29,600 runs: status counts Ok/Stopped/BudgetExhausted = 19,288/1,975/7,537. All five counters and oriented payload pairs also match. The candidate is clean under ASan and UBSan. [Log](semantics-probe.log).
- **Exact pairs:** the unchanged-adapter [checker](../g1-flat-input-2026-11-19/phase.cpp) matches brute on all 256 fixtures for every one of the 16 rows on both binaries, with zero warm allocations. [Log](exact-pairs.log); its timings are not G1 evidence.

## Matched G1 results

Frozen G1 workload on CPU 6, HPET, `performance`, quiet starts ≤40 °C and within 1 °C per pair. Order: 81 rounds control → candidate, then 117 rounds candidate → control. Both `.text` hashes match [text.sha256](text.sha256); the control ELF matches the staged control hash. All 96 rows match between binaries in each run: pair counts, checksums, fixture hashes, candidates, node visits, memory, and allocations. Both library engines allocate zero when warm. [Verification](diagnostic/verification.log) · [all 16 rows per run](diagnostic/g1-comparison.csv) · [library A/B, n=0–8](diagnostic/library-change.csv) · [raw samples](diagnostic/).

| Run | Control G1 pass / fail / inconclusive | Candidate G1 pass / fail / inconclusive |
|---:|---:|---:|
| 81 | 8 / 7 / 1 | 8 / 6 / 2 |
| 117 | 8 / 8 / 0 | 8 / 6 / 2 |

Rows 8–64 pass in both scenes on both binaries. The candidate's empty rows move from fail to inconclusive, with unresolved absolute change (≤0.4 ns). The 1-, 2- and 4-entry rows fail in every run.

Two-entry unordered `index`, full step ns/scene (A/B is a log-ratio 95% interval):

| Rounds | Scene | Control → candidate | Saved | A/B interval | Candidate G1 [95%] | Point gap to 2× brute |
|---:|---|---:|---:|---|---|---:|
| 81 | `clustered` | 33.6 → 30.4 | 3.2 | 0.879–0.931 faster | 2.375 [2.303–2.449] FAIL | 4.8 |
| 81 | `uniform` | 34.1 → 32.0 | 2.1 | 0.876–1.005 unresolved | 2.581 [2.406–2.768] FAIL | 7.2 |
| 117 | `clustered` | 33.7 → 30.3 | 3.4 | 0.874–0.925 faster | 2.484 [2.421–2.548] FAIL | 5.9 |
| 117 | `uniform` | 34.1 → 30.7 | 3.4 | 0.875–0.926 faster | 2.558 [2.493–2.625] FAIL | 6.7 |

Four-entry unordered `index` gets faster in all four comparisons: 56.7→50.7 and 56.4→50.9 (`clustered`), 60.3→54.8 and 60.9→54.9 (`uniform`).

Ordered `index-ordered` timing, although its code is unchanged:

| Rows | Result |
|---|---|
| n=2 | 4.0–4.7 ns/scene faster in all four comparisons (resolved) |
| `clustered` n=4 | −0.5 and +1.9 ns, unresolved |
| `uniform` n=4 | +2.1 and +1.5 ns, unresolved |
| `clustered` n=8 | 14.9 and 11.3 ns/scene slower (resolved in both runs) |
| `uniform` n=8 | 7.6 and 6.2 ns/scene slower, unresolved |

Every G1 row from 8 to 64 still passes.

## Interpretation

- The copy removal is real in the assembly. Its two-entry gain (≤3.4 ns) is smaller than the swings in ordered code that did not change (up to 4.7 ns faster and 14.9 ns slower). Placement alone moves small rows by that much, so the unordered gain is an upper-bound lead, not a clean attribution.
- Even taken at face value, two entries still need another 4.8–7.2 ns/scene at the point estimate, and more to clear the upper bound. The [invalid warm-copy probe](../g1-two-copy-headroom-2026-11-21/README.md) saved 9–10 ns but omitted contractual work. The two savings were never measured together and must not be added.
- Fixing the resolved ordered `clustered` n=8 slowdown would require layout control. Shipping this patch as-is risks that regression without gaining a G1 row.

## Reproduction

`WORK=/tmp/quadtree-g1-direct-two.current` (contains the work-directory path). Link `WORK/control` and `WORK/control-build` to the exact [packed cross-item tree and Release build](../g2-packed-cross-item-2026-11-21/README.md#checks-and-reproduction), and link `WORK/box2d`. Copy `control` to `WORK/candidate` and apply `direct.patch`, verifying [candidate-inputs.sha256](candidate-inputs.sha256). Then run, in order:

1. `bash build.sh WORK`
2. `python3 ../g1-unordered-empty-2026-11-21/codegen.py WORK . --unordered`
3. `python3 codegen-names.py .`
4. `python3 codegen-callees.py WORK`
5. `bash run.sh WORK diagnostic`
6. `python3 summarize.py diagnostic`

Compile `semantics-probe.cpp` with `c++ -std=gnu++20 -O3 -DNDEBUG -IWORK/VARIANT`, and also with `-O1 -fsanitize=address,undefined`. Build `phase.cpp` per the [exact-pair recipe](../g1-tiny-store-fusion-2026-11-19/README.md#reproduction) and run all 16 rows for each binary.

## Handoff

**Follow-up done:** in the [layout control](../g1-layout-control-2026-11-21/README.md), the unordered two-entry saving persists (3.3–4.7 ns) and the ordered two-entry shift disappears.

**Do not stage the direct result alone,** and do not stack it on the invalid copy probe. Next, separate layout from code before the next two-entry design. Run one controlled layout check of the same control/candidate pair, for example with `-falign-functions=64` on both or by swapping function order, and see whether the ordered n=2 and n=8 shifts and the unordered two-entry saving persist. Only a saving that survives layout control counts as evidence. A credible two-entry design still needs about 5–7 ns/scene more, from validated build plus pair/result work combined.
