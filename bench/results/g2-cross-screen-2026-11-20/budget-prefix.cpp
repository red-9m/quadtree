// Untimed: compare unordered status, work and callback prefixes to the radix control.
#include <quadtree-index.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>

namespace
{
void check(bool ok, std::source_location where = std::source_location::current())
{
    if (!ok)
    {
        std::fprintf(stderr, "probe check failed at %s:%u\n", where.file_name(), static_cast<unsigned>(where.line()));
        std::abort();
    }
}

template<typename... Fields>
void emit(Fields... fields)
{
    check(((std::fwrite(&fields, sizeof(fields), 1, stdout) == 1) && ...));
}

constexpr auto unlimited = std::numeric_limits<std::uint64_t>::max();

void geometry()
{
    using namespace QuadTree;
    using T = std::int32_t;
    const std::array<T, 5> coordinates{std::numeric_limits<T>::min(), -1, 0, 1, std::numeric_limits<T>::max()};
    std::array<Box<T>, 225> boxes{};
    std::size_t n = 0;
    for (auto x : coordinates)
        for (auto y : coordinates)
            for (auto u : coordinates)
                for (auto v : coordinates)
                    if (x <= u && y <= v)
                        boxes[n++] = {x, y, u, v};
    check(n == boxes.size());
    for (const auto& a : boxes)
        for (const auto& b : boxes)
            for (auto mode : {Boundary::Default, Boundary::TouchInclusive})
            {
                const auto x0 = std::max(a.minX, b.minX), x1 = std::min(a.maxX, b.maxX);
                const auto y0 = std::max(a.minY, b.minY), y1 = std::min(a.maxY, b.maxY);
                const bool degenerate = a.minX == a.maxX || a.minY == a.maxY ||
                    b.minX == b.maxX || b.minY == b.maxY;
                const bool expected = mode == Boundary::TouchInclusive || degenerate ?
                    x0 <= x1 && y0 <= y1 : x0 < x1 && y0 < y1;
                check(detail::overlaps(a, b, mode) == expected);
            }
}

template<bool Instrumented>
void fixture(QuadTree::Boundary boundary)
{
    using namespace QuadTree;
    using I = Index<int, std::int32_t, Checked, Instrumented>;
    constexpr int n = 96;
    std::array<BuildEntry<int, std::int32_t>, n> entries{};
    for (int i = 0; i < n; ++i)
    {
        const int x = (i % 4) * 36 - 70 + (i * 7) % 21;
        const int y = (i % 3) * 25 - 40 + (i * 11) % 17;
        const Filter filter = i % 7 == 0 ? Filter{0, 0} : i % 3 == 0 ? Filter{2, 3} : Filter{1, 1};
        entries[static_cast<std::size_t>(i)] = {{x, y, x + i % 16, y + i % 13}, i, filter, {}};
    }
    // Exact contact, a point, and spanning boxes to force cross-subtree work.
    entries[0].rect = {-38, -13, -34, -9};
    entries[1].rect = {-34, -13, -23, -9};
    entries[2].rect = {-34, -9, -34, -9};
    entries[3].rect = {-40, -17, 39, 18};
    entries[4].rect = {-31, -41, -28, 19};
    for (std::size_t i = 0; i < 5; ++i)
        entries[i].filter = {1, 1};
    Workspace<std::int32_t> workspace;
    check(workspace.reserve({n, 0, n}) == Status::Ok);
    auto made = I::create({.boundary = boundary, .splitThreshold = 3, .mergeThreshold = 1, .smallSetThreshold = 0});
    check(made.ok());
    auto& index = made.value();
    check(index.build(entries, {.workspace = &workspace}) == Status::Ok);
    check(index.stats().nodes > 5);
    typename I::HandleType handle;
    check(index.forEachEntry([&](const auto& entry) noexcept
    {
        if (entry.payload == 50)
            handle = entry.handle;
        return Continue;
    }).complete());
    const auto run = [&](WorkBudget budget, std::size_t stopAfter, bool custom) noexcept
    {
        std::size_t reports = 0;
        const auto visitor = [&](const auto& a, const auto& b) noexcept
        {
            emit('v', a.payload, b.payload);
            return ++reports == stopAfter ? Stop : Continue;
        };
        const auto predicate = [](const auto& a, const auto& b) noexcept
        {
            emit('p', a.payload, b.payload);
            return (a.payload + b.payload) % 5 != 0;
        };
        const auto result = custom ? index.forEachPair(visitor, {.budget = budget}, predicate) :
            index.forEachPair(visitor, {.budget = budget});
        const auto& c = result.counters;
        emit('r', result.status, c.nodeVisits, c.candidateTests, c.filterRejections, c.predicateCalls, c.reports);
        return result;
    };
    for (bool mutate : {false, true})
    {
        if (mutate)
            check(index.update(handle, Box<std::int32_t>{-42, -16, -36, -9}) == Status::Ok);
        for (bool custom : {false, true})
        {
            const auto full = run({}, 0, custom);
            check(full.complete());
            if constexpr (Instrumented)
                check(full.counters.nodeVisits > 100 && full.counters.candidateTests > 100 && full.counters.reports > 10);
            const auto visits = Instrumented ? full.counters.nodeVisits : std::uint64_t{1200};
            const auto tests = Instrumented ? full.counters.candidateTests : std::uint64_t{1200};
            for (std::uint64_t k = 0; k <= visits + 1; ++k) (void)run({unlimited, k}, 0, custom);
            for (std::uint64_t k = 0; k <= tests + 1; ++k) (void)run({k, unlimited}, 0, custom);
            for (std::uint64_t k = 0; k <= visits; k += 13)
                for (const std::size_t stop : {1u, 3u, 17u})
                    (void)run({k / 5, k}, stop, custom);
            for (const std::size_t stop : {1u, 3u, 17u, 91u}) (void)run({}, stop, custom);
        }
    }
}
} // namespace

int main()
{
    geometry();
    for (auto boundary : {QuadTree::Boundary::Default, QuadTree::Boundary::TouchInclusive})
    {
        fixture<true>(boundary);
        fixture<false>(boundary);
    }
    check(std::fflush(stdout) == 0);
}
