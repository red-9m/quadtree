// Deterministic transcript for float/double builds of 0-70 entries (flat path and small trees): status,
// extent and stored rect bit patterns (signed zeros), handle mapping, old-handle validity, and pair results
// with counters, for both boundaries, prepared/unprepared builds and invalid input (NaN, infinity, inverted).
// Build against control and candidate trees and diff the output.
#include "quadtree-index.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <vector>

using namespace QuadTree;

template<typename T>
using Bits = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;

template<typename T>
void emitBox(const Box<T>& b)
{
    std::printf(" %llx,%llx,%llx,%llx", static_cast<unsigned long long>(std::bit_cast<Bits<T>>(b.minX)),
        static_cast<unsigned long long>(std::bit_cast<Bits<T>>(b.minY)),
        static_cast<unsigned long long>(std::bit_cast<Bits<T>>(b.maxX)),
        static_cast<unsigned long long>(std::bit_cast<Bits<T>>(b.maxY)));
}

template<typename T>
void trial(std::mt19937& rng, unsigned id)
{
    using I = Index<std::uint32_t, T>;
    using E = BuildEntry<std::uint32_t, T>;
    Config<T> config;
    config.boundary = id % 2 ? Boundary::TouchInclusive : Boundary::Default;
    if (id % 3 == 0)
        config.extent = Extent::Adaptive;
    else
    {
        config.extent = Extent::Snapshot;
        config.snapshotGrowth = SnapshotGrowth::Adaptive;
    }
    auto made = I::create(config);
    if (!made.ok())
        std::abort();
    auto index = std::move(made).value();
    Workspace<T> scratch;
    if (scratch.reserve({0, 0, 80}) != Status::Ok)
        std::abort();
    std::vector<typename I::HandleType> previous;
    std::printf("trial %u %zu\n", id, sizeof(T));
    for (unsigned step = 0; step < 4; ++step)
    {
        const unsigned n = rng() % 5 == 0 ? 65 + rng() % 6 : rng() % 12;
        std::vector<E> input;
        const T values[] = {T(-0.0), T(0.0), T(1), T(-1), T(2.5), T(3), T(-2), T(4)};
        for (unsigned i = 0; i < n; ++i)
        {
            T x = values[rng() % 8] + T(rng() % 4), y = values[rng() % 8];
            T w = T(rng() % 3), h = T(rng() % 3);
            Box<T> b{x, y, x + w, y + h};
            if (rng() % 4 == 0) b.minX = T(-0.0);
            if (rng() % 5 == 0) b.maxY = T(-0.0) < b.minY ? b.minY : T(-0.0);
            const auto bad = rng() % 60;
            if (bad == 0) b.minX = std::numeric_limits<T>::quiet_NaN();
            if (bad == 1) b.maxY = std::numeric_limits<T>::infinity();
            if (bad == 2) b = {T(2), T(0), T(1), T(1)};
            input.push_back({b, 100 * step + i, rng() % 6 == 0 ? Filter{1, 2} : Filter{}, std::nullopt});
        }
        std::vector<typename I::HandleType> out(n);
        const bool prepared = rng() % 3 != 0;
        const auto status = index.build(input, std::span<typename I::HandleType>(out),
            {.workspace = prepared ? &scratch : nullptr});
        std::printf(" step %u n=%u prep=%d status=%u size=%zu", step, n, int(prepared), unsigned(status), index.size());
        if (const auto e = index.extent())
            emitBox(*e);
        std::printf(" old=");
        for (const auto& h : previous)
            std::printf("%d", int(index.valid(h)));
        if (status == Status::Ok)
        {
            std::printf(" rects=");
            for (const auto& h : out)
            {
                std::printf(" %u", index.payload(h));
                emitBox(index.rect(h));
            }
            previous = out;
        }
        for (bool tight : {false, true})
        {
            std::vector<std::pair<std::uint32_t, std::uint32_t>> pairs;
            const auto r = index.forEachPair([&](const auto& a, const auto& b) noexcept
            {
                pairs.emplace_back(std::min(a.payload, b.payload), std::max(a.payload, b.payload));
                return Continue;
            }, {.budget = {.candidateTests = tight ? 3u : std::numeric_limits<std::uint64_t>::max()}});
            std::sort(pairs.begin(), pairs.end());
            std::printf(" | st=%u ct=%llu fr=%llu rep=%llu:", unsigned(r.status),
                static_cast<unsigned long long>(r.counters.candidateTests),
                static_cast<unsigned long long>(r.counters.filterRejections),
                static_cast<unsigned long long>(r.counters.reports));
            if (!tight)
                for (auto [a, b] : pairs)
                    std::printf(" %u-%u", a, b);
        }
        std::printf("\n");
    }
}

int main()
{
    std::mt19937 rng(9091);
    for (unsigned id = 0; id < 1500; ++id)
    {
        trial<float>(rng, id);
        trial<double>(rng, id);
    }
}
