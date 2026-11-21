// Untimed: compare exact status, counters, predicate order and visitor streams.
#include <quadtree-index.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace
{
void check(bool ok) { if (!ok) std::abort(); }

template<typename... Fields>
void emit(Fields... fields)
{
    check(((std::fwrite(&fields, sizeof(fields), 1, stdout) == 1) && ...));
}

constexpr auto unlimited = std::numeric_limits<std::uint64_t>::max();

template<bool Instrumented>
void fixture(QuadTree::Boundary boundary)
{
    using namespace QuadTree;
    using I = Index<int, std::int32_t, Checked, Instrumented>;
    constexpr int n = 80;
    std::array<BuildEntry<int, std::int32_t>, n> entries{};
    for (int i = 0; i < n; ++i)
    {
        const int x = (i % 4) * 70 - 110 + (i * 7) % 17;
        const int y = (i % 3) * 60 - 80 + (i * 11) % 21;
        const Filter filter = i % 7 == 0 ? Filter{0, 0} : i % 3 == 0 ? Filter{2, 3} : Filter{1, 1};
        entries[static_cast<std::size_t>(i)] = {{x, y, x + i % 12, y + i % 14}, i, filter,
            static_cast<std::uint64_t>((i * 37) % n)};
    }
    Workspace<std::int32_t> workspace;
    check(workspace.reserve({n, 0, n}) == Status::Ok);
    auto made = I::create({.boundary = boundary, .splitThreshold = 3, .mergeThreshold = 1, .smallSetThreshold = 0});
    check(made.ok());
    auto& index = made.value();
    check(index.build(entries, {.workspace = &workspace}) == Status::Ok);
    check(index.stats().nodes > 5);
    const auto run = [&](WorkBudget budget, std::size_t stopAfter) noexcept
    {
        std::size_t reports = 0;
        const auto result = index.forEachPair([&](const auto& a, const auto& b) noexcept
        {
            emit('v', a.payload, b.payload);
            return ++reports == stopAfter ? Stop : Continue;
        }, {.ordered = true, .workspace = &workspace, .budget = budget},
        [](const auto& a, const auto& b) noexcept
        {
            emit('p', a.payload, b.payload);
            return (a.payload + b.payload) % 5 != 0;
        });
        const auto& c = result.counters;
        emit('r', result.status, c.nodeVisits, c.candidateTests, c.filterRejections, c.predicateCalls, c.reports);
        return result;
    };
    const auto full = run({}, 0);
    check(full.complete());
    // Instrumented execution declares the total; independently sweep finite
    // limits on the uninstrumented instantiation as well.
    const auto visits = Instrumented ? full.counters.nodeVisits : std::uint64_t{1600};
    const auto tests = Instrumented ? full.counters.candidateTests : std::uint64_t{330};
    for (std::uint64_t k = 0; k <= visits + 1; ++k) (void)run({unlimited, k}, 0);
    for (std::uint64_t k = 0; k <= tests + 1; ++k) (void)run({k, unlimited}, 0);
    for (std::uint64_t k = 0; k <= visits; k += 13)
        for (const std::size_t stop : {1u, 3u, 17u})
            (void)run({k / 5, k}, stop);
    for (const std::size_t stop : {1u, 3u, 17u, 91u}) (void)run({}, stop);
}
} // namespace

int main()
{
    for (auto boundary : {QuadTree::Boundary::Default, QuadTree::Boundary::TouchInclusive})
    {
        fixture<true>(boundary);
        fixture<false>(boundary);
    }
    check(std::fflush(stdout) == 0);
}
