// Untimed boundary, transaction and scratch-lifetime probe for ranked builds.
#include <quadtree-index.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <vector>

namespace
{
using namespace QuadTree;

void check(bool ok, std::source_location where = std::source_location::current())
{
    if (!ok)
    {
        std::fprintf(stderr, "probe failure at line %u\n", static_cast<unsigned>(where.line()));
        std::abort();
    }
}

struct CountingResource final: Resource
{
    std::size_t requests = 0;
    void* allocate(std::size_t bytes, std::size_t alignment) noexcept override
    {
        ++requests;
        return defaultResource()->allocate(bytes, alignment);
    }
    void deallocate(void* ptr) noexcept override { defaultResource()->deallocate(ptr); }
};

constexpr auto maxRank = std::numeric_limits<std::uint64_t>::max();

std::uint64_t rankFor(std::size_t i, unsigned pattern) noexcept
{
    if (pattern == 0)
        return i == 0 ? maxRank : i == 1 ? 0 : (maxRank / 2 - i) * UINT64_C(0x9e3779b97f4a7c15);
    if (pattern == 1)
        return (std::uint64_t(i) << 40) | 0xff;
    // All varying bits lie in a single byte at the radix threshold; above
    // 256 there is a second varying byte.
    return (std::uint64_t(i) << 48) | 0xf000000000000011ULL;
}

template<Coordinate T>
void checkBuilds(std::size_t ranked, unsigned pattern, bool sparse)
{
    using I = Index<int, T, Checked, true>;
    const auto n = ranked + (sparse ? 13u : 0u);
    CountingResource resource;
    Workspace<T> previous(&resource);
    check(previous.reserve({n, 0, n}) == Status::Ok);
    Workspace<T> scratch(std::move(previous));
    auto made = I::create({.extent = Extent::Fixed, .root = Box<T>{0, 0, 128, 128},
        .smallSetThreshold = 0}, &resource);
    check(made.ok());
    auto& index = made.value();
    std::vector<BuildEntry<int, T>> input;
    for (std::size_t i = 0; i < n; ++i)
    {
        const T x = static_cast<T>(i % 32);
        input.push_back({{x, 0, static_cast<T>(x + 32), 32}, static_cast<int>(i), {},
            i < ranked ? std::optional<std::uint64_t>{rankFor(i, pattern)} : std::nullopt});
    }
    const BuildOptions<T> options{.workspace = &scratch, .transactional = true};
    std::vector<typename I::HandleType> handles(n), sentinel(n), output(n);
    check(index.build(input, handles, options) == Status::Ok);
    const auto requests = resource.requests;
    const auto original = handles[0];
    const auto originalRank = index.rank(original);

    if (ranked >= 2)
    {
        const auto saved = input[ranked - 1].rank;
        input[ranked - 1].rank = input[0].rank;
        check(index.build(input, output, options) == Status::DuplicateRank);
        check(output == sentinel && index.valid(original) && index.rank(original) == originalRank && index.size() == n);
        const auto box = input.back().rect;
        input.back().rect = {0, 0, 129, 32};
        check(index.build(input, output, options) == Status::OutOfBounds);
        check(output == sentinel && index.valid(original) && index.size() == n);
        input.back().rect = box;
        input[ranked - 1].rank = saved;
    }
    if (!sparse && n >= 256)
    {
        const auto result = index.forEachPair([](const auto& a, const auto& b) noexcept
        {
            check(a.rank && b.rank && *a.rank < *b.rank);
            return Stop;
        }, {.ordered = true, .workspace = &scratch});
        check(result.status == Status::Stopped && result.counters.reports == 1);
    }
    check(index.build(input, output, options) == Status::Ok);
    check(resource.requests == requests && !index.valid(original));
    for (std::size_t i = 0; i < n; ++i)
        check(index.valid(output[i]) && index.payload(output[i]) == input[i].payload &&
            index.rank(output[i]) == input[i].rank);
}

struct MoveOnly
{
    int id;
    explicit MoveOnly(int value) noexcept: id(value) {}
    MoveOnly(MoveOnly&& other) noexcept: id(std::exchange(other.id, -1)) {}
};

void checkConsume()
{
    using I = Index<MoveOnly, std::int32_t, Checked, true>;
    constexpr std::size_t n = 512;
    std::vector<BuildEntry<MoveOnly, std::int32_t>> input;
    input.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        input.push_back({{0, 0, 4, 4}, MoveOnly(static_cast<int>(i)), {}, rankFor(i, 0)});
    auto made = I::create();
    check(made.ok());
    auto& index = made.value();
    Workspace<std::int32_t> scratch;
    check(scratch.reserve({0, 0, n}) == Status::Ok);
    const BuildOptions<std::int32_t> options{.workspace = &scratch, .transactional = true};
    input.back().rank = input.front().rank;
    check(index.buildConsume(input, options) == Status::DuplicateRank);
    for (std::size_t i = 0; i < n; ++i) check(input[i].payload.id == static_cast<int>(i));
    input.back().rank = rankFor(n - 1, 0);
    check(index.buildConsume(input, options) == Status::Ok);
    for (const auto& e : input) check(e.payload.id == -1);
}
} // namespace

int main()
{
    for (auto n : std::array<std::size_t, 9>{1, 2, 16, 128, 255, 256, 257, 513, 1024})
        for (auto pattern : {0u, 1u, 2u})
            for (auto sparse : {false, true})
            {
                checkBuilds<std::int32_t>(n, pattern, sparse);
                checkBuilds<float>(n, pattern, sparse);
            }
    checkConsume();
    std::puts("109 ranked-build cases passed (int32, float, 255/256/257, sparse and full-width ranks, rollback, scratch reuse, consume).");
}
