#include <quadtree-index.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <utility>

using ColliderId = int;
struct Tile { int id; };
using Pair = std::pair<ColliderId, int>;

int fail(const char* operation, QuadTree::Status status)
{
    std::fprintf(stderr, "%s failed (status %d)\n", operation, static_cast<int>(status));
    return 1;
}

int main()
{
    using QuadTree::Box;
    using QuadTree::Status;
    using Dynamic = QuadTree::Index<ColliderId, int>;
    using Static = QuadTree::Index<Tile, int>;
    using TileEntry = QuadTree::BuildEntry<Tile, int>;

    auto made_dynamic = Dynamic::create();
    if (!made_dynamic.ok()) return fail("create dynamic index", made_dynamic.status);
    auto dynamic_index = std::move(made_dynamic).value();

    auto made_static = Static::create();
    if (!made_static.ok()) return fail("create static index", made_static.status);
    auto static_index = std::move(made_static).value();
    const std::array<TileEntry, 3> tiles{{
        {{0, 0, 3, 3}, {7}},
        {{9, 0, 12, 3}, {9}},
        {{11, 0, 14, 3}, {11}},
    }};
    const auto built = static_index.build(tiles);
    if (built != Status::Ok) return fail("build static tile index", built);

    const auto first = dynamic_index.insert(Box<int>{0, 0, 2, 2}, ColliderId{101});
    if (!first.ok()) return fail("insert collider 101", first.status);
    const auto second = dynamic_index.insert(Box<int>{1, 1, 3, 3}, ColliderId{102});
    if (!second.ok()) return fail("insert collider 102", second.status);

    // Use the handle for mutation, not the stable application ID (101).
    const auto updated = dynamic_index.update(first.handle, Box<int>{8, 0, 11, 2});
    if (updated != Status::Ok) return fail("move collider 101", updated);

    // Stage AABB candidates without allocating in the visitor or changing a cache.
    constexpr std::array<Pair, 2> expected{{{101, 9}, {102, 7}}};
    std::array<Pair, expected.size()> staged{};
    std::size_t count = 0;
    const auto result = dynamic_index.forEachPair(static_index,
        [&](const auto& moving, const auto& tile) noexcept
        {
            if (count == staged.size()) return QuadTree::Stop;
            staged[count++] = {moving.payload, tile.payload.id};
            return QuadTree::Continue;
        });
    if (!result.complete()) return fail("enumerate cross pairs; discard staged output", result.status);

    // Check exact left/right identities, independent of traversal order.
    std::sort(staged.begin(), staged.end());
    if (count != expected.size() || staged != expected)
    {
        std::fputs("Unexpected dynamic/static pairs\n", stderr);
        return 1;
    }
    // Only completed output can replace the previous broad-phase candidate set.
    for (const auto& [collider_id, tile_id] : staged)
        std::printf("dynamic %d -> static %d\n", collider_id, tile_id);
}
