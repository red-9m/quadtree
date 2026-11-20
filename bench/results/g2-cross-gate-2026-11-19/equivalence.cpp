// Diagnostic: emit exact query event streams, status and counters for cmp.
#include "bench-index.h"

#include <cstdlib>
#include <limits>

namespace
{
constexpr auto unlimited = std::numeric_limits<std::uint64_t>::max();

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

template<bool Instrumented>
void fixture(std::string_view scene, std::size_t n, bool synthetic, bool touch)
{
    using T = std::int32_t;
    using Index = QuadTree::Index<std::uint32_t, T, QuadTree::Unchecked, Instrumented>;
    using QuadTree::Status;
    bench::IndexResource memory;
    QuadTree::Workspace<T> workspace(&memory);
    check(workspace.reserve({0, 0, n}) == Status::Ok);
    auto made = Index::create({.extent = QuadTree::Extent::Snapshot,
        .snapshotGrowth = QuadTree::SnapshotGrowth::Adaptive,
        .boundary = touch ? QuadTree::Boundary::TouchInclusive : QuadTree::Boundary::Default}, &memory);
    check(made.ok());
    auto tree = std::move(made).value();
    const auto boxes = bench::convert<T>(bench::generate(scene, n, bench::sceneSeed(1, scene, n, 0)));
    std::vector<QuadTree::BuildEntry<std::uint32_t, T>> input;
    for (std::uint32_t i = 0; i < n; ++i)
    {
        const std::uint64_t group = 1ull << (i % 3);
        input.push_back({boxes[i], i, synthetic ? QuadTree::Filter{group, i % 7 == 0 ? 0 : group} : QuadTree::Filter{}});
    }
    check(tree.build(input, {.workspace = &workspace}) == Status::Ok);
    const auto allocations = bench::memory::allocations();
    check(tree.build(input, {.workspace = &workspace}) == Status::Ok);
    auto query = [&](QuadTree::WorkBudget budget, std::uint64_t stop, bool custom)
    {
        std::uint64_t reports = 0;
        auto visitor = [&](const auto& a, const auto& b) noexcept
        {
            emit(std::uint8_t{1}, a.payload, b.payload);
            return ++reports == stop ? QuadTree::Stop : QuadTree::Continue;
        };
        auto predicate = [&](const auto& a, const auto& b) noexcept
        {
            emit(std::uint8_t{2}, a.payload, b.payload);
            return (a.payload + b.payload) % 3 != 0;
        };
        const QuadTree::QueryOptions<T> options{.budget = budget};
        const auto result = custom ? tree.forEachPair(visitor, options, predicate) : tree.forEachPair(visitor, options);
        const auto c = result.counters;
        emit(std::uint8_t{0}, static_cast<std::uint8_t>(result.status), c.nodeVisits,
             c.candidateTests, c.filterRejections, c.predicateCalls, c.reports);
        if constexpr (!Instrumented)
            check(c.nodeVisits == 0 && c.candidateTests == 0 && c.filterRejections == 0 && c.predicateCalls == 0 && c.reports == 0);
        else
            check(reports == c.reports);
        return result;
    };
    const auto complete = query({}, unlimited, false);
    check(complete.ok());
    for (std::uint64_t limit : {0u, 1u, 7u, 63u})
    {
        (void)query({limit, unlimited}, unlimited, false);
        (void)query({unlimited, limit}, unlimited, false);
        (void)query({limit, limit}, unlimited, false);
    }
    if (synthetic)
    {
        const auto ref = query({}, unlimited, true);
        check(ref.ok());
        if constexpr (Instrumented)
            check(ref.counters.filterRejections != 0 && ref.counters.reports >= 2);
        check(query({}, 1, true).status == Status::Stopped);
        // Sweep both independent budgets at every value through completeness.
        // For the uninstrumented specialization, detect completion from status.
        for (std::uint64_t limit = 0;; ++limit)
            if (query({unlimited, limit}, unlimited, true).ok())
                break;
        for (std::uint64_t limit = 0;; ++limit)
            if (query({limit, unlimited}, unlimited, true).ok())
                break;
    }
    else if constexpr (Instrumented)
    {
        const auto c = complete.counters;
        check(query({c.candidateTests, c.nodeVisits}, unlimited, false).ok());
        check(query({c.candidateTests, c.nodeVisits - 1}, unlimited, false).status == Status::BudgetExhausted);
        check(query({c.candidateTests - 1, c.nodeVisits}, unlimited, false).status == Status::BudgetExhausted);
    }
    check(bench::memory::allocations() == allocations);
}
} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "synthetic")
        for (bool touch : {false, true})
        {
            fixture<true>("clustered", 192, true, touch);
            fixture<false>("clustered", 192, true, touch);
        }
    else if (argc == 3)
    {
        const auto n = std::strtoul(argv[2], nullptr, 10);
        fixture<true>(argv[1], n, false, false);
        fixture<false>(argv[1], n, false, false);
    }
    else
        return 2;
    check(std::fflush(stdout) == 0);
}
