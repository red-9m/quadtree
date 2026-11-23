# Pre-specified resolution rule (written before any stage of this recheck ran)

The source is `7cfbebe`, benchmark `.text` `c01f6323…`. The 5-round matrices run first, for all four types, snapshot and persistent.

For each type, every hard snapshot gate row that is inconclusive in the 5-round matrix is rerun in **two** separate 45-round processes, `<type>-resolve` and `<type>-resolve-2`, with identical arguments. The rows to rerun are selected by scene and size from the matrix's inconclusive rows. The final verdict for such a row is the pooled interval over those two processes: geometric-mean ratio, half-width sqrt(h₁² + h₂²)/2, where h_i is each process's log half-width. Pass if the upper bound is ≤ the limit, fail if the lower bound is > the limit, otherwise inconclusive.

The same pooled interval is used for G1 0–4-entry rows to apply the accepted ≤ 3.5× exception cap, and for the two `int32` G2 exception rows (≤ 4.5× cap). Rows that pass or fail in the 5-round matrix keep that verdict. Exceptions apply only where the 08 brief grants them (`int32` G2 rows; G1 0–4 entries for the release type `int32`). This recheck does not grant new exceptions for `int64`/`float`/`double`.
