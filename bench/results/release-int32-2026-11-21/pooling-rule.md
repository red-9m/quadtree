# Pre-specified pooling rule for `split-line-translated` n=10⁵ (written before `resolve-2` ran)

After `resolve`, G2 `int32` `split-line-translated` n=100000 remained inconclusive: 1.954 [1.897–2.012]. The 2% between-process term dominates its interval, so more rounds in one process cannot resolve it. Per the manifest ("rerun with more rounds or processes"), the verdict comes from processes with identical arguments, all on the same `.text` `97d8835e…`:

1. `release-acceptance-2026-11-21/data/resolve-int32` (already run)
2. `release-int32-2026-11-21/data/resolve` (already run)
3. `release-int32-2026-11-21/data/resolve-2` (one new process, run after this file is written)

Pooled ratio = geometric mean of the three per-process ratios. Each process's log half-width h_i is taken from its gate interval. The pooled half-width is sqrt(Σ h_i²)/3, which treats the process errors as independent. Pass if the pooled upper bound is ≤ 2, fail if the pooled lower bound is > 2, otherwise inconclusive. The other rows of those runs are reported but not pooled. This rule does not change the frozen limits or any per-process verdict.
