// Deterministic transcript of prepared/unprepared int32 snapshot builds (0..6 entries).
// Build against control and candidate trees and diff the output. Covers workspace capacity,
// handle output, ranks (fallback), invalid geometry, Fixed/Adaptive/Snapshot extents,
// old-handle invalidation, fresh owners, extent publication and pair results after the build.
#include "quadtree-index.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <random>
#include <vector>

using namespace QuadTree;
using B = Box<std::int32_t>;
using E = BuildEntry<std::uint32_t, std::int32_t>;
using I = Index<std::uint32_t, std::int32_t>;
using H = I::HandleType;

static void check(bool ok)
{
    if (!ok)
        std::abort();
}

static void printBox(const std::optional<B>& b)
{
    if (b)
        std::printf("[%d,%d,%d,%d]", b->minX, b->minY, b->maxX, b->maxY);
    else
        std::printf("none");
}

int main()
{
    std::mt19937 rng(7031);
    for (unsigned trial = 0; trial < 3000; ++trial)
    {
        Config<std::int32_t> config;
        switch (trial % 4)
        {
        case 0: config.extent = Extent::Snapshot; config.snapshotGrowth = SnapshotGrowth::Adaptive; break;
        case 1: config.extent = Extent::Snapshot; config.snapshotGrowth = SnapshotGrowth::Reject; break;
        case 2: config.extent = Extent::Adaptive; break;
        default: config.extent = Extent::Fixed; config.root = B{-2, -2, 12, 12}; break;
        }
        if (trial % 5 == 0)
            config.smallSetThreshold = 2;
        auto made = I::create(config);
        check(made.ok());
        auto index = std::move(made).value();
        Workspace<std::int32_t> scratch;
        const std::size_t reserve = rng() % 8;
        check(scratch.reserve({0, 0, reserve}) == Status::Ok);
        std::vector<H> previous;
        std::printf("trial %u extent=%u small=%u ws=%zu\n", trial, unsigned(config.extent),
            config.smallSetThreshold, reserve);
        for (unsigned step = 0; step < 6; ++step)
        {
            const unsigned n = rng() % 7;
            std::vector<E> input;
            for (unsigned i = 0; i < n; ++i)
            {
                std::int32_t x = std::int32_t(rng() % 12) - 1, y = std::int32_t(rng() % 12) - 1;
                std::int32_t w = std::int32_t(rng() % 4), h = std::int32_t(rng() % 4);
                if (rng() % 40 == 0)
                    w = -1; // invalid geometry
                E e{B{x, y, x + w, y + h}, 1000 * step + i, {}, std::nullopt};
                if (rng() % 4 == 0)
                    e.filter = {1u << (rng() % 2), 1u << (rng() % 2)};
                if (rng() % 6 == 0)
                    e.rank = rng() % 5; // duplicates possible
                input.push_back(e);
            }
            const bool prepared = rng() % 4 != 0;
            const bool wantHandles = rng() % 2 == 0;
            const std::size_t outSize = wantHandles ? (rng() % 5 == 0 && n ? n - 1 : n) : 0;
            std::vector<H> out(outSize);
            const BuildOptions<std::int32_t> options{.workspace = prepared ? &scratch : nullptr};
            Status status;
            if (wantHandles)
                status = index.build(input, std::span<H>(out), options);
            else
                status = index.build(input, options);
            std::printf(" step %u n=%u prep=%d h=%d/%zu status=%u size=%zu extent=", step, n, int(prepared),
                int(wantHandles), outSize, unsigned(status), index.size());
            printBox(index.extent());
            std::printf(" oldvalid=");
            for (const auto& h : previous)
                std::printf("%d", int(index.valid(h)));
            if (status == Status::Ok && wantHandles)
            {
                std::printf(" new=");
                for (std::size_t i = 0; i < n; ++i)
                {
                    check(index.valid(out[i]));
                    const auto& r = index.rect(out[i]);
                    std::printf("%u:%d,%d,%d,%d:%llu:%d ", index.payload(out[i]), r.minX, r.minY, r.maxX, r.maxY,
                        static_cast<unsigned long long>(index.filter(out[i]).category),
                        index.rank(out[i]) ? int(*index.rank(out[i])) : -1);
                    for (const auto& h : previous)
                        check(!(h == out[i]));
                }
                previous.assign(out.begin(), out.end());
            }
            std::printf(" entries=");
            const auto all = index.forEachEntry([](const auto& v) noexcept
            {
                std::printf("%u,", v.payload);
                return Continue;
            });
            check(all.ok());
            std::printf(" pairs=");
            const auto pairs = index.forEachPair([](const auto& a, const auto& b) noexcept
            {
                std::printf("%u-%u,", a.payload, b.payload);
                return Continue;
            });
            std::printf(" ps=%u ct=%llu\n", unsigned(pairs.status),
                static_cast<unsigned long long>(pairs.counters.candidateTests));
        }
    }
}
