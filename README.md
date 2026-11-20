# QuadTree

`QuadTree::Index` is a header-only C++20 spatial index for axis-aligned bounding
boxes (AABBs). Include `<quadtree-index.h>` and link the CMake target
`QuadTree::QuadTree`.

- **Snapshot:** use `build` when replacing a whole scene, such as a fresh frame
  or a batch of static tiles. A successful rebuild invalidates old handles.
- **Persistent:** use `insert`, `update`, and `remove` when only part of the scene
  changes. Keep each entry's handle for updates; use stable application collider
  IDs to identify objects across rebuilds.

## Run the dynamic/static example

[`examples/dynamic-static.cpp`](examples/dynamic-static.cpp) bulk-builds a static
tile index, moves a collider in a persistent dynamic index, then calls
`dynamic_index.forEachPair(static_index, ...)`. The visitor receives the dynamic
collider ID on the left and a different payload type (`Tile`) on the right.

```sh
cmake -S . -B build-example -DCMAKE_BUILD_TYPE=Debug -DQUADTREE_BUILD_DEMO=OFF -DQUADTREE_BUILD_BENCHMARKS=OFF -DQUADTREE_BUILD_TESTS=ON
cmake --build build-example --config Debug --target quadtree-dynamic-static
ctest --test-dir build-example -C Debug -V -R '^index-example-dynamic-static$'
```

The example checks and prints `dynamic 101 -> static 9` and
`dynamic 102 -> static 7`. These are current AABB overlap candidates, **not**
confirmed contacts or enter/exit events. Stage results and check `complete()`
before replacing a candidate cache: a stopped or budget-exhausted traversal may
already have called the visitor. For incremental cache updates, see the
[moved-set replacement protocol](INDEX.md#persistent-moved-sets-and-cache-replacement).

## Use in a project

With CMake 3.21+, use `add_subdirectory(path/to/quadtree)` for a checkout, or
`find_package(QuadTree 2.0 CONFIG REQUIRED)` for an installed copy. In either case:

```cmake
target_link_libraries(my_app PRIVATE QuadTree::QuadTree)
```

To install the runtime package from the checkout:

```sh
cmake -S . -B build-install -DQUADTREE_INSTALL=ON -DQUADTREE_BUILD_DEMO=OFF -DQUADTREE_BUILD_TESTS=OFF -DQUADTREE_BUILD_BENCHMARKS=OFF
cmake --install build-install --prefix "$PWD/quadtree-install"
```

Set `CMAKE_PREFIX_PATH` to that prefix when configuring your app. Without CMake,
put the directory containing `quadtree-index.h` on the include path and keep
`detail/quadtree-index-impl.h` beside it under `detail/`.

## Reference and limits

- [Runtime API and examples](INDEX.md) · [Build/input ownership](INDEX.md#build-and-input-ownership) · [Workspace and reservation](INDEX.md#reservation-workspace-and-allocation)
- [Measured limitations](bench/results/current-snapshot-2026-11-18/README.md): measured snapshot rows miss G1 and G2; release performance acceptance is not complete.
- [Migration guide](INDEX.md#migration) · [Legacy `Tree` documentation](docs/legacy.md). The legacy `quadtree.h` is a separate API, not a compatibility layer or part of the installed runtime package. Do not include both API headers in one translation unit.
