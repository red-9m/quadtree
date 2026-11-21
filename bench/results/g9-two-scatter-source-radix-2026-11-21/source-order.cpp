// Untimed differential probe for the source list, including independent scratch guards.
#include <quadtree-index.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <source_location>
#include <vector>

namespace
{
using namespace QuadTree;
using T = std::int32_t;
constexpr auto unlimited = std::numeric_limits<std::uint64_t>::max();

void check(bool ok, std::source_location where = std::source_location::current())
{
    if (!ok)
    {
        std::fprintf(stderr, "source-order probe failure at line %u\n", static_cast<unsigned>(where.line()));
        std::abort();
    }
}

template<typename... Fields>
void emit(Fields... fields)
{
    check(((std::fwrite(&fields, sizeof(fields), 1, stdout) == 1) && ...));
}

struct CountingResource final: Resource
{
    std::size_t calls = 0;
    void* allocate(std::size_t bytes, std::size_t alignment) noexcept override
    {
        ++calls;
        return defaultResource()->allocate(bytes, alignment);
    }
    void deallocate(void* ptr) noexcept override { defaultResource()->deallocate(ptr); }
};

void fixture(std::uint32_t n, unsigned pattern, Boundary boundary)
{
    CountingResource resource;
    Workspace<T> full(&resource);
    check(full.reserve({n, 0, n}) == Status::Ok);
    using I = Index<std::uint32_t, T, Checked, true>;
    auto made = I::create({.extent = Extent::Adaptive, .boundary = boundary,
        .splitThreshold = 4, .mergeThreshold = 2, .smallSetThreshold = 0}, &resource);
    check(made.ok());
    auto& index = made.value();
    std::vector<BuildEntry<std::uint32_t, T>> entries;
    entries.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i)
    {
        const auto group = i / 8;
        const T x = 32 * static_cast<T>(group % 32u) + static_cast<T>(i % 4u);
        const T y = 32 * static_cast<T>(group / 32u) + static_cast<T>(i % 3u);
        const auto order = (i * 37u) % 8192u;
        const auto rank = pattern == 0 ? detail::mix64(i) : pattern == 1 ?
            (std::uint64_t(order) << 48) | 0x0000123456789abcULL : 0xfedcba9876540000ULL | order;
        check(rank != unlimited);
        const Filter filter = i % 7u == 0 ? Filter{0, 0} : i % 3u == 0 ? Filter{2, 3} : Filter{1, 1};
        entries.push_back({{x, y, x + 3, y + 4}, i, filter, rank});
    }
    check(index.build(entries, {.workspace = &full}) == Status::Ok && index.stats().nodes > 5);
    std::vector<std::uint32_t> expected(n);
    std::iota(expected.begin(), expected.end(), 0u);
    const auto& store = index.mEntries;
    std::sort(expected.begin(), expected.end(), [&](auto a, auto b) { return store.mMeta[a].rank < store.mMeta[b].rank; });

    const auto verify = [&](Workspace<T>& scratch, bool eligible)
    {
        const auto calls = resource.calls;
        const auto held = scratch.retainedBytes();
        const auto run = [&](WorkBudget budget, unsigned stopAfter)
        {
            if (scratch.mKeys.capacity() != 0)
                scratch.mKeys[0].hi = unlimited;
            unsigned reports = 0;
            auto result = index.forEachPair([&](const auto& a, const auto& b) noexcept
            {
                emit('v', a.payload, b.payload);
                return ++reports == stopAfter ? Stop : Continue;
            }, {.ordered = true, .workspace = &scratch, .budget = budget},
            [](const auto& a, const auto& b) noexcept
            {
                emit('p', a.payload, b.payload);
                return (a.payload + b.payload) % 5u != 0;
            });
            const auto& c = result.counters;
            emit('r', result.status, c.nodeVisits, c.candidateTests, c.filterRejections, c.predicateCalls, c.reports);
            check(std::equal(expected.begin(), expected.end(), scratch.mOrder.data()));
            check(resource.calls == calls && scratch.retainedBytes() == held);
            // No row can have 128 partners in this fixture. Only the source sort can touch this sentinel.
            if (scratch.mKeys.capacity() != 0)
                check((scratch.mKeys[0].hi != unlimited) == (EXPECT_RADIX && eligible));
            return result;
        };
        const auto complete = run({}, 0);
        const auto visits = complete.counters.nodeVisits, tests = complete.counters.candidateTests;
        check(complete.complete() && visits > 100 && tests > 100 && complete.counters.reports > 100);
        for (auto k : {std::uint64_t{0}, std::uint64_t{1}, visits / 2, visits - 1, visits})
            (void)run({unlimited, k}, 0);
        for (auto k : {std::uint64_t{0}, std::uint64_t{1}, tests / 2, tests - 1, tests})
            (void)run({k, unlimited}, 0);
        for (unsigned k : {1u, 3u, 91u})
            (void)run({}, k);
        (void)run({tests / 2, visits / 2}, 91);
    };
    verify(full, n >= 4096);
    if (n >= 4096)
    {
        for (unsigned mode : {0u, 1u, 2u})
        {
            Workspace<T> scratch(&resource);
            check(scratch.reserve({n, 0, 0}) == Status::Ok);
            if (mode != 0)
            {
                // An untimed diagnostic can make either independent buffer one key short.
                check(scratch.mKeys.allocate(mode == 1 ? n - 1u : n) == Status::Ok);
                check(scratch.mTemp.allocate((mode == 2 ? n - 1u : n) * sizeof(detail::SpatialKey)) == Status::Ok);
            }
            verify(scratch, false);
        }
    }
    check(index.build(entries, {.workspace = &full}) == Status::Ok);
    verify(full, n >= 4096);
}
} // namespace

int main()
{
    fixture(4095, 0, Boundary::Default);
    fixture(4096, 0, Boundary::Default);
    fixture(4097, 1, Boundary::TouchInclusive);
    fixture(4097, 2, Boundary::Default);
    check(std::fflush(stdout) == 0);
}
