# 06 · Portability CI evidence

**Implemented; hosted run pending.** The [Portability workflow](../../.github/workflows/portability.yml) has six Linux compiler/configuration jobs and three Linux instrumentation jobs. All nine passed locally with the Ubuntu 24.04 toolchain versions selected by the workflow. The workflow is still local; push the release worktree to a branch or open a PR, then link the green Actions run here before marking 06 done. MSVC is deferred until explicitly requested and is not an acceptance gate.

## Ubuntu 24.04 matrix rehearsal · 2026-11-20

Local rootless container, not GitHub Actions. [Compiler flags and CTest logs](portability-ubuntu24-2026-11-20/checks.log) · [Package versions and tested input hashes](portability-ubuntu24-2026-11-20/environment.txt).

| Toolchain | Debug: suite + consumer | Release: suite + consumer |
|---|---:|---:|
| GCC 13.3.0 / libstdc++ 13 | 16/16 + 1/1 | 16/16 + 1/1 |
| Clang 18.1.3 / libstdc++ 13 | 16/16 + 1/1 | 16/16 + 1/1 |
| Clang 18.1.3 / libc++ 18 | 16/16 + 1/1 | 16/16 + 1/1 |

- Clang ASan/UBSan Debug with leak checks: 16/16. GCC `_GLIBCXX_DEBUG` Debug: 16/16. Clang TSan Debug: 1/1 (concurrent and nested queries). The container used the local host kernel and did not exercise the workflow's hosted `sysctl` step.
- Every compiler job built the standalone headers, exception-disabled compile probe, and demo under strict warnings and ran the dynamic/static example, quick fuzzer, and relocated consumer.
- Rehearsal used `ubuntu:24.04` with `g++-13 clang-18 libc++-18-dev libc++abi-18-dev libclang-rt-18-dev cmake ninja-build` installed. Run the commands below in that image with the checkout mounted read-only. No timing verdicts.

## Earlier local verification · 2026-11-20

Linux x86-64, CMake/CTest 4.4.2, Ninja 1.13.2. Counts below are runtime CTest checks (including `index-fuzz-quick`) plus the relocated installed consumer. Each row also built the standalone headers, the exception-disabled compile probe, and the legacy demo with strict warnings.

| Toolchain | Generator | Debug: suite + consumer | Release: suite + consumer |
|---|---|---:|---:|
| GCC 16.2.1 / libstdc++ 16 | Ninja | 15/15 + 1/1 | 15/15 + 1/1 |
| Clang 22.1.8 / libstdc++ 16 | Ninja Multi-Config | 15/15 + 1/1 | 15/15 + 1/1 |
| Clang 22.1.8 / libc++ 22.1.8 (`-stdlib=libc++`) | Ninja | 15/15 + 1/1 | 15/15 + 1/1 |

- Clang ASan/UBSan Debug with leak checks: 15/15. GCC `_GLIBCXX_DEBUG` Debug: 15/15.
- Clang TSan Debug: 1/1 (`index-test`, including concurrent and nested ordered queries with distinct workspaces).
- Full GCC Release CTest with Box2D disabled: 20/20. The Ninja Multi-Config demo built and ran from a single build tree: `_DEBUG` only in Debug; tree and reference query counts agreed in both configurations. No timing verdicts.
- `actionlint` 1.7.12 and `bash -n ci/portability.sh` passed.

## Reproduce

Run [ci/portability.sh](../../ci/portability.sh) with `check Debug` or `check Release`. Set `CXX` to `g++-13` or `clang++-18`; add `CXXFLAGS=-stdlib=libc++` for libc++. Use `asan-ubsan Debug`, `stl-debug Debug`, or `tsan Debug` for the instrumentation jobs. The script prints verbose compiler commands and the exact CTest invocations; the relocated consumer runs from a separate source/build tree with package registries disabled and a prefix containing a space.

- GCC/Clang strict flags: `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -pedantic-errors -Werror`; exception-disabled targets add `-fno-exceptions`.
- Mixed `_DEBUG`/`NDEBUG` checks switch macros only around the runtime header; they do not mix STL iterator ABIs. `_GLIBCXX_DEBUG` is libstdc++-only and uniform across every TU of its job.
- Sanitizer coverage is Linux x86-64 with Clang/libstdc++; libc++ sanitizer combinations are not in this matrix. ASan and TSan run separately because their runtimes cannot be combined. The hosted LLVM 18 TSan job sets `vm.mmap_rnd_bits=28` to avoid shadow-map conflicts.
