# 03 · Attribute the direct-payload regression

**The regression is in dense commit, not bulk planning or topology.** On the two unordered blockers, the direct load removes constructor and lookahead cycles but adds more cycles at the current-entry gather/callsite. Keep the descriptor-pointer path; no runtime change or gate result.

Two reversed quiet-start blocks, CPU 6, staged Release control versus the [rejected direct-payload candidate](../g2-direct-payload-2026-11-21/README.md). Each executable has a byte- and layout-matched `.text` symbol twin. Twelve `cycles:u` captures cover both unordered blockers and ordered translated at 100,000; four IBS captures inspect unordered translated. Block preflights ended at 39.750 and 40.000 °C. Fixtures within blocks ran back to back.

Candidate minus control, million sampled user-cycle periods per warm repetition. Periods divided by repetitions are approximate attribution, not unprofiled wall time. Dense aggregate includes constructor, lookahead, current gather/call, existing rank insertion and residual dense loop work.

| Fixture / engine | Dense, blocks 1 / 2 | Construct | Lookahead | Current gather/call | Total build |
|---|---:|---:|---:|---:|---:|
| `uniform` / `index` | +1.738 / +1.582 | −1.936 / −2.059 | −0.967 / −0.908 | +4.675 / +4.476 | +1.713 / +1.835 |
| translated / `index` | +2.126 / +1.771 | −1.864 / −2.242 | −1.193 / −1.321 | +4.978 / +5.261 | +1.920 / +1.847 |
| translated / `index-ordered` | +0.400 / +0.651 | −8.534 / −8.632 | +0.031 / 0 | +5.310 / +5.245 | +0.713 / +0.695 |

- Some construction cost moves by definition: the [candidate ELF](../g2-direct-payload-2026-11-21/direct-load.asm) loads the value in the caller and passes it in `r9d`. Compare the whole dense region, not the constructor in isolation. On unordered rows planning changes by at most +0.23 million cycles/rep and topology by at most 0.11 million in either direction. The dense increase agrees in sign with the separate pilot's build regression.
- Unordered translated IBS at the exact payload-load PCs reports 19/25 L1 for the old dependent load and 37/44 L1 for the direct load. These are sparse labelled samples, not total accesses or causal latency measurements. The address multiply, scheduling, and code-layout effects are not separated. On ordered translated the rank-insertion attribution also increases; ordered pair periods move without a query source change. Do not infer a rank or traversal regression mechanism from this profile.
- [Cycle changes](cycle-change.csv) · [Periods and reps](cycle-periods.csv) · [Hot PCs](hot-pcs.csv) · [IBS labels](ibs-dense.csv) · [Verification](verification.log): 16 captures match pilot fixtures, checksums, and work; zero warm allocations. No samples reported lost.

## Reproduction

Use the [direct-payload screen's work tree](../g2-direct-payload-2026-11-21/README.md#correctness-and-reproduction). Run `bash profile.sh WORK CAPTURE_DIR`, then `python3 summarize.py CAPTURE_DIR RESULT_DIR`. For offline analysis, unpack [raw callchains](raw-callchains.tar.gz), copy [run order](run-order.csv) and the [control](control.elf-path.txt) / [candidate](candidate.elf-path.txt) DSO paths, and provide the two verified symbol twins. [Text matching](text-check.log) · [Environment](environment.txt). Power settings restored.

## Handoff

**Screen a clean bulk-leaf refit fast path on the unchanged two-stream + source-radix control.** At 10,000 `split-line`, prior [commit attribution](../g2-commit-breakdown-2026-11-21/README.md) put refit at ~5% of unordered build cycles (all nodes, not only leaves). `materializeBulk` initializes fresh nodes and computes their own summaries before calling `refit`; a leaf can publish the same own count/bounds/filter directly. Preserve parent and mutation refits, empty-node sentinels, split retry, reference links, and sorted fallback. Verify exact state through a mutation and check both engines. This targets the smaller blocker; all leaf refit work is unlikely to close the full 100,000-entry gap. Do not stack or tune direct payload addressing.
