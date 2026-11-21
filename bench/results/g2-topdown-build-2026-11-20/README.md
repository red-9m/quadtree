# 03 · Top-down bulk partition

**Not retained.** The top-down candidate cut full-step time by 26–31% on the six failing G2 rows. The 15-round repeat still has five failures and one inconclusive G2 row, and adds two G9 failures. Runtime source is unchanged.

## Candidate

The [combined patch](candidate.patch) extends the [dense build and rank-sort candidate](../g2-dense-build-rank-2026-11-20/README.md). For nonflat `int32` builds, it classifies entries at each materialized split, alternates stable permutations in the two reserved key buffers, and records internal node boundaries during partitioning. Commit consumes that plan instead of generating full Morton keys, sorting them, counting topology, and rediscovering the cuts. A single occupied quadrant checks the existing Morton-prefix rule; plan scratch exhaustion falls back to the sorted loader. The fallback does not extend capacity, and the exact node check remains before commit. Other coordinate types and mutations still use the staged candidate's sorted loader or existing mutation path. No API thresholds, benchmark adapter, fixtures, or limits changed.

Dense slots are spatially grouped but no longer sorted by full Morton suffix within unsplit nodes. This changes unordered traversal order, not pair membership or ascending-slot orientation. Ordered full and moved-set output is unchanged.

## Frozen snapshot slice

Counts are pass / fail / inconclusive. G0, G3, G4 and G6 pass in all runs. G1 is unchanged at 8 / 6 / 2 throughout.

| Rounds | G2 control → candidate | G9 control → candidate |
|---:|---:|---:|
| 9 | 8 / 6 / 1 → 9 / 2 / 4 | 14 / 1 / 0 → 11 / 1 / 3 |
| 15 | 8 / 6 / 1 → 9 / 5 / 1 | 14 / 1 / 0 → 12 / 3 / 0 |

15-round repeat, `index`, milliseconds per snapshot. Steps are full-step estimates; build and pairs are phase medians.

| Scene | Entries | Step before → after | Build before → after | Pairs before → after | G2 after (limit 2×) |
|---|---:|---:|---:|---:|---:|
| `uniform` | 10,000 | 1.614 → 1.190 | 0.945 → 0.524 | 0.662 → 0.662 | 2.053 [1.991–2.118], inconclusive |
| `split-line` | 10,000 | 1.769 → 1.283 | 1.043 → 0.550 | 0.715 → 0.726 | 2.140 [2.069–2.213], fail |
| `split-line-translated` | 10,000 | 1.754 → 1.262 | 1.061 → 0.561 | 0.691 → 0.699 | 2.114 [2.038–2.193], fail |
| `uniform` | 100,000 | 20.837 → 15.101 | 13.595 → 7.564 | 7.152 → 7.310 | 2.346 [2.269–2.426], fail |
| `split-line` | 100,000 | 22.713 → 15.645 | 15.255 → 8.250 | 7.233 → 7.393 | 2.168 [2.090–2.249], fail |
| `split-line-translated` | 100,000 | 22.316 → 15.399 | 15.407 → 8.306 | 6.912 → 7.027 | 2.300 [2.217–2.386], fail |
| `clustered` | 100,000 | 28.583 → 22.314 | 14.273 → 8.096 | 14.396 → 14.141 | pass in both candidate runs |

- Build medians fell 44–48% on the six remaining G2 rows; unordered pair medians were flat or higher. The observed upper bounds require another roughly 6–18% full-step reduction if those bounds scale proportionally.
- Ordered full steps improved 17.8–20.1% at 100,000 on the three failing G2 scenes, less than unordered. `split-line` and `split-line-translated` at 100,000 became G9 failures: 4.164× [4.023–4.309] and 4.220× [4.081–4.362], respectively, versus 4×. The prior `mixed-size` failure remains at 4.446× [4.317–4.579].
- [Per-row gate inputs and verdicts](g2-comparison.csv) · [Matched step and phase changes](step-change.csv) · [9-round control](9-control/gates.txt) / [candidate](9-candidate/gates.txt) · [15-round control](15-control/gates.txt) / [candidate](15-candidate/gates.txt).
- [Exact probe](equivalence.log): identical topology, input-order handle mapping, normalized unordered pair sets and exact ordered streams for full and moved-set queries, including extreme and coincident integer boxes. [Full-slice verification](verification.log): all 68 library rows per run completed with zero warm allocations; all matching uncapped rows agree on checksums, counts, visits and footprint. [CTest: 21/21](tests.log), including exception-disabled and fuzz tests.

## Reproduction

Extract `57d30dcc49cff7de5563f46625b385cc145861cb` into two source directories, excluding `bench/results` and `docs`. Apply `candidate.patch` only to the candidate (`patch -p1`). Verify [control inputs](control-inputs.sha256) and [candidate inputs](candidate-inputs.sha256). Build both Release with GCC 16.2.1 (`-O3 -DNDEBUG`) and pinned Box2D `178495ba35695aefe9c321e569d2277f06db3b90` via `FETCHCONTENT_SOURCE_DIR_BOX2D`. Verify [`.text` hashes](text.sha256) with `objcopy --dump-section .text=NAME.text`, run `bash verify.sh CONTROL_SOURCE CANDIDATE_SOURCE` and run CTest in the candidate build.

Use the [frozen full-slice runner](../g1-flat-input-2026-11-19/reproduce.sh) with `taskset -c 6` and its `full` argument. Use the [recorded order](run-order.csv), [environment](environment.txt), and performance profile, then [restore the profile](restored-mode.txt). Gzip the raw CSVs and run the [summarizer](../g2-integer-key-2026-11-19/summarize.py) on the parent directory. Compare only matched runs, not absolute timings across earlier experiments.

**Handoff:** keep the top-down patch staged, not shipped. Profile the remaining ordered walk on the three G9 rows against this candidate; target repeated per-source traversal, not more rank-sort tuning. The two 100,000-entry G2 rows farthest from the limit (`uniform`, `split-line-translated`) also need a substantial unordered phase gain or further build traffic reduction. Retest both gates together. Release acceptance remains blocked.
