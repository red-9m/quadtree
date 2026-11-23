# Pre-specified run and resolution rule (written before the matched run)

The control is the `d52dbe1` release binary (`.text` `9040f7fc…`); the candidate is `packed-pairs.patch`. The 5-round snapshot matrices alternate control and candidate per type: float, double, int32, int64 ([run.sh](run.sh)).

Every candidate hard snapshot row that is inconclusive in its 5-round matrix is rerun in two separate 45-round candidate processes, `candidate-<type>-resolve` and `candidate-<type>-resolve-2`, with identical arguments. The verdict is the pooled interval: geometric-mean ratio, half-width sqrt(h₁² + h₂²)/2. The same pooled interval applies the accepted `int32` exception caps: G1 0–4 entries ≤ 3.5, and G2 `split-line-translated` 10² / `uniform` 10³ ≤ 4.5. No exception covers the other types. Rows that pass or fail in the matrix keep that verdict.

**Amendment (before the second matched run).** The first matched run ([data-c2/](data-c2/)) showed that the candidate's faster unordered pairs raise G9 (ordered/unordered) at 10⁵: `float` 3 fail, `double` 1 fail, `int32` 2 inconclusive. The candidate therefore also uses the packed test for ordered self-pair candidates in `orderedSelf` (`c3`). This file's rule now applies to `c3` and its new matched run in `data/`. The `data-c2` run is kept as evidence and is not used for verdicts.
