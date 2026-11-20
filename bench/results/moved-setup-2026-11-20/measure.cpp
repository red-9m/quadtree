// Task 04 diagnostic: prepared moved queries with the first node visit denied.
#include "quadtree-index.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <utility>
#include <vector>

namespace
{
using I = QuadTree::Index<int, std::int32_t, QuadTree::Checked>;
using Workspace = QuadTree::Workspace<std::int32_t>;
using QuadTree::Status;
using Clock = std::chrono::steady_clock;
constexpr std::size_t batches = 9, sets = 16;

void check(bool ok)
{
    if (!ok)
    {
        std::fputs("moved-setup invariant failed\n", stderr);
        std::exit(1);
    }
}

class CountingResource final: public QuadTree::Resource
{
public:
    std::size_t allocations = 0;
    void* allocate(std::size_t bytes, std::size_t alignment) noexcept override
    {
        ++allocations;
        return QuadTree::defaultResource()->allocate(bytes, alignment);
    }
    void deallocate(void* ptr) noexcept override { QuadTree::defaultResource()->deallocate(ptr); }
};

// Prevent specialization or hoisting of the empty-set call out of the batch.
[[gnu::noinline]] QuadTree::QueryResult setup(const I& tree, Workspace& scratch,
    std::span<const I::HandleType> moved, bool ordered)
{
    asm volatile("" ::: "memory");
    return tree.forEachPairInvolving(moved, [](const auto&, const auto&) noexcept -> QuadTree::Visit
        { std::abort(); }, {.ordered = ordered, .workspace = &scratch, .budget = {.nodeVisits = 0}});
}

[[gnu::noinline]] void reset(Workspace& scratch)
{
    asm volatile("" ::: "memory");
    scratch.resetMovedMembership();
}

void measure(std::size_t n, unsigned run)
{
    CountingResource memory;
    auto made = I::create({.smallSetThreshold = 0}, &memory);
    check(made.ok());
    auto tree = std::move(made).value();
    Workspace build(&memory), scratch(&memory);
    check(build.reserve({0, 0, n}) == Status::Ok);
    check(scratch.reserve({n, n / 100, 0}) == Status::Ok);
    std::vector<QuadTree::BuildEntry<int, std::int32_t>> input;
    std::vector<I::HandleType> handles(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto x = static_cast<std::int32_t>(16 * (i % 320));
        const auto y = static_cast<std::int32_t>(16 * (i / 320));
        input.push_back({{x, y, x + 20, y + 20}, static_cast<int>(i), {}, n - i}); // No rank-zero shortcut.
    }
    check(tree.build(input, handles, {.workspace = &build}) == Status::Ok);
    check(tree.stats().nodes > 1);
    const auto allocations = memory.allocations, bytes = scratch.retainedBytes();
    struct SampleCase { const char* kind; std::size_t count; bool ordered; };
    const std::array cases{SampleCase{"empty", 0, false}, SampleCase{"single", 1, false},
        SampleCase{"fixed-10", 10, false}, SampleCase{"one-percent", n / 100, false},
        SampleCase{"empty", 0, true}, SampleCase{"single", 1, true},
        SampleCase{"fixed-10", 10, true}, SampleCase{"one-percent", n / 100, true}};
    std::array<std::vector<I::HandleType>, sets> moved;
    for (std::size_t s = 0; s < sets; ++s)
        for (std::size_t i = 0; i < n / 100; ++i)
            moved[s].push_back(handles[(s * (n / sets) + i * 97) % n]);
    for (std::size_t batch = 0; batch < batches; ++batch)
    {
        for (std::size_t k = 0; k < cases.size(); ++k)
        {
            const auto [kind, count, ordered] = cases[(k + batch + run) % cases.size()];
            const auto expected = count == 0 ? Status::Ok : Status::BudgetExhausted;
            for (const auto& list : moved)
            {
                const auto result = setup(tree, scratch, std::span(list).first(count), ordered);
                const auto c = result.counters;
                check(result.status == expected && c.nodeVisits == 0 && c.candidateTests == 0 &&
                    c.filterRejections == 0 && c.predicateCalls == 0 && c.reports == 0);
            }
            const std::size_t reps = std::max<std::size_t>(4096, 524288 / (1 + count / 4));
            unsigned bad = 0;
            const auto start = Clock::now();
            for (std::size_t i = 0; i < reps; ++i)
                bad |= setup(tree, scratch, std::span(moved[i % sets]).first(count), ordered).status != expected;
            const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
            check(bad == 0 && memory.allocations == allocations && scratch.retainedBytes() == bytes);
            std::printf("%u,%zu,%zu,%s,%s,%zu,%zu,%lld,%zu\n", run, batch, n, ordered ? "ordered" : "unordered",
                kind, count, reps, static_cast<long long>(ns), memory.allocations - allocations);
        }
        // Positive control: the explicit O(n) membership reset, not part of query setup.
        constexpr std::size_t reps = 4096;
        const auto start = Clock::now();
        for (std::size_t i = 0; i < reps; ++i)
            reset(scratch);
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count();
        check(memory.allocations == allocations && scratch.retainedBytes() == bytes);
        std::printf("%u,%zu,%zu,na,reset,0,%zu,%lld,%zu\n", run, batch, n, reps, static_cast<long long>(ns),
            memory.allocations - allocations);
    }
}
} // namespace

int main(int argc, char** argv)
{
    check(argc == 2);
    const auto run = static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10));
    const std::array<std::size_t, 3> sizes{1000, 10000, 100000};
    std::puts("run,batch,n,order,case,moved,repetitions,total_ns,allocations");
    for (std::size_t i = 0; i < sizes.size(); ++i)
        measure(sizes[run % 2 == 0 ? sizes.size() - i - 1 : i], run);
}
