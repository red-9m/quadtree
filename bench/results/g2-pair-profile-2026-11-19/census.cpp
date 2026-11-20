// Diagnostic only: census.sh inserts the traversal counters in temporary headers.
#include "bench-index.h"

#include <cstdlib>

namespace
{
void check(bool ok)
{
    if (!ok)
        std::abort();
}

struct Sink
{
    std::uint64_t pairs = 0;
    std::uint64_t checksum = 0;

    void operator()(std::uint32_t a, std::uint32_t b) noexcept
    {
        ++pairs;
        checksum += bench::mix64((std::uint64_t(std::min(a, b)) << 32) | std::max(a, b));
    }
};
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    const std::string_view scene = argv[1];
    const auto n = std::strtoul(argv[2], nullptr, 10);
    const auto boxes = bench::convert<std::int32_t>(bench::generate(scene, n, bench::sceneSeed(1, scene, n, 0)));
    bench::LibraryIndex<std::int32_t> index;
    index.build(boxes, bench::Boundary::Default);
    const auto allocations = bench::memory::allocations();
    index.build(boxes, bench::Boundary::Default);
    Sink sink;
    const auto counters = index.pairs(sink, 200'000'000);
    using namespace QuadTree::detail;
    check(!counters.capped && bench::memory::allocations() == allocations);
    check(profileWalkCharged + profileSelfCharged + profileCrossCharged == counters.nodeVisits);
    check(profileCrossCharged == profileCrossBounds + profileCrossFilter + profileCrossPassed);
    check(profileCrossCalls == profileCrossEmpty + profileCrossCharged);
    std::printf("%s,%zu,%ju,%016jx,%ju,%ju,%ju,%ju,%ju,%ju,%ju,%ju,%ju,%ju,%ju\n", argv[1], std::size_t(n),
        std::uintmax_t(sink.pairs), std::uintmax_t(bench::mix64(sink.checksum)),
        std::uintmax_t(counters.candidates), std::uintmax_t(counters.nodeVisits),
        std::uintmax_t(profileSelfCharged), std::uintmax_t(profileWalkCharged),
        std::uintmax_t(profileCrossCalls), std::uintmax_t(profileCrossEmpty), std::uintmax_t(profileCrossCharged),
        std::uintmax_t(profileCrossBounds), std::uintmax_t(profileCrossFilter), std::uintmax_t(profileCrossPassed),
        std::uintmax_t(profileCrossChildCalls));
}
