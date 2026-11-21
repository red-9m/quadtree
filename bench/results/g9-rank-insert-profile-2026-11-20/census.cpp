// Untimed rank-set census against the exact benchmark adapter and seeds.
#include "bench-index.h"

#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <set>

namespace
{
using T = std::int32_t;
using Engine = bench::LibraryIndex<T, true>;

void check(bool ok)
{
    if (!ok) std::abort();
}

void count(const char* scene, std::size_t n)
{
    const auto source = bench::generate(scene, n, bench::sceneSeed(1, scene, n, 0));
    const auto boxes = bench::convert<T>(source);
    Engine index;
    index.build(boxes, bench::Boundary::Default);
    const auto& store = index.mTree->mEntries; // -fno-access-control, diagnostic only.
    const auto cold = store.rankStats();
    const auto beforeAllocations = bench::memory::allocations();
    index.build(boxes, bench::Boundary::Default);
    const auto warm = store.rankStats();
    const auto buildAllocations = bench::memory::allocations() - beforeAllocations;
    const auto capacity = store.mRankCapacity;
    const auto rankCount = store.mRankCount;
    const auto probes = warm.probes - cold.probes;
    const auto resets = warm.cellResets - cold.cellResets;
    const auto conversions = warm.conversions - cold.conversions;
    check(rankCount == n && capacity >= 2 * n && (capacity & (capacity - 1)) == 0);

    // In hash mode with no erases, probes = sum(displacement + 1).
    // Group by exact displacement (cut points expose the tail).
    std::array<std::uint64_t, 6> hist{};
    std::size_t maxDistance = 0, bucketCount = 0;
    std::uint64_t recomputed = 0;
    std::set<std::uintptr_t> homeLines, usedLines;
    if (!warm.tree)
    {
        for (std::size_t i = 0; i < capacity; ++i)
        {
            if (!store.occupied(i))
                continue;
            const auto home = store.homeOf(store.mRanks[i].bucket.rank);
            const auto distance = (i + capacity - home) & (capacity - 1);
            homeLines.insert(reinterpret_cast<std::uintptr_t>(&store.mRanks[home].bucket.epoch) / 64);
            usedLines.insert(reinterpret_cast<std::uintptr_t>(&store.mRanks[i].bucket.epoch) / 64);
            maxDistance = std::max(maxDistance, distance);
            ++hist[distance < 4 ? distance : distance < 8 ? 4 : 5];
            recomputed += distance + 1;
            ++bucketCount;
        }
        check(bucketCount == n && recomputed == probes && conversions == 0);
    }

    const auto beforePairs = bench::memory::allocations();
    std::uint64_t pairs = 0, checksum = 0;
    auto sink = [&](std::uint32_t a, std::uint32_t b) noexcept
    {
        ++pairs;
        checksum += bench::mix64((std::uint64_t(std::min(a, b)) << 32) | std::max(a, b));
    };
    const auto result = index.pairs(sink, 200000000);
    const auto pairAllocations = bench::memory::allocations() - beforePairs;
    check(!result.capped && result.orderViolations == 0 && buildAllocations == 0 && pairAllocations == 0);
    check(store.rankStats().probes == warm.probes);
    std::printf("%s,%zu,%s,%" PRIx64 ",%" PRIu64 ",%" PRIx64, scene, n, index.name.data(),
        bench::mix64(n ^ bench::sceneHash(source)), pairs, bench::mix64(checksum));
    for (const auto x: {result.candidates, result.nodeVisits, result.references, std::uint64_t(result.maxReferences),
             result.cellTests, std::uint64_t(buildAllocations + pairAllocations), std::uint64_t(rankCount),
             std::uint64_t(capacity), std::uint64_t(sizeof(*store.mRanks)), cold.probes, cold.cellResets,
             cold.conversions, probes, resets, conversions, std::uint64_t(warm.tree), recomputed,
             std::uint64_t(maxDistance), std::uint64_t(homeLines.size()), std::uint64_t(usedLines.size())})
        std::printf(",%" PRIu64, x);
    for (const auto x: hist)
        std::printf(",%" PRIu64, x);
    std::puts("");
}
} // namespace

int main()
{
    std::puts("scene,n,engine,scene_hash,pairs,checksum,candidates,node_visits,references,max_refs,cell_tests,"
        "allocs,rank_count,capacity,cell_bytes,cold_probes,cold_resets,cold_conversions,warm_probes,warm_resets,"
        "warm_conversions,tree,warm_probes_from_displacement,max_displacement,home_epoch_lines,occupied_epoch_lines,"
        "distance_0,distance_1,distance_2,distance_3,distance_4_7,distance_8_plus");
    for (const auto* scene: {"uniform", "clustered", "split-line", "split-line-translated", "mixed-size", "outliers"})
        for (std::size_t n: {1000u, 10000u, 100000u})
            count(scene, n);
}
