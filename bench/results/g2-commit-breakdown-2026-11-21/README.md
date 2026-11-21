# 03 · Dense and topology commit on two-stream + source radix

**Prioritize the payload address path at 100,000, not child initialization.** The unordered payload block is 6.7–8.0% of build cycles on the two large blockers, and the dense lookahead block is another 4.8–5.8%. At 10,000 `split-line`, own-range initialization and refit are larger than the payload block. No new runs, runtime changes, or gate verdicts.

## Reattribution of the staged Release captures

The [classifier report's 20 captures](../g2-bulk-classifier-2026-11-21/README.md) use the exact staged two-stream + source-radix control. Reuse the verified `.text`-matched debug twin. Split the original dense and topology buckets by audited construction PCs and DWARF commit callsites; the sums reconcile exactly in all 12 `cycles:u` captures. Dense lookahead includes the existing box, descriptor, and payload hints. Gather/call includes the current ID and address formation and the constructor callsite, not the called constructor. `own range` includes reference initialization and own bounds/filter summaries; `node init` includes block acquisition and child construction. Other periods remain in their original phase.

Percent of **sampled unordered build cycles**, ranges over the two blocks.

| Fixture | Payload block | Dense lookahead | Current gather/call | Own range | Node init | Refit |
|---|---:|---:|---:|---:|---:|---:|
| `uniform` 100,000 | 6.68–7.63 | 5.02–5.75 | 3.90–4.09 | 4.59–5.18 | 2.09–2.30 | 3.03–3.39 |
| `split-line` 10,000 | 2.36–2.50 | 2.73–2.91 | 4.03–4.68 | 6.38–6.52 | 2.91–2.92 | 5.04–5.08 |
| `split-line-translated` 100,000 | 6.73–8.02 | 4.82–5.66 | 2.45–2.57 | 4.73–4.91 | 1.88–2.45 | 3.18–3.68 |

- In `constructBuildEntry<int32>`, `0x147830` loads the payload pointer, `0x147839` loads the value through it, and the next instruction (`0x14783c`) loads the destination-array base. The latter alone receives 6.2–7.5% of build periods on the two large unordered rows. Dense lookahead's indirect payload hint (`0xd3317`) receives 3.1–4.4%. These are nearby sampled PCs, not latency measurements or proof that the destination-base load is the bottleneck. [Disassembly](dense-commit-instructions.asm).
- IBS at the actual dependent payload load records 109/132 unordered labelled LOAD samples hitting L1. The other 23 are outside L1. The adjacent destination-base load is L1 on 114/115 unordered LOAD samples. Ordered translated records only 12 labelled dependent-load samples (one L1); don't generalize from that small sample. No inference about bandwidth or misses per build.
- On ordered 100,000 rows, payload blocks account for 17.9–28.8% of build cycles, and rank insertion for 18.5–25.0%; the unranked payload lookahead path is absent. The assembly and two sizes motivate a focused experiment, not a common commit rewrite or another fuse-with-topology attempt.
- [Commit periods](commit-periods.csv) · [Hot PCs and inline locations](hot-pcs.csv) · [IBS commit labels](ibs-commit-sites.csv) · [Verification](verification.log). All reused rows match frozen fixtures, checksums and traversal counts, with zero warm allocations.

## Reproduction

Recover `CAPTURE_DIR` and the verified `staged.debug` twin as described in the [classifier report](../g2-bulk-classifier-2026-11-21/README.md#reproduction). Run `python3 summarize.py CAPTURE_DIR RESULT_DIR`. The script requires its recorded `.text` hash and reconciles against its original cycle totals.

## Handoff

**Screen a direct payload source for contiguous, non-projected `int32` bulk copy.** Start on two-stream + source radix. For eligible trivial payloads, use the already known logical input index to address the original contiguous `BuildEntry` payload directly at dense commit and for its existing lookahead, instead of loading the pointer from the captured descriptor. Leave descriptor geometry/filter/rank capture, prefetch distances, placement/handle order and topology alone. Preserve the descriptor-based path for consume, projection and noncontiguous inputs; test rollback, mixed ranks and exact dense/callback streams. Confirm the dependent load is actually gone in the Release ELF and screen both engines at both blocker sizes before any joint G2/G9 retest.

The existing unordered budgets at 100,000 are about 0.34–0.53 ms for `uniform` and 0.38–0.69 ms for translated. The measured blocks are not entirely removable, and the 10,000-entry blocker may need a different change. G1/G2 still block 08.
