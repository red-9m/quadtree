# Pre-specified resolution for the staged recheck (written before any resolve stage ran)

Rows left inconclusive by the 5-round matrices are rerun in separate 45-round processes using identical arguments:
- `int32-resolve` and `int32-resolve-2`: `--scenes=split-line-translated,uniform --sizes=100,1000,100000`.
- `int64-resolve`: `--scenes=split-line,split-line-translated,uniform --sizes=100,1000,10000,100000`.

The verdict of `int32-resolve` is final for each row, except G2 `int32` `split-line-translated` 10⁵. If that row is still inconclusive, its verdict comes from pooling `int32-resolve` and `int32-resolve-2`, using the formula of [the release pooling rule](../release-int32-2026-11-21/pooling-rule.md): geometric-mean ratio, pooled half-width sqrt(Σ h_i²)/k with k = 2 processes. Release-run processes are not pooled here, because they used a different binary. `int64` rows use the `int64-resolve` verdict only; this staged recheck does not decide `int64` exceptions.
