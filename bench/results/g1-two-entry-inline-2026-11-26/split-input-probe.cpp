// Diagnostic: warm two-entry build + pairs with the adapter's input buffer at a chosen page offset.
#include "bench-index.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace
{
using T = TYPE;
using Scene = std::vector<bench::Box<T>>;
using I = QuadTree::Index<std::uint32_t, T>;
using E = QuadTree::BuildEntry<std::uint32_t, T>;

struct PairSink
{
    std::uint64_t pairs = 0, checksum = 0;
    void operator()(std::uint32_t a, std::uint32_t b) noexcept
    {
        const std::uint64_t lo = std::min(a, b), hi = std::max(a, b);
        ++pairs;
        checksum += bench::mix64((lo << 32) | hi);
    }
};

struct Engine
{
    bench::IndexResource resource;
    std::optional<I> tree;
    QuadTree::Workspace<T> workspace{&resource};
    E* input;
    std::size_t size = 0;
    Engine(std::size_t offset)
    {
        QuadTree::Config<T> config;
        config.extent = QuadTree::Extent::Snapshot;
        config.snapshotGrowth = QuadTree::SnapshotGrowth::Adaptive;
        tree.emplace(I::create(config, &resource).value());
        (void)workspace.reserve({0, 0, 64});
        auto* raw = static_cast<unsigned char*>(std::aligned_alloc(4096, 3 * 4096));
        input = reinterpret_cast<E*>(raw + 4096 + offset);
    }
};

[[gnu::noinline]] std::uint64_t step(Engine& e, const std::array<Scene, 256>& batch)
{
    std::uint64_t checksum = 0;
    for (std::size_t k = 0; k < batch.size(); ++k)
    {
        PairSink sink;
        const auto& boxes = batch[k];
        for (std::size_t i = 0; i < boxes.size(); ++i)
            e.input[i] = {boxes[i], static_cast<std::uint32_t>(i), {}, std::nullopt};
        if (e.tree->build(std::span<const E>(e.input, boxes.size()), {.workspace = &e.workspace}) != QuadTree::Status::Ok)
            std::abort();
        const auto r = e.tree->forEachPair([&](const auto& a, const auto& b) noexcept
            { sink(a.payload, b.payload); return QuadTree::Continue; },
            {.workspace = &e.workspace, .budget = {.candidateTests = 1u << 30}});
        checksum += bench::mix64(sink.checksum + k) + r.counters.candidateTests;
    }
    return checksum;
}
} // namespace

int main(int argc, char** argv)
{
    const std::string_view scene = argv[1];
    const std::size_t n = std::strtoul(argv[2], nullptr, 10);
    std::array<Scene, 256> batch;
    for (std::size_t k = 0; k < batch.size(); ++k)
        batch[k] = bench::convert<T>(bench::generate(scene, n, bench::sceneSeed(1, scene, n, k)));
    for (int a = 3; a < argc; ++a)
    {
        const std::size_t offset = std::strtoul(argv[a], nullptr, 16);
        Engine e(offset);
        double best = 1e30;
        for (int rep = 0; rep < 30; ++rep)
        {
            const auto start = std::chrono::steady_clock::now();
            std::uint64_t c = 0;
            for (int i = 0; i < 100; ++i)
                c += step(e, batch);
            asm volatile("" ::"r"(c));
            best = std::min(best, std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / (100.0 * 256));
        }
        std::printf("%s n=%zu offset=%03zx %.2f ns/scene\n", scene.data(), n, offset, best);
    }
}
