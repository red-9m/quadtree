// Count-only probe using the frozen adapter and fixtures; never use for timing.
#include "bench-index.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>

namespace
{
template<bool Ordered>
void count(const char* scene, std::size_t n)
{
    const auto source = bench::generate(scene, n, bench::sceneSeed(1, scene, n, 0));
    const auto boxes = bench::convert<std::int32_t>(source);
    bench::LibraryIndex<std::int32_t, Ordered> index;
    index.build(boxes, bench::Boundary::Default);
    auto& depths = QuadTree::detail::scatterCensus;
    for (auto& d: depths)
        d = {};
    const auto before = bench::memory::allocations();
    index.build(boxes, bench::Boundary::Default);
    std::uint64_t pairs = 0, checksum = 0;
    auto sink = [&](std::uint32_t a, std::uint32_t b) noexcept
    {
        ++pairs;
        checksum += bench::mix64((std::uint64_t(std::min(a, b)) << 32) | std::max(a, b));
    };
    const auto result = index.pairs(sink, 200000000);
    const auto allocs = bench::memory::allocations() - before;
    if (result.capped || result.orderViolations != 0 || allocs != 0)
        std::abort();
    const auto nodes = index.mTree->stats().nodes; // Diagnostic-only access.
    const auto hash = bench::mix64(n ^ bench::sceneHash(source));
    for (std::size_t depth = 0; depth < std::size(depths); ++depth)
    {
        const auto& d = depths[depth];
        if (d.groups == 0)
            continue;
        std::printf("%s,%zu,%s,%zu,%zu,%zu,%" PRIx64 ",%" PRIu64 ",%" PRIx64,
            scene, n, index.name.data(), depth, nodes, allocs, hash, pairs, bench::mix64(checksum));
        for (const auto value: {result.candidates, result.nodeVisits, result.references,
                 std::uint64_t(result.maxReferences), result.cellTests, d.groups, d.ids, d.first, d.gap1,
                 d.gap2, d.gap3to4, d.gap5to8, d.gap9plus, d.singleBinGroups, d.singleBinIds,
                 d.maxRun, d.halfPairs, d.sameTagHalfPairs})
            std::printf(",%" PRIu64, value);
        for (const auto value: d.tags)
            std::printf(",%" PRIu64, value);
        std::printf(",%.8Lf\n", d.expectedAdjacent);
    }
}
} // namespace

int main()
{
    std::puts("scene,n,engine,depth,nodes,allocs,scene_hash,pairs,checksum,candidates,node_visits,references,"
        "max_refs,cell_tests,groups,ids,first,gap_1,gap_2,gap_3_4,gap_5_8,gap_9_plus,single_bin_groups,"
        "single_bin_ids,max_run,half_pairs,same_tag_half_pairs,tag_0,tag_1,tag_2,tag_3,tag_4,expected_adjacent");
    for (const auto* scene: {"uniform", "split-line-translated"})
        for (const std::size_t n: {10000u, 100000u})
        {
            count<false>(scene, n);
            count<true>(scene, n);
        }
}
