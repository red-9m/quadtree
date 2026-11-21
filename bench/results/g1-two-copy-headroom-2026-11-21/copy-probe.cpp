#include "quadtree-index.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>

using namespace QuadTree;
using I = Index<std::uint32_t, std::int32_t>;
using H = I::HandleType;
using B = Box<std::int32_t>;
using E = BuildEntry<std::uint32_t, std::int32_t>;

static void check(bool ok)
{
    if (!ok)
        std::abort();
}

int main(int argc, char** argv)
{
    check(argc == 2);
    const bool invalid = std::string_view(argv[1]) == "invalid-copy";
    Config<std::int32_t> config;
    config.extent = Extent::Snapshot;
    config.snapshotGrowth = SnapshotGrowth::Adaptive;
    auto made = I::create(config);
    check(made.ok());
    auto index = std::move(made).value();
    Workspace<std::int32_t> scratch;
    check(scratch.reserve({0, 0, 2}) == Status::Ok);
    const auto options = BuildOptions<std::int32_t>{.workspace = &scratch};
    const std::array<E, 2> cold{{{B{0,0,2,2},1}, {B{1,1,3,3},2}}};
    std::array<H, 2> handles{};
    check(index.build(cold, handles, options) == Status::Ok);
    check(index.valid(handles[0]) && index.valid(handles[1]));
    const auto oldExtent = index.extent();
    std::array<E, 2> warm{{{B{10,0,12,2},7,{1,2}}, {B{11,1,13,3},9,{2,1}}}};
    check(index.build(warm, options) == Status::Ok);
    unsigned visits = 0;
    const auto result = index.forEachPair([&](const auto& a, const auto& b) noexcept
    {
        check(a.payload == warm[0].payload && b.payload == warm[1].payload);
        check(a.rect == warm[0].rect && b.rect == warm[1].rect);
        check(a.filter == warm[0].filter && b.filter == warm[1].filter);
        ++visits;
        return Continue;
    });
    check(result.ok() && visits == 1 && result.counters.candidateTests == 1 && result.counters.reports == 1);
    if (invalid)
    {
        check(index.valid(handles[0]) && index.valid(handles[1]) && index.extent() == oldExtent);
        std::cout << "Scratch branch active: overlapping current payloads/boxes/filters report, but identity and extent stay stale.\n";
    }
    else
    {
        check(!index.valid(handles[0]) && !index.valid(handles[1]) && index.extent() == B{10,0,13,3});
        std::cout << "Control reports the same current pair and publishes fresh identity and extent.\n";
    }
    warm[1].rect = B{14,1,16,3};
    check(index.build(warm, options) == Status::Ok);
    const auto noPair = index.forEachPair([](const auto&, const auto&) noexcept -> Visit { std::abort(); });
    check(noPair.ok() && noPair.counters.candidateTests == 1 && noPair.counters.reports == 0);
    std::cout << "Copying a disjoint box suppresses the old pair without changing candidate charge.\n";
}
