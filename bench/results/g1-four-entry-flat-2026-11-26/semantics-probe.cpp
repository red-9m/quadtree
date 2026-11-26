// Diff this deterministic unordered self-pair transcript across builds.
#include "quadtree-index.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <span>

using namespace QuadTree;

static void check(bool ok)
{
    if (!ok)
        std::abort();
}

template<typename I, typename T>
static void run(const I& index, std::uint64_t budget, unsigned stopAt, int predicateMode)
{
    std::array<unsigned, 10> reports{}, predicates{};
    unsigned seen = 0, tested = 0;
    const auto pair = [](const auto& a, const auto& b) noexcept { return 10 * a.payload + b.payload; };
    const auto visitor = [&](const auto& a, const auto& b) noexcept
    {
        check(seen < reports.size());
        reports[seen++] = pair(a, b);
        return seen == stopAt ? Stop : Continue;
    };
    const QueryOptions<T> options{.budget = {.candidateTests = budget, .nodeVisits = 0}};
    const auto result = predicateMode == 0 ? index.forEachPair(visitor, options) :
        index.forEachPair(visitor, options, [&](const auto& a, const auto& b) noexcept
        {
            check(tested < predicates.size());
            predicates[tested++] = pair(a, b);
            return ((a.payload + b.payload) & 1u) == unsigned(predicateMode - 1);
        });
    const auto& c = result.counters;
    std::printf("budget=%llu stop=%u pred=%d s=%u n=%llu c=%llu f=%llu p=%llu r=%llu",
        static_cast<unsigned long long>(budget), stopAt, predicateMode, unsigned(result.status),
        static_cast<unsigned long long>(c.nodeVisits), static_cast<unsigned long long>(c.candidateTests),
        static_cast<unsigned long long>(c.filterRejections), static_cast<unsigned long long>(c.predicateCalls),
        static_cast<unsigned long long>(c.reports));
    std::printf(" predicates:");
    for (unsigned i = 0; i < tested; ++i) std::printf(" %02u", predicates[i]);
    std::printf(" reports:");
    for (unsigned i = 0; i < seen; ++i) std::printf(" %02u", reports[i]);
    std::printf("\n");
}

template<typename T, bool Instrumented>
static void scenario(std::mt19937& rng, unsigned trial)
{
    using I = Index<unsigned, T, Checked, Instrumented>;
    using E = BuildEntry<unsigned, T>;
    Config<T> cfg;
    cfg.boundary = trial % 2 ? Boundary::Default : Boundary::TouchInclusive;
    cfg.smallSetThreshold = 5;
    const bool snapshot = trial % 3 != 0;
    if (snapshot)
    {
        cfg.extent = Extent::Snapshot;
        cfg.snapshotGrowth = SnapshotGrowth::Adaptive;
    }
    auto made = I::create(cfg);
    check(made.ok());
    auto index = std::move(made).value();
    Workspace<T> scratch;
    check(index.reserve({.entries = 5, .rankedEntries = 5}) == Status::Ok);
    check(scratch.reserve({0, 0, 5}) == Status::Ok);
    const unsigned n = (trial / 3) % 6;
    std::array<E, 5> entries{};
    std::array<typename I::HandleType, 5> handles{};
    for (unsigned i = 0; i < n; ++i)
    {
        T base = -8;
        if constexpr (std::same_as<T, std::int64_t>)
        {
            if (trial % 5 == 0) base = (T(1) << 32) - 8;
            if (trial % 5 == 1) base = std::numeric_limits<T>::min() + 32;
            if (trial % 5 == 2) base = std::numeric_limits<T>::max() - 32;
            if (trial % 5 == 3 && i == n - 1) base = T(1) << 32;
        }
        const T x = base + T(rng() % 8), y = T(rng() % 8);
        const T w = T(rng() % 4), h = T(rng() % 4);
        Box<T> box{x, y, x + w, y + h};
        if constexpr (FloatCoordinate<T>)
            if (trial % 5 == 0 && i == 0)
            {
                const auto d = std::numeric_limits<T>::denorm_min();
                box = {-d, -T(0), d, d};
            }
        Filter filter;
        if (rng() % 3 == 0)
            filter = {std::uint64_t(1) << (rng() % 3), rng() & 7};
        entries[i] = {box, i, filter, (trial / 3) % 5 == 0 ? std::optional<std::uint64_t>(5 - i) : std::nullopt};
    }
    const auto input = std::span(entries).first(n);
    if (snapshot)
        check(index.build(input, handles, {.workspace = &scratch}) == Status::Ok);
    else
    {
        const auto insert = [&](const E& entry)
        {
            const auto result = index.insert(entry.rect, entry.payload, entry.filter, entry.rank);
            check(result.ok());
            return result.handle;
        };
        for (unsigned i = 0; i < n; ++i) handles[i] = insert(entries[i]);
        if (n > 1)
        {
            check(index.remove(handles[0]) == Status::Ok);
            insert(entries[0]); // Dense order no longer matches slot order.
        }
    }
    std::printf("trial=%u n=%u snapshot=%d boundary=%u instrumented=%d\n", trial, n, int(snapshot),
        unsigned(cfg.boundary), int(Instrumented));
    const unsigned needed = n < 2 ? 0 : n * (n - 1) / 2;
    for (unsigned cap = 0; cap <= needed + 1; ++cap)
        for (const unsigned stop : {0u, 1u, 2u})
            for (int pred = 0; pred < 3; ++pred)
                run<I, T>(index, cap <= needed ? cap : std::numeric_limits<std::uint64_t>::max(), stop, pred);
}

template<typename T>
static void type(const char* name)
{
    std::printf("%s\n", name);
    std::mt19937 rng(20261126);
    for (unsigned trial = 0; trial < 180; ++trial)
    {
        scenario<T, true>(rng, trial);
        scenario<T, false>(rng, trial);
    }
}

int main()
{
    type<std::int32_t>("int32");
    type<std::int64_t>("int64");
    type<float>("float");
    type<double>("double");
}
