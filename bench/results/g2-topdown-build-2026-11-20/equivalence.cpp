// Untimed comparison. Unordered pairs are normalized by payload ID because
// the top-down loader changes the dense slot assignment within leaves.
#include "bench-index.h"

#include <limits>

namespace
{
using T = std::int32_t;
using Index = QuadTree::Index<std::uint32_t, T>;
using Box = QuadTree::Box<T>;
using Pair = std::pair<std::uint32_t, std::uint32_t>;

void check(bool ok) { if (!ok) std::abort(); }

template<typename... V>
void emit(V... fields)
{
    check(((std::fwrite(&fields, sizeof(fields), 1, stdout) == 1) && ...));
}

void emitBox(const Box& b) { emit(b.minX, b.minY, b.maxX, b.maxY); }

void compareBuild(std::span<const Box> boxes)
{
    const auto n = boxes.size();
    bench::IndexResource resource;
    QuadTree::Workspace<T> workspace(&resource);
    check(workspace.reserve({n, n, n}) == QuadTree::Status::Ok);
    auto made = Index::create({.extent = QuadTree::Extent::Snapshot,
        .snapshotGrowth = QuadTree::SnapshotGrowth::Adaptive}, &resource);
    check(made.ok());
    auto tree = std::move(made).value();
    std::vector<QuadTree::BuildEntry<std::uint32_t, T>> input;
    for (std::uint32_t i = 0; i < n; ++i)
        input.push_back({boxes[i], i, {}, bench::mix64(i ^ 0x72616e6bu)});
    std::vector<Index::HandleType> handles(n), dirty;
    check(tree.build(input, handles, {.workspace = &workspace}) == QuadTree::Status::Ok);
    const auto old = handles.front();
    const auto allocations = bench::memory::allocations();
    check(tree.build(input, handles, {.workspace = &workspace}) == QuadTree::Status::Ok);
    check(bench::memory::allocations() == allocations && !tree.valid(old));
    for (std::size_t i = 0; i < n; ++i)
    {
        check(tree.valid(handles[i]) && tree.payload(handles[i]) == i && tree.rank(handles[i]) == input[i].rank);
        emitBox(tree.rect(handles[i]));
        if (i % 127 == 0) dirty.push_back(handles[i]);
    }
    emit(tree.stats().nodes);
    check(tree.forEachNode([](const auto& node) noexcept
    {
        emitBox(node.cell);
        if (node.entries != 0) emitBox(node.bounds);
        emit(node.summary.category, node.summary.mask, node.depth, node.entries, node.ownEntries);
        return QuadTree::Continue;
    }).ok());

    std::vector<Pair> pairs;
    for (bool ordered : {false, true})
        for (bool moved : {false, true})
        {
            pairs.clear();
            auto collect = [&](const auto& a, const auto& b) noexcept
            {
                if (!ordered)
                {
                    using QuadTree::detail::HandleAccess;
                    check(HandleAccess::slot(a.handle) < HandleAccess::slot(b.handle));
                    pairs.emplace_back(std::min(a.payload, b.payload), std::max(a.payload, b.payload));
                }
                else
                {
                    check(a.rank && b.rank && *a.rank < *b.rank);
                    pairs.emplace_back(a.payload, b.payload);
                }
                return QuadTree::Continue;
            };
            const QuadTree::QueryOptions<T> options{.ordered = ordered, .workspace = &workspace};
            const auto r = moved ? tree.forEachPairInvolving(dirty, collect, options) : tree.forEachPair(collect, options);
            check(r.ok());
            if (!ordered) std::sort(pairs.begin(), pairs.end());
            emit(pairs.size());
            for (const auto& [a, b] : pairs) emit(a, b);
        }
}
} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "extremes")
    {
        constexpr auto lo = std::numeric_limits<T>::lowest(), hi = std::numeric_limits<T>::max();
        std::vector<Box> boxes{{lo, lo, hi, hi}, {lo, 0, hi, 0}, {hi, hi, hi, hi}};
        for (T i = 0; i < 257; ++i)
        {
            boxes.push_back({lo + i, 0, lo + i, 1});
            boxes.push_back({hi - i, 0, hi - i, 1});
        }
        compareBuild(boxes);
        for (auto& box : boxes) box = {0, 0, 0, 0};
        compareBuild(boxes);
    }
    else if (argc == 3)
    {
        const auto n = std::strtoul(argv[2], nullptr, 10);
        const auto boxes = bench::convert<T>(bench::generate(argv[1], n, bench::sceneSeed(1, argv[1], n, 0)));
        compareBuild(boxes);
    }
    else return 2;
    check(std::fflush(stdout) == 0);
}
