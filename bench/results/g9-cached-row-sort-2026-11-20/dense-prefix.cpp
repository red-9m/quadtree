// Untimed: compare long ordered rows, including runs with no cache storage.
#include <quadtree-index.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace
{
void check(bool ok) { if (!ok) std::abort(); }

template<typename... Fields>
void emit(Fields... fields)
{
    check(((std::fwrite(&fields, sizeof(fields), 1, stdout) == 1) && ...));
}

struct Resource final : QuadTree::Resource
{
    std::size_t allocations{};
    void* allocate(std::size_t bytes, std::size_t alignment) noexcept override
    {
        ++allocations;
        return QuadTree::defaultResource()->allocate(bytes, alignment);
    }
    void deallocate(void* ptr) noexcept override { QuadTree::defaultResource()->deallocate(ptr); }
};

void probe()
{
    using namespace QuadTree;
    constexpr std::size_t n = 130;
    constexpr auto unlimited = std::numeric_limits<std::uint64_t>::max();
    std::array<BuildEntry<int, std::int32_t>, n> entries{};
    for (std::size_t i = 0; i < n; ++i)
    {
        // Unique non-contiguous ranks, including both endpoints and equal low words.
        const auto j = (i * 73) % n;
        const auto rank = j == n - 1 ? unlimited : (std::uint64_t{j} << 37) | (j % 3);
        entries[i] = {{0, 0, 10, 10}, static_cast<int>(i), {}, rank};
    }
    Resource resource;
    auto made = Index<int, std::int32_t, Checked>::create({.smallSetThreshold = 0}, &resource);
    check(made.ok());
    auto& index = made.value();
    Workspace<std::int32_t> build(&resource);
    check(build.reserve({0, 0, n}) == Status::Ok && index.build(entries, {.workspace = &build}) == Status::Ok);
    check(index.stats().nodes == 1);
    for (const auto cache : {std::size_t{0}, std::size_t{65}, n})
    {
        Workspace<std::int32_t> scratch(&resource);
        check(scratch.reserve({n, 0, cache}) == Status::Ok);
        const auto allocations = resource.allocations;
        const auto run = [&](WorkBudget budget, std::size_t stop, bool custom)
        {
            QueryOptions<std::int32_t> options{.ordered = true, .workspace = &scratch, .budget = budget};
            std::size_t reports = 0;
            auto visit = [&](const auto& a, const auto& b) noexcept
            {
                check(a.rank && b.rank && *a.rank < *b.rank);
                emit('v', a.payload, b.payload, *a.rank, *b.rank);
                return ++reports == stop ? Stop : Continue;
            };
            const auto result = custom ? index.forEachPair(visit, options, [](const auto& a, const auto& b) noexcept
            {
                emit('p', a.payload, b.payload);
                return (a.payload + b.payload) % 7 != 0;
            }) : index.forEachPair(visit, options);
            const auto& c = result.counters;
            emit('r', result.status, c.nodeVisits, c.candidateTests, c.filterRejections, c.predicateCalls, c.reports);
            check(resource.allocations == allocations);
            return result.status;
        };
        for (bool custom : {false, true})
        {
            check(run({}, 0, custom) == Status::Ok);
            for (const auto cut : {0u, 64u, 65u, 128u, 129u, 130u, 4094u, 4095u})
                check(run({cut, unlimited}, 0, custom) == Status::BudgetExhausted);
            for (const auto visits : {1u, 2u, 64u, 66u, 128u})
                check(run({unlimited, visits}, 0, custom) == Status::BudgetExhausted);
            for (const auto stop : {1u, 65u, 120u, 130u, 1150u})
                check(run({}, stop, custom) == Status::Stopped);
        }
    }
}
} // namespace

int main()
{
    probe();
    check(std::fflush(stdout) == 0);
}
