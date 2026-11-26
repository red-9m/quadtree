// Transcript of warm flat replacements (0..6 entries) with invalid, ranked and signed-zero input.
#include "quadtree-index.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>
#include <vector>
using namespace QuadTree;

template<typename T>
void run(const char* name)
{
    using I = Index<std::uint32_t, T>;
    using E = BuildEntry<std::uint32_t, T>;
    std::mt19937_64 rng(42);
    const T specials[] = {T(0), T(-0.0), T(1), T(-1), T(5), std::numeric_limits<T>::max(), std::numeric_limits<T>::lowest(),
        std::numeric_limits<T>::has_infinity ? std::numeric_limits<T>::infinity() : T(7),
        std::numeric_limits<T>::has_quiet_NaN ? std::numeric_limits<T>::quiet_NaN() : T(-7)};
    auto value = [&]() -> T { return rng() % 4 == 0 ? specials[rng() % 9] : T(static_cast<int>(rng() % 21) - 10); };
    for (int cfg = 0; cfg < 2; ++cfg)
    {
        Config<T> config;
        if (cfg) { config.extent = Extent::Snapshot; config.snapshotGrowth = SnapshotGrowth::Adaptive; }
        auto made = I::create(config);
        if (!made.ok()) { std::printf("create %u\n", unsigned(made.status)); std::abort(); }
        auto tree = std::move(made).value();
        Workspace<T> ws;
        (void)ws.reserve({8, 0, 8});
        (void)tree.reserve({.entries = 8, .rankedEntries = 8});
        std::vector<typename I::HandleType> prev;
        for (int it = 0; it < 20000; ++it)
        {
            const std::size_t n = rng() % 7;
            std::vector<E> in(n);
            for (std::size_t i = 0; i < n; ++i)
            {
                T a = value(), b = value(), c = value(), d = value();
                if (rng() % 3) { if (!(a <= c)) std::swap(a, c); if (!(b <= d)) std::swap(b, d); }
                in[i] = {Box<T>{a, b, c, d}, static_cast<std::uint32_t>(i), {rng() & 3, rng() & 3},
                    rng() % 8 == 0 ? std::optional<std::uint64_t>(rng() % 4) : std::nullopt};
            }
            std::vector<typename I::HandleType> out(n);
            const auto before = detail::moduleIdentityCounter.load();
            const bool withHandles = rng() & 1;
            const Status s = withHandles ? tree.build(in, std::span(out), {.workspace = &ws}) : tree.build(in, {.workspace = &ws});
            const auto used = detail::moduleIdentityCounter.load() - before;
            std::printf("%s %d %d n=%zu s=%u id=%llu size=%zu", name, cfg, it, n, unsigned(s), (unsigned long long)used, tree.size());
            if (auto e = tree.extent())
                std::printf(" ext=%a,%a,%a,%a", double(e->minX), double(e->minY), double(e->maxX), double(e->maxY));
            std::size_t validPrev = 0;
            for (auto h : prev) validPrev += tree.valid(h);
            std::printf(" prev=%zu", validPrev);
            (void)tree.forEachEntry([&](const auto& v) noexcept {
                std::printf(" [%u %a %a %a %a %d%d r%lld]", v.payload, double(v.rect.minX), double(v.rect.minY),
                    double(v.rect.maxX), double(v.rect.maxY), int(std::signbit(double(v.rect.minX))), int(std::signbit(double(v.rect.maxY))),
                    v.rank ? (long long)*v.rank : -1LL);
                return Continue; });
            unsigned pairs = 0;
            const auto r = tree.forEachPair([&](const auto&, const auto&) noexcept { ++pairs; return Continue; });
            std::printf(" pairs=%u/%u", pairs, unsigned(r.status));
            if (s == Status::Ok && withHandles)
            {
                for (auto h : out) std::printf(" v%d", int(tree.valid(h)));
                prev = out;
            }
            std::printf("\n");
        }
    }
}

int main()
{
    run<std::int32_t>("i32");
    run<std::int64_t>("i64");
    run<float>("f32");
    run<double>("f64");
}
