// Compare exact topology, dense entry and pair streams without private access.
#include "bench-index.h"

#include <cstdlib>

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

void emitBox(const QuadTree::Box<std::int32_t>& box)
{
    emit(box.minX, box.minY, box.maxX, box.maxY);
}

void emitResult(QuadTree::QueryResult result)
{
    check(result.ok());
    const auto c = result.counters;
    emit(c.nodeVisits, c.candidateTests, c.filterRejections, c.predicateCalls, c.reports);
}

void fixture(std::string_view scene, std::uint32_t n, std::uint32_t threshold)
{
    using T = std::int32_t;
    using Index = QuadTree::Index<std::uint32_t, T>;
    using QuadTree::Status;
    const auto boxes = bench::convert<T>(bench::generate(scene, n, bench::sceneSeed(1, scene, n, 0)));
    bench::IndexResource memory;
    QuadTree::Workspace<T> workspace(&memory);
    check(workspace.reserve({n, n, n}) == Status::Ok);
    auto made = Index::create({.extent = QuadTree::Extent::Snapshot,
        .snapshotGrowth = QuadTree::SnapshotGrowth::Adaptive, .smallSetThreshold = threshold}, &memory);
    check(made.ok());
    auto tree = std::move(made).value();
    std::vector<QuadTree::BuildEntry<std::uint32_t, T>> input;
    for (std::uint32_t i = 0; i < n; ++i)
        input.push_back({boxes[i], i});
    std::vector<Index::HandleType> handles(n), old(n);
    auto pair = [](const auto& a, const auto& b) noexcept
    {
        emit(a.payload, b.payload);
        return QuadTree::Continue;
    };
    for (int mode = 0; mode < 3; ++mode)
    {
        const bool ordered = mode == 2;
        if (ordered)
            for (auto& entry : input)
                entry.rank = bench::mix64(entry.payload ^ 0x72616e6bu);
        auto build = [&]
        {
            return mode == 0 ? tree.build(input, {.workspace = &workspace}) :
                tree.build(input, handles, {.workspace = &workspace});
        };
        check(build() == Status::Ok);
        std::copy(handles.begin(), handles.end(), old.begin());
        const auto allocations = bench::memory::allocations();
        check(build() == Status::Ok);
        if (mode != 0)
            for (auto handle : old)
                check(!tree.valid(handle));
        emitResult(tree.forEachNode([](const auto& node) noexcept
        {
            emitBox(node.cell);
            if (node.entries != 0)
                emitBox(node.bounds);
            emit(node.summary.category, node.summary.mask, node.depth, node.entries, node.ownEntries);
            return QuadTree::Continue;
        }));
        emitResult(tree.forEachEntry([&](const auto& entry) noexcept
        {
            check(tree.valid(entry.handle));
            if (mode != 0)
                check(entry.handle == handles[entry.payload]);
            emit(entry.payload, entry.filter.category, entry.filter.mask, entry.rank.value_or(0));
            emitBox(entry.rect);
            return QuadTree::Continue;
        }));
        const QuadTree::QueryOptions<T> options{.ordered = ordered, .workspace = &workspace};
        emitResult(tree.forEachPair(pair, options));
        if (mode != 0)
        {
            const Index::HandleType moved[]{handles[n - 1], handles[n / 2], handles[0]};
            emitResult(tree.forEachPairInvolving(moved, pair, options));
        }
        check(bench::memory::allocations() == allocations);
    }
}
} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "tails")
    {
        for (std::uint32_t n : {15u, 16u, 17u, 31u, 32u, 33u})
            fixture("clustered", n, 0);
        for (std::uint32_t n : {64u, 65u})
            fixture("clustered", n, 64);
    }
    else if (argc == 3)
        fixture(argv[1], static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 10)), 64);
    else
        return 2;
    check(std::fflush(stdout) == 0);
}
