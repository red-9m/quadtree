// Diagnostic: repeat a single warm batch using the unmodified library adapter.
#include "bench-index.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <string_view>
#include <vector>

namespace
{
using T = std::int32_t;
using Scene = std::vector<bench::Box<T>>;
constexpr auto boundary = bench::Boundary::Default;
constexpr std::uint64_t limit = 200'000'000;

struct PairSink
{
    std::uint64_t pairs = 0;
    std::uint64_t checksum = 0;
    void operator()(std::uint32_t a, std::uint32_t b) noexcept
    {
        const std::uint64_t lo = std::min(a, b);
        const std::uint64_t hi = std::max(a, b);
        ++pairs;
        checksum += bench::mix64((lo << 32) | hi);
    }
};

[[gnu::noinline]] void replace(bench::LibraryIndex<T>& engine, const Scene& scene)
{
    engine.build(scene, boundary);
}

[[gnu::noinline]] std::uint64_t scan(const bench::LibraryIndex<T>& engine, std::size_t scene)
{
    PairSink sink;
    const auto counters = engine.pairs(sink, limit);
    return bench::mix64(sink.checksum + scene) + counters.candidates;
}

[[gnu::noinline]] std::uint64_t step(bench::LibraryIndex<T>& engine, const std::array<Scene, 256>& batch)
{
    std::uint64_t checksum = 0;
    for (std::size_t k = 0; k < batch.size(); ++k)
    {
        replace(engine, batch[k]);
        checksum += scan(engine, k);
    }
    return checksum;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view scene = argv[1];
    const auto n = static_cast<std::size_t>(std::strtoul(argv[2], nullptr, 10));
    if ((scene != "uniform" && scene != "clustered") || n > 64)
        return 2;
    std::array<Scene, 256> batch;
    for (std::size_t k = 0; k < batch.size(); ++k)
        batch[k] = bench::convert<T>(bench::generate(scene, n, bench::sceneSeed(1, scene, n, k)));

    bench::LibraryIndex<T> engine;
    bench::BruteForce<T> brute;
    std::uint64_t expected = 0;
    for (std::size_t k = 0; k < batch.size(); ++k)
    {
        std::vector<std::pair<std::uint32_t, std::uint32_t>> actual, reference;
        auto sink = [](auto& pairs) { return [&pairs](auto a, auto b) { pairs.emplace_back(std::min(a, b), std::max(a, b)); }; };
        auto collect = sink(actual), collectBrute = sink(reference);
        engine.build(batch[k], boundary);
        brute.build(batch[k], boundary);
        const auto results = engine.pairs(collect, limit);
        const auto control = brute.pairs(collectBrute, limit);
        std::ranges::sort(actual);
        std::ranges::sort(reference);
        if (actual != reference || results.capped || control.capped || results.candidates != control.candidates)
            return 1;
        PairSink check;
        for (const auto& [a, b] : reference)
            check(a, b);
        expected += bench::mix64(check.checksum + k) + control.candidates;
    }

    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    auto end = start;
    const auto allocations = bench::memory::allocations();
    std::uint64_t checksum = 0, repeats = 0;
    do
    {
        for (unsigned i = 0; i < 64; ++i)
            checksum += step(engine, batch);
        repeats += 64;
        end = Clock::now();
    } while (end - start < std::chrono::seconds(1));
    const auto ns = std::chrono::duration<double, std::nano>(end - start).count();
    std::printf("%s n=%zu %.1f ns/scene (%llu batches)\n", argv[1], n, ns / (static_cast<double>(repeats) * 256),
        static_cast<unsigned long long>(repeats));
    return checksum != expected * repeats || bench::memory::allocations() != allocations ? 1 : 0;
}
