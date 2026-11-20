# 06 · Automate portability and installed-consumer checks

**Goal:** make the compiler, header, ABI and sanitizer checks reproducible. Start independently; installed-consumer CI requires 05. Requirements: `QT-NFR-05`, `QT-NFR-08`–`QT-NFR-10`, `QT-FIX-06`, `QT-FIX-12`, `QT-FIX-13`.

**Read only:** [top-level CMake](../../CMakeLists.txt) for demo flags and single-config detection; [tests/CMakeLists.txt](../../tests/CMakeLists.txt) for `index-test-noexcept`, mixed `_DEBUG`/`NDEBUG` translation units and death tests; [bench/CMakeLists.txt](../../bench/CMakeLists.txt) for `index-fuzz-quick`. Reuse those targets. Mixed macros do not imply mixed STL debug-iterator ABIs.

**Scope:** Linux only. MSVC checks and support claims are deferred until explicitly requested; they do not block 06 or release acceptance.

**Deliver:** Debug/Release jobs for GCC/libstdc++, Clang/libstdc++ and Clang/libc++; standalone public-header compile checks, strict warnings and exception-disabled runs. Cover ASan/UBSan, STL debug mode and concurrent/nested queries under TSan where supported. Fix build flags that prevent portability, including configuration detection on multi-config generators. Keep native/LTO/linker choices opt-in and private. Run the relocated consumer from 05.

**Exit:** publish a passing job matrix with the compilers, flags and CTest commands visible. Record unsupported sanitizer/platform combinations explicitly. No frozen timing verdicts on shared CI runners; no legacy arithmetic redesign here.
