# 08 · Release-candidate source: promote the staged G2/G9 work

**Promoted and committed as `e73f6c5`.** The release candidate is the exact staged packed cross-item tree: two-stream scatter + source radix + packed cross-item eligibility, from [Task 03](../g2-packed-cross-item-confirm-2026-11-21/README.md). G2/G9 pass 15/15 on the measured `int32` slice only with this source; the committed `HEAD` (`57d30dc`) lacks it. No new algorithm work.

## What changed

The runtime and test sources were copied verbatim from the staged tree that produced the [45-round confirmation](../g2-packed-cross-item-confirm-2026-11-21/README.md):

- `detail/quadtree-index-impl.h`
- `tests/index-test.cpp`
- `tests/index-mixed-macros.h`
- the two `INDEX.md` lines describing `buildEntries` workspace contents (captured boxes plus descriptors).

The [Task 02 exception](../../../docs/plan/02-g1-small-snapshots-result.md) notes in `INDEX.md` and `bench/MANIFEST.md` remain. All other code, CMake and benchmark inputs were already identical.

## Verification

- **Provenance:** every code input listed in the staged [input hashes](../g2-packed-cross-item-2026-11-21/candidate-inputs.sha256) matches the working tree ([code-inputs.sha256](code-inputs.sha256)). It excludes only the two documentation files changed above.
- **Benchmark binary:** compiling the benchmark from this tree with the staged Release recipe gives `.text` byte-identical to the staged binary: `97d8835e1156f3781754862ad2b7b8a0d23622dc9a8fa811e2e93a6ac5f24433`, the frozen/control `.text` of the G1/G2 evidence. That evidence therefore describes this source exactly.
- **Release CTest:** [21/21](tests.log) (GCC 16.2.1, benchmarks/tests and pinned Box2D on, `QUADTREE_BENCH_NATIVE=OFF`), including the exception-disabled tests and quick fuzz.
- **Full differential fuzz:** [all types and scenes, seeds 1–40, 1500 steps](fuzz.log).
- **Local portability** (`ci/portability.sh`, Ninja), [log](portability-local.log):

  | Compiler | Mode | Result |
  |---|---|---|
  | GCC 16.2.1 | check Debug / Release | 16/16 + consumer 1/1 each |
  | GCC 16.2.1 | ASan/UBSan Debug | 16/16 |
  | GCC 16.2.1 | `_GLIBCXX_DEBUG` Debug | 16/16 |
  | Clang 22.1.8 | check Debug / Release | 16/16 + consumer 1/1 each |
  | Clang 22.1.8 | ASan/UBSan Debug | 16/16 |
  | Clang 22.1.8 | TSan Debug | 1/1 |

  Versions are in [environment.txt](environment.txt). Not rerun locally: libc++ and the older Ubuntu 24.04 toolchains.

## Still required before 08 sign-off

1. **Hosted CI:** push branch `quadtree-index` and rerun the [portability workflow](../../../.github/workflows/portability.yml) on that commit or later. The [06 evidence](../../../docs/plan/06-portability-result.md) covers `57d30dc`, which predates this source. This needs a push by the repository owner.
2. **Full 08 matrix:** run the calibrated full matrix on the committed revision, per the [08 brief](../../../docs/plan/08-final-acceptance.md).
