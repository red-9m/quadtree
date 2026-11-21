// Exact callbacks at the full/single-child charge boundary, before and after refits.
#include <quadtree-index.h>

#include <array>
#include <cstdio>
#include <cstdlib>

namespace
{
using namespace QuadTree;
using T = std::int32_t;

void check(bool ok) { if (!ok) std::abort(); }

template<bool Instrumented>
void probe()
{
    using I = Index<int, T, Checked, Instrumented>;
    auto made = I::create({.extent = Extent::Fixed, .root = Box<T>{0, 0, 256, 256},
        .splitThreshold = 3, .mergeThreshold = 1, .smallSetThreshold = 0});
    check(made.ok());
    auto& index = made.value();
    std::array<BuildEntry<int, T>, 16> input;
    std::array<typename I::HandleType, 16> handles{};
    Workspace<T> workspace;
    check(workspace.reserve(16) == Status::Ok);
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        const T x = (i & 4) ? 200 : 16;
        const T y = (i & 8) ? 200 : 16;
        input[i] = {{x, y, x + 4, y + 4}, static_cast<int>(i), {}, i};
    }
    check(index.build(input, handles, {.workspace = &workspace}) == Status::Ok);
    check(index.stats().nodes == 5);
    for (std::size_t stage = 0; stage != 3; ++stage)
    {
        if (stage == 1)
            for (std::size_t i = 12; i < 16; ++i) check(index.remove(handles[i]) == Status::Ok);
        if (stage == 2)
            for (std::size_t i = 12; i < 16; ++i)
                check(index.insert(input[i].rect, input[i].payload, input[i].filter, input[i].rank).ok());
        for (std::uint64_t visits = 0; visits <= 76; ++visits)
            for (std::size_t stop : {0u, 1u, 7u})
            {
                std::size_t reports = 0;
                const auto result = index.forEachPair([&](const auto& a, const auto& b) noexcept
                {
                    std::printf("v%d,%d ", a.payload, b.payload);
                    return ++reports == stop ? Stop : Continue;
                }, {.ordered = true, .workspace = &workspace, .budget = {.nodeVisits = visits}},
                [](const auto& a, const auto& b) noexcept
                {
                    std::printf("p%d,%d ", a.payload, b.payload);
                    return true;
                });
                const auto& c = result.counters;
                std::printf("%u,%llu,%llu,%llu,%llu,%llu\n", static_cast<unsigned>(result.status),
                    static_cast<unsigned long long>(c.nodeVisits), static_cast<unsigned long long>(c.candidateTests),
                    static_cast<unsigned long long>(c.predicateCalls), static_cast<unsigned long long>(c.filterRejections),
                    static_cast<unsigned long long>(c.reports));
                if (stop == 0 && visits == 76)
                {
                    check(result.complete() && reports == (stage == 1 ? 18u : 24u));
                    if constexpr (Instrumented) check(c.nodeVisits == (stage == 1 ? 44u : 75u));
                }
            }
    }
}
} // namespace

int main()
{
    probe<true>();
    probe<false>();
    check(std::fflush(stdout) == 0);
}
