// Contract check (not a control comparison): under node/candidate budgets, ordered output must be an exact
// prefix of the complete ordered sequence, and unordered output a duplicate-free subset of the complete set.
// Exercised for int64 on bench fixtures, both boundary modes, full and moved-set queries.
#include "bench-index.h"

#include <algorithm>
#include <cstdio>
#include <set>

namespace
{
#ifndef PROBE_T
#define PROBE_T std::int64_t
#endif
using T = PROBE_T;
using Index = QuadTree::Index<std::uint32_t, T>;
using Pair = std::pair<std::uint32_t, std::uint32_t>;
unsigned long checks = 0;

void check(bool ok, int line)
{
    if (!ok)
    {
        std::fprintf(stderr, "contract failure at line %d\n", line);
        std::abort();
    }
    ++checks;
}
#define CHECK(x) check((x), __LINE__)

void run(const char* scene, std::uint32_t n, QuadTree::Boundary boundary)
{
    const auto boxes = bench::convert<T>(bench::generate(scene, n, bench::sceneSeed(1, scene, n, 0)));
    QuadTree::Workspace<T> workspace;
    CHECK(workspace.reserve({n, n, n}) == QuadTree::Status::Ok);
    auto made = Index::create({.extent = QuadTree::Extent::Snapshot, .snapshotGrowth = QuadTree::SnapshotGrowth::Adaptive,
        .boundary = boundary});
    CHECK(made.ok());
    auto tree = std::move(made).value();
    std::vector<QuadTree::BuildEntry<std::uint32_t, T>> input;
    for (std::uint32_t i = 0; i < n; ++i)
        input.push_back({boxes[i], i, {}, bench::mix64(i ^ 0x72616e6bu)});
    std::vector<Index::HandleType> handles(n), dirty;
    CHECK(tree.build(input, handles, {.workspace = &workspace}) == QuadTree::Status::Ok);
    for (std::uint32_t i = 0; i < n; i += 61)
        dirty.push_back(handles[i]);
    for (bool moved : {false, true})
        for (bool ordered : {false, true})
        {
            auto query = [&](QuadTree::WorkBudget budget, std::vector<Pair>& out)
            {
                auto collect = [&](const auto& a, const auto& b) noexcept
                {
                    out.emplace_back(a.payload, b.payload);
                    return QuadTree::Continue;
                };
                const QuadTree::QueryOptions<T> options{.ordered = ordered, .workspace = &workspace, .budget = budget};
                return moved ? tree.forEachPairInvolving(dirty, collect, options) : tree.forEachPair(collect, options);
            };
            std::vector<Pair> full;
            CHECK(query({}, full).ok());
            std::set<Pair> fullSet;
            for (auto [a, b] : full)
                CHECK(fullSet.insert({std::min(a, b), std::max(a, b)}).second);
            for (std::uint64_t limit : {0ull, 1ull, 7ull, 97ull, 1009ull, 20011ull})
                for (bool nodes : {false, true})
                {
                    std::vector<Pair> part;
                    QuadTree::WorkBudget budget;
                    (nodes ? budget.nodeVisits : budget.candidateTests) = limit;
                    const auto r = query(budget, part);
                    CHECK(r.status == QuadTree::Status::Ok || r.status == QuadTree::Status::BudgetExhausted);
                    if (r.status == QuadTree::Status::Ok)
                        CHECK(part.size() == full.size());
                    if (ordered)
                        CHECK(part.size() <= full.size() && std::equal(part.begin(), part.end(), full.begin()));
                    else
                    {
                        std::set<Pair> seen;
                        for (auto [a, b] : part)
                        {
                            const Pair p{std::min(a, b), std::max(a, b)};
                            CHECK(fullSet.contains(p) && seen.insert(p).second);
                        }
                    }
                }
        }
}
} // namespace

int main()
{
    for (const char* scene : {"uniform", "clustered", "split-line", "split-line-translated", "mixed-size", "outliers"})
        for (std::uint32_t n : {100u, 1000u, 10000u})
            for (auto mode : {QuadTree::Boundary::Default, QuadTree::Boundary::TouchInclusive})
                run(scene, n, mode);
    std::printf("budget contract: %lu checks passed\n", checks);
}
