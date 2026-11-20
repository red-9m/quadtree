# 07 · Runtime quick start and example verification

**Done.** [README.md](../../README.md) leads with the runtime quick start and
installation instructions. The old API is documented separately in
[docs/legacy.md](../legacy.md).

The [dynamic/static example](../../examples/dynamic-static.cpp) checks exact
left/right pairs after moving a collider: `dynamic 101 -> static 9` and
`dynamic 102 -> static 7`. It stages candidate output and requires `complete()`
before publishing it. Pair order is normalized only for the example check.

## Verification

- The README example commands passed with GCC 16.2.1 Debug. The example also
  passed with Clang 22.1.8 / libc++ Release under Ninja Multi-Config and
  `-fno-exceptions`.
- A separate CMake app built and ran the same example using
  `find_package(QuadTree 2.0 CONFIG REQUIRED)` after relocating the install to
  a prefix with a space. The compiler used the relocated headers.
- `CXX=g++ bash ci/portability.sh check Release`: 16/16 checks (including
  `index-example-dynamic-static`) and 1/1 relocated consumer.
- Verified README paths and heading anchors for runtime usage, ownership,
  workspace, cache replacement, migration, legacy docs, and the G1/G2 report.
  No new performance verdicts.
