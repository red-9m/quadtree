// Deterministic transcript of unordered self-pair results on 0..5-entry flat indexes.
// Build against control and candidate trees and diff the output; any difference is a semantic change.
#include "quadtree-index.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

using namespace QuadTree;
using B = Box<std::int32_t>;
using E = BuildEntry<std::uint32_t, std::int32_t>;

template<bool Instrumented>
using I = Index<std::uint32_t, std::int32_t, Unchecked, Instrumented>;

static void check(bool ok)
{
    if (!ok)
        std::abort();
}

template<typename Index>
static void run(const Index& index, std::uint64_t needed, unsigned stopAt, int predicateMode, std::uint64_t budget)
{
    std::vector<std::pair<std::uint32_t, std::uint32_t>> seen;
    unsigned reports = 0;
    auto visitor = [&](const auto& a, const auto& b) noexcept
    {
        seen.emplace_back(a.payload, b.payload);
        return ++reports == stopAt ? Stop : Continue;
    };
    QueryOptions<std::int32_t> options{.budget = {.candidateTests = budget}};
    QueryResult result;
    if (predicateMode == 0)
        result = index.forEachPair(visitor, options);
    else
        result = index.forEachPair(visitor, options, [&](const auto& a, const auto& b) noexcept
            { return ((a.payload + b.payload) & 1u) == (predicateMode == 1 ? 0u : 1u); });
    std::printf(" [n=%llu stop=%u pred=%d budget=%llu] status=%u nv=%llu ct=%llu fr=%llu pc=%llu rep=%llu :",
        static_cast<unsigned long long>(needed), stopAt, predicateMode, static_cast<unsigned long long>(budget),
        static_cast<unsigned>(result.status), static_cast<unsigned long long>(result.counters.nodeVisits),
        static_cast<unsigned long long>(result.counters.candidateTests),
        static_cast<unsigned long long>(result.counters.filterRejections),
        static_cast<unsigned long long>(result.counters.predicateCalls),
        static_cast<unsigned long long>(result.counters.reports));
    for (auto [a, b] : seen)
        std::printf(" %u-%u", a, b);
    std::printf("\n");
}

template<bool Instrumented>
static void scenario(std::mt19937& rng, unsigned trial)
{
    Config<std::int32_t> config;
    const bool snapshot = trial % 2 == 0;
    config.boundary = trial % 3 == 0 ? Boundary::TouchInclusive : Boundary::Default;
    if (snapshot)
    {
        config.extent = Extent::Snapshot;
        config.snapshotGrowth = SnapshotGrowth::Adaptive;
    }
    else
        config.extent = Extent::Adaptive;
    auto made = I<Instrumented>::create(config);
    check(made.ok());
    auto index = std::move(made).value();
    const unsigned n = rng() % 6;
    std::vector<E> entries;
    for (unsigned i = 0; i < n; ++i)
    {
        const std::int32_t x = std::int32_t(rng() % 8), y = std::int32_t(rng() % 8);
        const std::int32_t w = std::int32_t(rng() % 4), h = std::int32_t(rng() % 4);
        Filter filter;
        if (rng() % 3 == 0)
            filter = {std::uint64_t(1) << (rng() % 3), (std::uint64_t(1) << (rng() % 3)) | (rng() % 2 ? 2u : 0u)};
        entries.push_back({B{x, y, x + w, y + h}, 100 + i, filter});
    }
    if (snapshot)
        check(index.build(entries) == Status::Ok);
    else
        for (const auto& e : entries)
            check(index.insert(e.rect, e.payload, e.filter).status == Status::Ok);
    std::printf("trial %u instr=%d snapshot=%d boundary=%u n=%u\n", trial, int(Instrumented), int(snapshot),
        unsigned(config.boundary), n);
    const std::uint64_t needed = n < 2 ? 0 : std::uint64_t(n) * (n - 1) / 2;
    const std::array<std::uint64_t, 4> budgets{~std::uint64_t(0), needed, needed ? needed - 1 : 0, 0};
    for (auto budget : budgets)
        for (unsigned stopAt : {0u, 1u, 2u})
            for (int predicate : {0, 1, 2})
                run(index, needed, stopAt, predicate, budget);
}

int main()
{
    std::mt19937 rng(20261121);
    for (unsigned trial = 0; trial < 400; ++trial)
    {
        scenario<true>(rng, trial);
        scenario<false>(rng, trial);
    }
}
