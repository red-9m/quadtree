// Untimed exact stream: pipeline lengths, epoch wrap and in-flight tree conversion.
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
using T = std::int32_t;

struct TinyEpoch : Checked
{
    static constexpr std::uint32_t rankEpochLimit = 4;
};
struct CountingResource final : Resource
{
    std::size_t requests = 0;
    void* allocate(std::size_t bytes, std::size_t alignment) noexcept override
    {
        ++requests;
        return defaultResource()->allocate(bytes, alignment);
    }
    void deallocate(void* ptr) noexcept override { defaultResource()->deallocate(ptr); }
};

void check(bool ok, std::source_location where = std::source_location::current())
{
    if (!ok)
    {
        std::fprintf(stderr, "lookahead probe failure at line %u\n", static_cast<unsigned>(where.line()));
        std::abort();
    }
}

template<typename... Fields>
void emit(Fields... fields)
{
    check(((std::fwrite(&fields, sizeof(fields), 1, stdout) == 1) && ...));
}

using I = Index<std::uint32_t, T, TinyEpoch, true>;

void dump(const I& index, Workspace<T>& scratch, const std::vector<I::HandleType>& handles,
    const std::vector<BuildEntry<std::uint32_t, T>>& entries)
{
    const auto& store = index.mEntries; // Diagnostic-only access.
    const auto stats = store.rankStats();
    emit(store.mRankEpoch, stats.tree, stats.probes, stats.cellResets, stats.conversions, store.mRankCount);
    if (stats.tree)
    {
        emit(store.mRankRoot, store.mRankHighWater);
        for (std::uint32_t i = 0; i < store.mRankHighWater; ++i)
        {
            const auto& node = store.mRanks[i].node;
            emit(node.rank, node.left, node.right);
        }
    }
    else
        for (std::size_t i = 0; i < store.mRankCapacity; ++i)
        {
            const bool occupied = store.occupied(i);
            emit(occupied);
            if (occupied)
                emit(store.mRanks[i].bucket.rank);
        }
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        check(index.valid(handles[i]) && index.payload(handles[i]) == entries[i].payload &&
            index.rank(handles[i]) == entries[i].rank);
        emit(detail::HandleAccess::slot(handles[i]));
    }
    const auto pairs = index.forEachPair([](const auto& a, const auto& b) noexcept
    {
        emit(a.payload, b.payload);
        return Continue;
    }, {.ordered = true, .workspace = &scratch});
    const auto& c = pairs.counters;
    check(pairs.complete());
    emit(c.nodeVisits, c.candidateTests, c.filterRejections, c.reports);
}

void probe(std::size_t n, bool adversarial)
{
    CountingResource resource;
    Workspace<T> scratch(&resource);
    check(scratch.reserve({n, n, n}) == Status::Ok);
    auto made = I::create({.extent = Extent::Adaptive, .splitThreshold = 4, .mergeThreshold = 2,
        .smallSetThreshold = 0}, &resource);
    check(made.ok());
    auto& index = made.value();
    std::vector<BuildEntry<std::uint32_t, T>> entries;
    entries.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i)
    {
        const T x = static_cast<T>((i * 13) % 151), y = static_cast<T>((i * 7) % 149);
        entries.push_back({{x, y, x + 12, y + 7}, i, {}, detail::mix64(i)});
    }
    const BuildOptions<T> options{.workspace = &scratch, .transactional = true};
    std::vector<I::HandleType> handles(n), rejected(n), sentinel(n);
    check(index.build(entries, handles, options) == Status::Ok);
    const auto requests = resource.requests;
    const auto& store = index.mEntries;
    const auto cold = store.rankStats();
    check(!cold.tree && index.stats().nodes > 1 && index.stats().nodes <= 1 + 4 * n);
    dump(index, scratch, handles, entries);
    if (adversarial)
    {
        // Same home even at maximum width. Fill a whole bounded probe chain,
        // then force conversion while the ranked bulk commit is in flight.
        std::uint64_t r = 0;
        const auto mask = std::uint64_t(store.mRankCapacity - 1);
        for (auto& entry: entries)
        {
            while ((detail::mix64(r) & mask) != (mask - 3))
                ++r;
            entry.rank = r++;
        }
    }
    for (int repeat = 0; repeat < 5; ++repeat)
    {
        const auto old = handles.front();
        const auto prior = index.rank(old);
        const auto last = entries.back().rank;
        entries.back().rank = entries.front().rank;
        check(index.build(entries, rejected, options) == Status::DuplicateRank);
        check(rejected == sentinel && index.valid(old) && index.rank(old) == prior);
        entries.back().rank = last;
        check(index.build(entries, handles, options) == Status::Ok);
        check(!index.valid(old) && resource.requests == requests);
        dump(index, scratch, handles, entries);
    }
    const auto warm = store.rankStats();
    if (adversarial)
        check(warm.tree && warm.conversions == cold.conversions + 1);
    else
        check(!warm.tree && warm.conversions == cold.conversions && warm.cellResets > cold.cellResets);
}
} // namespace

int main()
{
    for (const auto n: {63u, 64u, 65u, 95u, 96u, 97u, 257u})
        probe(n, false);
    probe(160, true);
    probe(257, true);
    check(std::fflush(stdout) == 0);
}
