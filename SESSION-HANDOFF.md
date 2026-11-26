# Handoff: validate the quadtree-index release candidate

## Repo state

- Branch `quadtree-index` in `/home/dima/src/quadtree.git`; tested runtime at `c13e498`, with a docs-only handoff commit on top. Local commits, not pushed. Leave untracked `build-review/` alone.
- `ec67d81`: [four-entry flat snapshots](bench/results/g1-four-entry-flat-2026-11-26/README.md). Against `95c9178`, G1 `n=4` went from 0/48 to 48/48 passes, worst upper bound 1.939× brute. No warm allocations. `n=8–64` passed 192/192 in those screens.
- `c13e498`: [compact ordered traversal for large `int32` queries](bench/results/g9-int32-ordered-common-2026-11-26/README.md). `int32` G9 passes 15/15 in the default snapshot matrix; `split-line-translated n=100000` is 3.470 [3.357–3.586] vs 4.212 [4.069–4.360] at `ec67d81`. All three independent 45-round target runs pass (upper ≤ 3.475×). G2/G3/G4/G6 pass in that matrix. Matched work and checksums agree; zero warm allocations.
- Latest correctness: CTest 22/22; Clang `index-test` default, `-U__SSE2__ -fno-exceptions`, and ASan+UBSan pass. The 64-step `int32` persistent slice passes 24/24 hard gates on both binaries. This is **not** full release sign-off.

## Next: Task 08 release validation

Start with the [Task 08 brief](docs/plan/08-final-acceptance.md), prior [acceptance protocol](bench/results/int64-threaded-acceptance/README.md#release-validation) and [frozen limits](bench/MANIFEST.md#frozen-acceptance-limits). The old runner in [`packed-staged-2026-11-23`](bench/results/packed-staged-2026-11-23/run.sh) checks an old `.text` hash and Box2D checkout: use it as a template, not unchanged.

1. Clean archive build of `c13e498`, Release GCC 16.2.1/CMake 4.4.2, `-DQUADTREE_BUILD_BENCHMARKS=ON`, native tuning off. Pin Box2D to the archive SHA-256 in [`bench/CMakeLists.txt`](bench/CMakeLists.txt). Check the candidate `.text` hash below and rerun CTest. Use CPU 6 with sibling 7 idle, `powersave`, EPP and platform profile `performance`, HPET. Use quiet starts and record thermal/power settings.
2. Full snapshot and 64-step persistent matrices; use the prior [run coverage](bench/results/packed-staged-2026-11-23/README.md#method) and existing engines, fixtures, caps, and process errors. Include all coordinate types; timing sign-off is `int32` only, but correctness, allocations and portability apply to all four. Retain raw samples, gates, work/memory, rebuild peaks and scope reconciliation. Confirm all 15 `int32` G9 rows, not just the translated target.
3. Define the inconclusive-row repeat rule before running. Publish original failures/inconclusives and repeats separately. Apply only the existing owner-approved `int32` G1/G2 exceptions in the brief; no G9 exception. Run hosted portability CI for the exact candidate before sign-off. Do not update historical sign-off to cover the candidate until the required checks pass.

## Watch items

- `float clustered n=2`: same [4 KiB page-crossing adapter store](bench/results/g1-two-entry-inline-2026-11-26/README.md#limits-float-clustered-n2-in-the-full-screen). In G9 regressions, 22-character output dirs reproduce an inconclusive on both binaries; 81-character dirs pass 64/64 G1 rows on both. Use matched path lengths; don't modify adapters or count an inconclusive as a pass. This is the remaining G1 investigation if optimizing further under Task 02, not an `int32` timing blocker.
- `int32 uniform n=4`: candidate default matrix upper 2.041 (inconclusive); two 81-round follow-ups pass (upper 1.783/1.823). Recheck on the release matrix and record the original verdict.
- `int64 split-line-translated n=100000` G2: thin margin (upper 1.950/1.983 in `ec67d81` follow-ups). Time it in the release matrix; no `int64` exception has been granted.
- The G9 fast path uses existing reserved scratch, a shared filter-bit proof, narrow signed gaps and compact DFS when node entries are contiguous. Keep ordered prefixes, Stop/counters, fallback and zero warm allocations if further work touches it (`detail/quadtree-index-impl.h`, `orderedCommon` / `orderedSelf`).

## Reproduce and provenance

| Runtime | Release `quadtree-bench` SHA-256 | Local executable (ephemeral) |
|---|---|---|
| `95c9178` | `546d79c7035b2f5e1f0639c09b3ebb367f40a007bbc79d79ddd005ffc8fcd82b` | `/tmp/g14/control` |
| `ec67d81` | `2090617b229d379ff82cd4ae929aa026598e33c801a69de3fe8222203ba960e2` | `/tmp/g9t/control` |
| `c13e498` | `f7d8968a68ac97be29a7d28626dfaa9874069c2d66c6392c9f0a31cceaf7394c` | `/tmp/g9t/candidate` |

`c13e498` benchmark `.text` SHA-256: `369ce16eb6696e9d58748079dbac65aa920b9afdca9bbfce9a929e0f67129204`. Runtime header SHA-256: `0264f71b739185170ffc47bd5ce7d475d8a260c2ce85ee18ef107962c056b58a`. See the reports above for archived measurements and `reproduce.sh`/`regress.sh`; `/tmp/g14` and `/tmp/g9t` can be recreated from the tested commits. Update [`QUADTREE-UPDATE-PLAN.md`](QUADTREE-UPDATE-PLAN.md) and this file after validation.
