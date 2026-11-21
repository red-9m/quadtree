#include "quadtree-index.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>

using namespace QuadTree;
using I = Index<std::uint32_t, std::int32_t>;
using E = BuildEntry<std::uint32_t, std::int32_t>;
using B = Box<std::int32_t>;

static void check(bool ok)
{
    if (!ok)
        std::abort();
}

int main(int argc, char** argv)
{
    check(argc == 2);
    const bool invalid = std::string_view(argv[1]) == "invalid-singleton";
    Config<std::int32_t> config;
    config.extent = Extent::Snapshot;
    config.snapshotGrowth = SnapshotGrowth::Adaptive;
    auto created = I::create(config);
    check(created.ok());
    auto index = std::move(created).value();
    Workspace<std::int32_t> scratch;
    check(index.reserve({.entries = 2, .rankedEntries = 2}) == Status::Ok);
    check(scratch.reserve({2, 0, 2}) == Status::Ok);
    std::array<E, 1> one{{{B{0,0,2,2},1}}};
    if (invalid)
    {
        check(index.build(one, {.workspace = &scratch}) == Status::Ok);
        const auto original = index.extent();
        one[0].rect = B{2,2,4,4};
        check(index.build(one, {.workspace = &scratch}) == Status::Ok);
        check(index.extent() == original && index.extent() != one[0].rect);
        std::cout << "Invalid singleton bypass confirmed: successful replacement leaves old extent.\n";
        return 0;
    }
    const auto never = [](const auto&, const auto&) noexcept -> Visit { std::abort(); };
    const auto noPredicate = [](const auto&, const auto&) noexcept -> bool { std::abort(); };
    const auto tiny = [&](Status status, bool ordered)
    {
        const auto r = index.forEachPair(never, {.ordered = ordered, .budget = {0,0}}, noPredicate);
        check(r.status == status && r.counters.nodeVisits == 0 && r.counters.candidateTests == 0 &&
              r.counters.filterRejections == 0 && r.counters.predicateCalls == 0 && r.counters.reports == 0);
    };
    tiny(Status::Ok, false);
    tiny(Status::Ok, true);
    check(index.build(one, {.workspace = &scratch}) == Status::Ok);
    tiny(Status::Ok, false);
    tiny(Status::MissingRank, true);
    one[0].rank = 1;
    check(index.build(one, {.workspace = &scratch}) == Status::Ok);
    tiny(Status::Ok, true);
    std::array<E, 2> two{one[0], E{B{0,0,2,2},2,{},0}};
    check(index.build(two, {.workspace = &scratch}) == Status::Ok);
    for (bool ordered: {false,true})
    {
        const auto capped = index.forEachPair(never, {.ordered = ordered, .workspace = &scratch, .budget = {0,0}});
        check(capped.status == Status::BudgetExhausted && capped.counters.candidateTests == 0 && capped.counters.reports == 0);
        unsigned calls = 0;
        const auto stopped = index.forEachPair([&](const auto& a, const auto& b) noexcept
        {
            check(a.payload == (ordered ? 2u : 1u) && b.payload == (ordered ? 1u : 2u));
            ++calls;
            return Stop;
        }, {.ordered = ordered, .workspace = &scratch});
        check(stopped.status == Status::Stopped && calls == 1 && stopped.counters.reports == 1);
    }
    std::cout << "Pair-query status, rank, budgets, exact orientation and Stop pass.\n";
}
