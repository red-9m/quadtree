// Count-only diagnostic; census.sh annotates a scratch copy of the header.
#include "bench-index.h"
#include <cinttypes>
#include <limits>

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

template<bool Ordered>
void count(std::string_view scene)
{
    constexpr std::size_t n = 100000;
    const auto boxes = bench::convert<std::int32_t>(bench::generate(scene, n, bench::sceneSeed(1, scene, n, 0)));
    bench::LibraryIndex<std::int32_t, Ordered> index;
    index.build(boxes, bench::Boundary::Default);
    Sink sink;
    const auto r = index.pairs(sink, 200000000);
    if (r.capped) std::abort();
    const auto& c = QuadTree::detail::snapshotCensus;
    std::printf("%.*s,%s,%" PRIu64 ",%" PRIx64 ",%" PRIu64 ",%" PRIu64,
        static_cast<int>(scene.size()), scene.data(), Ordered ? "index-ordered" : "index",
        sink.pairs, bench::mix64(sink.checksum), r.candidates, r.nodeVisits);
    for (const auto value : {c.walkEmpty, c.walkDisjoint, c.walkFilter, c.walkAdmitted,
             c.crossEmpty, c.crossDisjoint, c.crossFilter, c.crossAdmitted, c.rankChecks, c.rankRejects})
        std::printf(",%" PRIu64, value);
    for (const auto value : c.rows) std::printf(",%" PRIu64, value);
    for (const auto value : c.partners) std::printf(",%" PRIu64, value);
    std::printf(",%zu\n", c.maxRow);
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    if (std::string_view(argv[1]) == "index-ordered") count<true>(argv[2]);
    else if (std::string_view(argv[1]) == "index") count<false>(argv[2]);
    else return 2;
}
