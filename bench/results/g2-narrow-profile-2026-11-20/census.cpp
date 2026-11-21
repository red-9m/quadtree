// Count-only, independent of the sampled Release binary.
#include "bench-index.h"

#include <cinttypes>
#include <vector>

namespace
{
using namespace QuadTree;
using T = std::int32_t;

void count(const char* scene, std::size_t packet)
{
    constexpr std::size_t n = 100000;
    const auto boxes = bench::convert<T>(bench::generate(scene, n, bench::sceneSeed(1, scene, n, 0)));
    auto made = Index<std::uint32_t, T>::create({.extent = Extent::Snapshot, .snapshotGrowth = SnapshotGrowth::Adaptive});
    if (!made.ok()) std::abort();
    auto& index = made.value();
    Workspace<T> workspace;
    if (workspace.reserve({n, 0, n}) != Status::Ok) std::abort();
    std::vector<BuildEntry<std::uint32_t, T>> entries;
    entries.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i)
        entries.push_back({boxes[i], i, {}, bench::mix64(i ^ 0x72616e6bu)});
    if (index.build(entries, {.workspace = &workspace}) != Status::Ok) std::abort();
    std::vector<std::uint32_t> stamps(index.stats().nodes);
    auto& c = detail::orderedCensus;
    c = {};
    c.stamps = stamps.data();
    c.packet = packet;
    std::uint64_t checksum = 0;
    const auto result = index.forEachPair([&](const auto& a, const auto& b) noexcept
    {
        checksum += bench::mix64((std::uint64_t(std::min(a.payload, b.payload)) << 32) | std::max(a.payload, b.payload));
        return Continue;
    }, {.ordered = true, .workspace = &workspace, .budget = {.candidateTests = 200000000}});
    if (!result.complete() || c.source + 2 != n || n - 1 + c.occupied != result.counters.nodeVisits ||
        c.filtered != 0) std::abort();
    std::printf("%s,%zu,%zu,%" PRIu64 ",%" PRIx64, scene, n, packet, result.counters.reports, bench::mix64(checksum));
    for (const auto value : {result.counters.candidateTests, result.counters.nodeVisits, c.groups, c.covered,
             c.coveredOccupied, c.occupied, c.overlapping, c.distinctGroups, c.upperGroups, c.upperDistinctGroups,
             c.fullGroups, c.trailingCharges})
        std::printf(",%" PRIu64, value);
    for (const auto value : c.histogram) std::printf(",%" PRIu64, value);
    std::puts("");
    c.stamps = nullptr;
}
} // namespace

int main()
{
    std::puts("scene,n,packet,pairs,checksum,candidates,node_visits,groups,covered_groups,covered_children,"
        "occupied_children,overlapping_children,distinct_groups,upper_groups,upper_distinct_groups,"
        "full_groups,trailing_charges,admit_0,admit_1,admit_2,admit_3,admit_4");
    for (const auto* scene : {"split-line-translated", "mixed-size"})
        for (const auto packet : {4u, 8u}) count(scene, packet);
}
