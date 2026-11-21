// Untimed: run the library benchmark adapter against the annotated header.
#include "bench-index.h"
#include <cinttypes>

namespace
{
struct Sink
{
    std::uint64_t pairs = 0, checksum = 0;
    void operator()(std::uint32_t a, std::uint32_t b) noexcept
    {
        ++pairs;
        checksum += bench::mix64((std::uint64_t(std::min(a, b)) << 32) | std::max(a, b));
    }
};
}

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const std::string_view scene = argv[1];
    constexpr std::size_t n = 100000;
    const auto boxes = bench::convert<std::int32_t>(bench::generate(scene, n, bench::sceneSeed(1, scene, n, 0)));
    bench::LibraryIndex<std::int32_t, true> index;
    index.build(boxes, bench::Boundary::Default);
    Sink sink;
    const auto result = index.pairs(sink, 200000000);
    const auto& s = QuadTree::detail::sortCensus;
    if (result.capped || s.calls[0] != 0 || s.calls[1] != 1 || s.elements[1] != n) return 1;
    constexpr const char* buckets[]{"unused", "source", "row_0_1", "row_2_8", "row_9_16", "row_17_64", "row_65_plus"};
    for (std::size_t i = 1; i < std::size(buckets); ++i)
        std::printf("%s,%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIx64 ",%" PRIu64 ",%" PRIu64 "\n",
            argv[1], buckets[i], s.calls[i], s.elements[i], s.comparisons[i], sink.pairs,
            bench::mix64(sink.checksum), result.candidates, result.nodeVisits);
}
