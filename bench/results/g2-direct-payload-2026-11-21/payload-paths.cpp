// Untimed exact streams for direct payload and descriptor-based fallback builds.
#include <quadtree-index.h>
#include <cstdio>
#include <cstdlib>
#include <list>
#include <source_location>
#include <vector>

namespace
{
using namespace QuadTree;
using T = std::int32_t;
using P = std::uint32_t;
using Entry = BuildEntry<P, T>;
using I = Index<P, T, Checked, true>;

void check(bool ok, std::source_location where = std::source_location::current())
{
    if (!ok)
    {
        std::fprintf(stderr, "payload path probe failed at line %u\n", static_cast<unsigned>(where.line()));
        std::abort();
    }
}

template<typename... Fields>
void emit(Fields... fields)
{
    check(((std::fwrite(&fields, sizeof(fields), 1, stdout) == 1) && ...));
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

Box<T> rect(P input) noexcept
{
    const auto i = input - 7000u;
    const auto x = static_cast<T>(i * 17u % 93u), y = static_cast<T>(i * 29u % 91u);
    return {x, y, x + 12, y + 17};
}

void query(I& index, Workspace<T>& scratch, bool ordered)
{
    for (const auto budget : {WorkBudget{}, WorkBudget{108, 66}})
    {
        const auto result = index.forEachPair([](const auto& a, const auto& b) noexcept
        {
            emit('v', a.payload, b.payload);
            return Continue;
        }, {.ordered = ordered, .workspace = &scratch, .budget = budget});
        const auto& c = result.counters;
        emit('q', result.status, c.nodeVisits, c.candidateTests, c.filterRejections, c.reports);
    }
}

void run(unsigned mode, unsigned ranks)
{
    constexpr std::size_t n = 257;
    // Offset contiguous span to catch indexing relative to storage instead of its first entry.
    std::vector<Entry> items(n + 2);
    auto entries = std::span<Entry>(items).subspan(1, n);
    std::vector<P> payloads;
    for (std::size_t i = 0; i < n; ++i)
    {
        const P p = 7000u + static_cast<P>(i);
        auto& e = entries[i];
        e = {rect(p), p, {1, 1}, std::nullopt};
        if (ranks == 2 || (ranks == 1 && i % 3 != 0))
            e.rank = detail::mix64(i);
        payloads.push_back(p);
    }
    CountingResource resource;
    Workspace<T> scratch(&resource);
    check(scratch.reserve({n, 0, n}) == Status::Ok);
    auto made = I::create({.extent = Extent::Fixed, .root = Box<T>{-32, -32, 128, 128}, .splitThreshold = 4,
        .mergeThreshold = 2, .smallSetThreshold = 0}, &resource);
    check(made.ok());
    auto& index = made.value();
    const BuildOptions<T> options{.workspace = &scratch, .transactional = true};
    std::vector<I::HandleType> handles(n), rejected(n);
    const std::list<Entry> noncontiguous(entries.begin(), entries.end());
    std::size_t projections = 0;
    const auto build = [&]()
    {
        switch (mode)
        {
        case 0: return index.build(std::span<const Entry>(entries), handles, options);
        case 1: return index.build(noncontiguous, handles, options);
        case 2: return index.buildConsume(entries, handles, options);
        default: return index.buildWith(payloads, [&](P p) noexcept { ++projections; return rect(p); }, options);
        }
    };
    check(build() == Status::Ok && index.stats().nodes > 1);
    const auto requests = resource.requests;
    for (unsigned repeat = 0; repeat < 2; ++repeat)
    {
        check(build() == Status::Ok && resource.requests == requests);
        if (mode != 3)
            for (std::size_t i = 0; i < n; ++i)
            {
                check(index.valid(handles[i]) && index.payload(handles[i]) == entries[i].payload &&
                    index.rank(handles[i]) == entries[i].rank);
                emit(detail::HandleAccess::slot(handles[i]));
            }
        check(index.forEachEntry([](const auto& e) noexcept { emit('e', e.payload); return Continue; }).complete());
        query(index, scratch, false);
        if (ranks == 2)
            query(index, scratch, true);
        check(resource.requests == requests);
    }
    check(projections == (mode == 3 ? 3 * n : 0));
    if (mode == 0)
    {
        const auto original = entries.back();
        entries.back().rect = {129, 130, 132, 133};
        auto status = index.build(std::span<const Entry>(entries), rejected, options);
        check(status == Status::OutOfBounds);
        emit(status);
        entries.back() = original;
        if (ranks == 2)
        {
            entries.back().rank = entries.front().rank;
            status = index.build(std::span<const Entry>(entries), rejected, options);
            check(status == Status::DuplicateRank);
            emit(status);
            entries.back() = original;
        }
        check(resource.requests == requests);
        for (std::size_t i = 0; i < n; ++i)
            check(rejected[i] == I::HandleType{} && index.valid(handles[i]) &&
                index.payload(handles[i]) == entries[i].payload && index.rank(handles[i]) == entries[i].rank);
        query(index, scratch, false);
    }
}
} // namespace

int main()
{
    for (unsigned mode = 0; mode != 3; ++mode)
        for (unsigned ranks = 0; ranks != 3; ++ranks)
            run(mode, ranks);
    run(3, 0);
    check(std::fflush(stdout) == 0);
}
