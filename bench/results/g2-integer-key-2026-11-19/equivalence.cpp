// Diagnostic only. verify.sh adds a test accessor to temporary source copies.
#include "bench-index.h"

#include <array>
#include <bit>
#include <cstdlib>
#include <limits>

namespace
{
void check(bool ok)
{
    if (!ok)
        std::abort();
}

template<typename... T>
void emit(T... fields)
{
    check(((std::fwrite(&fields, sizeof(fields), 1, stdout) == 1) && ...));
}

template<typename T>
void emitBox(const QuadTree::Box<T>& box)
{
    emit(box.minX, box.minY, box.maxX, box.maxY);
}

template<typename T>
using Index = QuadTree::Index<std::uint32_t, T>;

template<typename T>
void emitKey(const Index<T>& tree, const QuadTree::Box<T>& root, std::uint32_t base, std::uint32_t id,
    const QuadTree::Box<T>& box)
{
    const auto key = tree.profileKey(root, base, id, box);
    emit(key.hi, key.lo, key.level, key.entry);
}

template<typename T>
void extremes()
{
    using Box = QuadTree::Box<T>;
    using U = std::make_unsigned_t<T>;
    constexpr auto lo = std::numeric_limits<T>::lowest();
    constexpr auto hi = std::numeric_limits<T>::max();
    auto made = Index<T>::create({.maxDepth = 64});
    check(made.ok());
    auto tree = std::move(made).value();
    for (const Box root : {Box{lo, lo, hi, hi}, Box{lo, 0, hi, 0}, Box{-9, -8, 8, 9}})
        for (std::uint32_t base : {0u, 1u, 31u, 32u, 63u, 64u})
        {
            for (const Box box : {root, Box{root.minX, root.minY, root.minX, root.minY},
                     Box{root.maxX, root.maxY, root.maxX, root.maxY}, Box{0, 0, 0, 0}, Box{-1, 0, 0, 0}})
                emitKey(tree, root, base, 0, box);
            for (std::uint32_t i = 0; i < 32768; ++i)
            {
                const T x = std::clamp(std::bit_cast<T>(static_cast<U>(bench::mix64(2ull * i))), root.minX, root.maxX);
                const T y = std::clamp(std::bit_cast<T>(static_cast<U>(bench::mix64(2ull * i + 1))), root.minY, root.maxY);
                const T right = x < root.maxX && (i % 3) == 0 ? static_cast<T>(x + 1) : x;
                const T bottom = y < root.maxY && (i % 5) == 0 ? static_cast<T>(y + 1) : y;
                emitKey(tree, root, base, i, {x, y, right, bottom});
            }
        }
}

void fixture(std::string_view scene, std::size_t n)
{
    using T = std::int32_t;
    const auto boxes = bench::convert<T>(bench::generate(scene, n, bench::sceneSeed(1, scene, n, 0)));
    bench::IndexResource memory;
    QuadTree::Workspace<T> workspace(&memory);
    check(workspace.reserve({n, 0, n}) == QuadTree::Status::Ok);
    auto made = Index<T>::create({.extent = QuadTree::Extent::Snapshot,
        .snapshotGrowth = QuadTree::SnapshotGrowth::Adaptive}, &memory);
    check(made.ok());
    auto tree = std::move(made).value();
    std::vector<QuadTree::BuildEntry<std::uint32_t, T>> input;
    auto root = boxes[0];
    for (std::uint32_t i = 0; i < n; ++i)
    {
        root = QuadTree::detail::unite(root, boxes[i]);
        input.push_back({boxes[i], i});
    }
    for (std::uint32_t i = 0; i < n; ++i)
        emitKey(tree, root, 0, i, boxes[i]);

    for (bool ordered : {false, true})
    {
        if (ordered)
            for (auto& entry : input)
                entry.rank = bench::mix64(entry.payload ^ 0x72616e6bu);
        check(tree.build(input, {.workspace = &workspace}) == QuadTree::Status::Ok);
        auto start = bench::memory::allocations();
        check(tree.build(input, {.workspace = &workspace}) == QuadTree::Status::Ok);
        check(tree.forEachNode([](const auto& node) noexcept
        {
            emitBox(node.cell);
            if (node.entries != 0)
                emitBox(node.bounds);
            emit(node.summary.category, node.summary.mask, node.depth, node.entries, node.ownEntries);
            return QuadTree::Continue;
        }).ok());
        check(tree.forEachEntry([](const auto& entry) noexcept
        {
            emit(entry.payload);
            return QuadTree::Continue;
        }).ok());
        // Compare the exact callback stream, including orientation and order.
        check(tree.forEachPair([](const auto& a, const auto& b) noexcept
        {
            emit(a.payload, b.payload);
            return QuadTree::Continue;
        }, {.ordered = ordered, .workspace = &workspace}).ok());
        check(bench::memory::allocations() == start);
    }
}
} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "extremes")
    {
        extremes<std::int32_t>();
        extremes<std::int64_t>();
    }
    else if (argc == 3)
        fixture(argv[1], std::strtoul(argv[2], nullptr, 10));
    else
        return 2;
    check(std::fflush(stdout) == 0);
}
