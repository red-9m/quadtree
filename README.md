# QuadTree

`QuadTree::Index` is a header-only C++20 spatial index for axis-aligned bounding
boxes (AABBs). Include `<quadtree-index.h>` and link the CMake target
`QuadTree::QuadTree`.

- **Snapshot:** use `build` when replacing a whole scene, such as a fresh frame
  or a batch of static tiles. A successful rebuild invalidates old handles.
- **Persistent:** use `insert`, `update`, and `remove` when only part of the scene
  changes. Keep each entry's handle for updates; use stable application collider
  IDs to identify objects across rebuilds.

## Per-step broadphase without dirty tracking

Rebuild from all boxed objects each step. Store each collider's stable ID as
its payload: a successful build invalidates the previous snapshot's handles.
Reserve index storage and build workspace before the loop, and give the
reusable candidate vector an initial **size**, not just capacity
([reservation guide](INDEX.md#reservation-workspace-and-allocation)).

```cpp
#include <quadtree-index.h>
#include <algorithm>
#include <vector>

using Broadphase = QuadTree::Index<std::uint64_t, std::int32_t>;
using Entry = QuadTree::BuildEntry<std::uint64_t, std::int32_t>;
using IdPair = std::pair<std::uint64_t, std::uint64_t>;

QuadTree::CollectResult collectCandidates(const Broadphase& index, std::span<IdPair> out) noexcept
{
    std::size_t count = 0;
    const auto result = index.forEachPair([&](const auto& a, const auto& b) noexcept {
        if (count == out.size())
            return QuadTree::Stop;
        out[count++] = {a.payload, b.payload};
        return QuadTree::Continue;
    });
    const auto status = result.status == QuadTree::Status::Stopped
        ? QuadTree::Status::InsufficientOutput : result.status;
    return {status, count, result.counters};
}

auto broadphaseStep(Broadphase& index, QuadTree::Workspace<std::int32_t>& workspace,
                    std::span<const Entry> entries, std::vector<IdPair>& candidates,
                    auto&& dispatch) -> QuadTree::Status
{
    const auto status = index.build(entries, {.workspace = &workspace});
    if (status != QuadTree::Status::Ok)
        return status;

    auto result = collectCandidates(index, candidates);
    while (result.status == QuadTree::Status::InsufficientOutput) {
        const auto size = candidates.size();
        const auto room = candidates.max_size() - size;
        if (room == 0)
            return result.status;
        candidates.resize(size + std::min(room, std::max(std::size_t{1}, size)));
        result = collectCandidates(index, candidates); // Retry without rebuilding.
    }
    if (!result.complete())
        return result.status;

    for (auto [a, b] : std::span(candidates).first(result.count))
        dispatch(a, b); // Outside the index: may allocate or throw.
    return QuadTree::Status::Ok;
}
```

If growth is disallowed, fail the step on overflow without dispatching a partial
list. If exact sizing matters, use a visitor that writes only while space remains
but increments the count and returns `Continue` for every pair. After a complete
pass, resize to that count and collect again if the original buffer was too small.
For handles from the current snapshot instead of IDs, use bounded
`pairsTo(std::span<Broadphase::HandlePair>)`.

`Boundary::Default` excludes edge-only overlap between positive-area boxes;
choose `TouchInclusive` to include it. These are AABB candidates, not confirmed
contacts. The restriction on `-ffast-math` applies to floating coordinates,
not `int32`. See the [small-world benchmark](bench/MANIFEST.md#small-world-broadphase-opt-in)
for build and pair traversal timings.

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
- [Performance scope](INDEX.md#runtime-index): `int32`, `int64`, `float` and `double` are performance-accepted (since `ce92ef5`). `int32` keeps an accepted very-small-snapshot exception, which the latest run did not need.
- [Migration guide](INDEX.md#migration) · [Legacy `Tree` documentation](docs/legacy.md). The legacy `quadtree.h` is a separate API, not a compatibility layer or part of the installed runtime package. Do not include both API headers in one translation unit.
