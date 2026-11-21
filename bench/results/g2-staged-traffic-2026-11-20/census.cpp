// Untimed logical-access census. The adapter matches the frozen snapshot fixture.
#include "bench-index.h"

#include <cinttypes>
#include <cstdio>

namespace
{
template<bool Ordered>
void count(const char* scene, std::size_t n)
{
    const auto source = bench::generate(scene, n, bench::sceneSeed(1, scene, n, 0));
    const auto boxes = bench::convert<std::int32_t>(source);
    bench::LibraryIndex<std::int32_t, Ordered> index;
    index.build(boxes, bench::Boundary::Default);
    auto& depths = QuadTree::detail::bulkTraffic;
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
    if (result.capped || allocs != 0)
        std::abort();
    const auto nodes = index.mTree->stats().nodes; // Probe compiled with -fno-access-control.
    const auto hash = bench::mix64(n ^ bench::sceneHash(source));
    for (std::size_t i = 0; i < std::size(depths); ++i)
    {
        const auto& d = depths[i];
        if (d.calls == 0)
            continue;
        std::printf("%s,%zu,%s,%zu,%zu,%zu,%" PRIx64 ",%" PRIu64 ",%" PRIx64,
            scene, n, index.name.data(), i, nodes, allocs, hash, pairs, bench::mix64(checksum));
        for (const auto value : {result.candidates, result.nodeVisits, result.references,
                 std::uint64_t(result.maxReferences), result.cellTests, d.calls, d.input, d.classifyNodes,
                 d.classifyIDs, d.classifyChildIDs, d.lookaheadIDs, d.prefixIDs, d.splitNodes, d.scatterIDs, d.childIDs,
                 d.finishCopyIDs, d.ownCopyIDs, d.monotoneNodes, d.monotoneIDs,
                 d.monotoneMovedOwnCopyIDs, d.monotoneSavedOwnCopyIDs, d.singleBinNodes, d.singleBinIDs})
            std::printf(",%" PRIu64, value);
        std::puts("");
    }
}
} // namespace

int main()
{
    std::puts("scene,n,engine,depth,nodes,allocs,scene_hash,pairs,checksum,candidates,node_visits,references,"
        "max_refs,cell_tests,calls,input_ids,classify_nodes,classify_ids,classify_child_ids,lookahead_ids,prefix_ids,split_nodes,"
        "scatter_ids,child_ids,finish_copy_ids,own_copy_ids,monotone_nodes,monotone_ids,"
        "monotone_added_own_copy_ids,monotone_removed_own_copy_ids,single_bin_nodes,single_bin_ids");
    for (const auto* scene: {"uniform", "split-line-translated"})
        for (std::size_t n : {10000u, 100000u})
        {
            count<false>(scene, n);
            count<true>(scene, n);
        }
}
