#include "bench-index.h"
#include <cinttypes>

int main()
{
    std::puts("scene,n,pairs,checksum,candidates,node_visits,self_linked,self_1_3,self_4_7,self_8_plus,"
        "cross_linked,cross_1_3,cross_4_7,cross_8_plus,walk_linked,walk_1_3,walk_4_7,walk_8_plus,"
        "self_grouped,cross_grouped");
    for (auto scene : {"uniform", "split-line", "split-line-translated", "mixed-size"})
        for (std::size_t n : {10000u, 100000u})
        {
            for (auto& counts : QuadTree::detail::eligibleRangeTests)
                for (auto& value : counts) value = 0;
            for (auto& value : QuadTree::detail::groupCandidates) value = 0;
            const auto boxes = bench::convert<std::int32_t>(bench::generate(scene, n, bench::sceneSeed(1, scene, n, 0)));
            bench::LibraryIndex<std::int32_t> index;
            index.build(boxes, bench::Boundary::Default);
            std::uint64_t pairs = 0, checksum = 0;
            auto sink = [&](std::uint32_t a, std::uint32_t b) noexcept
            {
                ++pairs;
                checksum += bench::mix64((std::uint64_t(std::min(a, b)) << 32) | std::max(a, b));
            };
            const auto result = index.pairs(sink, 200000000);
            if (result.capped) std::abort();
            std::printf("%s,%zu,%" PRIu64 ",%" PRIx64 ",%" PRIu64 ",%" PRIu64, scene, n, pairs,
                bench::mix64(checksum), result.candidates, result.nodeVisits);
            for (auto& counts : QuadTree::detail::eligibleRangeTests)
                for (auto value : counts) std::printf(",%" PRIu64, value);
            for (auto value : QuadTree::detail::groupCandidates) std::printf(",%" PRIu64, value);
            std::puts("");
        }
}
