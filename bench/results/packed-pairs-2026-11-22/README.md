# 09 (reopened) · Packed pair tests and float capture: candidate

**Valid candidate, not yet staged.** [packed-pairs.patch](packed-pairs.patch) against `d52dbe1` reduces the open non-`int32` G2 rows to `int64` 1 fail and `double` 1 fail, plus 4 rows unresolved within 1% of their limit. `float` has no G2 failure left. `int32` passes every hard gate within the accepted exceptions, now without needing either G2 exception row. G9 has no failure for any type. Work counts are unchanged.

## Change (`detail/quadtree-index-impl.h`)

1. **Float capture.** `buildPrepared`'s capture of a float `Box` input uses the branch-free `canonicalBox`, which has the same acceptance and result as `detail::canonicalize`. Previously the build called `canonicalize<float>` out of line per entry. The warm flat build shares the helper.
2. **Packed overlap test in more traversals.** The packed overlap test, previously used only for cross-node item pairs, now also covers:
   - same-node item pairs (`selfPairs`);
   - holder items against subtree entries (`itemsVsSubtree`);
   - ordered self-pair candidates (`orderedSelf`).

   The packed test exists for `int32`, `float` and `double`; `int64` falls back to `detail::overlaps`. With a custom predicate the generic test is still used.

Screens ([screens/](screens/), `compare.py`):

| Variant | Result |
|---|---|
| change 1 alone | `double` −3% to −13%, `float` 10² −9% to −13% |
| + packed `selfPairs`/`itemsVsSubtree` | `int32` −4% to −11%, `float` −4% to −15% |
| `splitThreshold` 12/16 (rejected) | mixed |
| `c4`: packed node-bound tests in `walk`/`walkOrdered`/`crossPairs` (rejected) | unordered faster, ordered at 10⁵ unchanged, so G9 reaches 4.0–4.2 |

The first matched run of change 1 + packed `selfPairs`/`itemsVsSubtree` ([data-c2/](data-c2/)) failed G9 at 10⁵ (`float` 3, `double` 1). Faster unordered pairs raise the ordered/unordered ratio. Adding the packed ordered test (`c3`, this candidate) fixes that; the [rule amendment](resolution-rule.md) records it.

## Verification ([checks-c3/](checks-c3/))

- **Equivalence probe, all four types** (12–13 cases each): topology, handles, ranks and exact ordered and unordered pair sequences match `d52dbe1`. `int32`/`int64` dense-order streams are exact.
- **Budget contract:** ordered prefix and unordered subset hold for all four types, and under ASan/UBSan for floats.
- **Small-float probe:** byte-identical to control.
- **CTest:** 21/21. **Full fuzz:** passes. `.text` hashes in [text.sha256](text.sha256).

## Matched run ([data/](data/), [run.sh](run.sh), [rule](resolution-rule.md))

The control is the `d52dbe1` release binary. Order: control → candidate per type, 5 rounds. Every inconclusive candidate row was then rerun in two 45-round processes and pooled ([resolve.py](resolve.py), [final-status.txt](data/final-status.txt)). Every run exits 0, with identical checksums, candidates and node visits.

| Type | `--diff` geomean | Final G2 (20) | Final G9 (15) | G1 0–4 entries |
|---|---:|---|---|---|
| `int32` | 0.988 | **20 pass** | 15 pass | accepted exception (≤ 3.29 upper) |
| `int64` | 0.997 | 18 pass, 1 fail (`uniform` 10⁵ 2.13), 1 inconclusive (`split-line-translated` 10² 4.03 [3.95–4.11]) | 15 pass | fail, ≤ 3.10 upper |
| `float` | 0.955 | 19 pass, 1 inconclusive (`uniform` 10³ 4.02 [3.94–4.11]) | 14 pass, 1 inconclusive (`split-line-translated` 10⁵ 4.01) | fail or inconclusive, ≤ 3.45 upper |
| `double` | 0.957 | 17 pass, 1 fail (`uniform` 10³ 4.18 [4.09–4.27]), 2 inconclusive (`split-line-translated` 10³ 3.97, `uniform` 10⁵ 2.00) | 14 pass, 1 inconclusive (3.98) | fail, ≤ 3.68 upper (`uniform` n=1) |

G3, G4 and G6 pass for every type. Against the `d52dbe1` staged recheck, `float` drops from 8 G2 fails to 0, `double` from 8 to 1, and `int64` stays at 1.

## Handoff

Stage it like `d52dbe1`: commit, CI, then a full four-type recheck (snapshot and persistent) under a pre-specified pooled rule, including `int32` acceptance. Remaining after that, all within about 7% of their limits:
- `int64` `uniform` 10⁵ (box size);
- `double` `uniform` 10³;
- a few rows within 1% of their limits;
- G1 0–4 entries for the three non-`int32` types.
