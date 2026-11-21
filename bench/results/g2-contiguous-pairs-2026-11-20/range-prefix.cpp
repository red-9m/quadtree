// Exact charged work and callback prefix in many 4-lane groups and scalar tails.
#include <quadtree-index.h>

#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace
{
using namespace QuadTree;
using T = std::int32_t;
using B = Box<T>;

void check(bool ok) { if (!ok) std::abort(); }

template<bool Instrumented>
void probe(Boundary boundary)
{
    using I = Index<int, T, Checked, Instrumented>;
    constexpr T low = std::numeric_limits<T>::min(), high = std::numeric_limits<T>::max();
    auto made = I::create({.extent = Extent::Fixed, .root = B{low, low, high, high},
        .boundary = boundary, .splitThreshold = 3, .mergeThreshold = 1, .smallSetThreshold = 0, .maxDepth = 0});
    check(made.ok());
    auto& index = made.value();
    constexpr std::array<B, 17> boxes{{{low, low, -1, -1}, {-1, -1, high, high}, {high, high, high, high},
        {low, low, low, low}, {low, -3, high, -3}, {-3, low, -3, high}, {0, 0, 3, 3}, {3, 0, 6, 3},
        {0, 0, 0, 3}, {-3, -3, 0, 0}, {0, 0, 0, 0}, {-5, -5, -3, -3}, {1, 2, 3, 4}, {-9, 0, 9, 0},
        {30, 30, 31, 31}, {0, 0, 5, 5}, {-1, -1, 4, 4}}};
    std::array<BuildEntry<int, T>, boxes.size()> input{};
    std::array<typename I::HandleType, boxes.size()> handles{};
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = {boxes[i], static_cast<int>(i), i % 7 == 0 ? Filter{1, 1} : i % 7 == 1 ? Filter{2, 2} :
            i % 7 == 2 ? Filter{0, 1} : Filter{3, 3}, {}};
    check(index.build(input, handles) == Status::Ok && index.stats().nodes == 1);
    for (bool mutate : {false, true})
    {
        if (mutate) check(index.remove(handles[5]) == Status::Ok);
        for (std::uint64_t budget = 0; budget <= 137; ++budget)
            for (std::size_t stop : {0u, 1u, 2u, 7u, 21u})
            {
                std::size_t reports = 0;
                const auto visitor = [&](const auto& a, const auto& b) noexcept
                {
                    std::printf("v%d,%d ", a.payload, b.payload);
                    return ++reports == stop ? Stop : Continue;
                };
                const auto r = index.forEachPair(visitor, {.budget = {.candidateTests = budget}});
                const auto& c = r.counters;
                std::printf("%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n",
                    static_cast<unsigned>(r.status), c.nodeVisits, c.candidateTests, c.filterRejections, c.reports);
                if (budget == 137 && stop == 0) check(r.complete() && reports > 10);
            }
    }
}
} // namespace

int main()
{
    for (const auto boundary : {Boundary::Default, Boundary::TouchInclusive})
    {
        probe<true>(boundary);
        probe<false>(boundary);
    }
    check(std::fflush(stdout) == 0);
}
