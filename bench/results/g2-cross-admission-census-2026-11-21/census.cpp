// Untimed shadow traversal against the same live library index and exact stream.
#include "bench-index.h"
#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

namespace
{
using Adapter = bench::LibraryIndex<std::int32_t, false>;
using I = QuadTree::Index<std::uint32_t, std::int32_t>;
using Pair = std::pair<std::uint32_t, std::uint32_t>;

void check(bool ok, std::source_location where = std::source_location::current())
{
    if (!ok)
    {
        std::fprintf(stderr, "census mismatch at line %u\n", static_cast<unsigned>(where.line()));
        std::abort();
    }
}

struct Bin
{
    std::uint64_t calls = 0, empty = 0, xOnly = 0, yOnly = 0, both = 0, filtered = 0, admitted = 0;
    std::uint64_t candidates = 0, reports = 0;
};

#if defined(QT_ITEM_CENSUS)
struct ItemBin
{
    std::uint64_t calls = 0, rejected = 0, xOnly = 0, yOnly = 0, both = 0, contact = 0, strict = 0, reports = 0;
};
#endif

struct Census
{
    const I& index;
    const std::vector<Pair>& expected;
    I::Context context{{}};
    QuadTree::detail::AcceptAll predicate;
    std::array<std::array<Bin, 3>, 2> bins{}; // sibling/recursive × leaf-leaf/mixed/internal-internal
    std::size_t cursor = 0;
    std::array<std::uint64_t, 3> candidateOrigins{};
    std::array<std::uint64_t, 3> reportOrigins{};
    std::uint64_t selfVisits = 0, holderVisits = 0;
#if defined(QT_ITEM_CENSUS)
    std::array<std::array<ItemBin, 2>, 3> items{};
#endif

    void pair(std::uint32_t a, std::uint32_t b, std::size_t origin)
    {
        const auto before = context.result.counters.candidateTests;
        const auto old = cursor;
#if defined(QT_ITEM_CENSUS)
        const auto& x = index.mEntries.mBoxes[a];
        const auto& y = index.mEntries.mBoxes[b];
        const auto degenerate = x.minX == x.maxX || x.minY == x.maxY || y.minX == y.maxX || y.minY == y.maxY;
        auto& c = items[origin][degenerate];
        ++c.calls;
        const bool compatible = QuadTree::detail::compatible(index.mEntries.mMeta[a].filter, index.mEntries.mMeta[b].filter);
        const auto loX = std::max(x.minX, y.minX), hiX = std::min(x.maxX, y.maxX);
        const auto loY = std::max(x.minY, y.minY), hiY = std::min(x.maxY, y.maxY);
        const bool dx = loX > hiX, dy = loY > hiY;
        const bool contact = !dx && !dy && (loX == hiX || loY == hiY);
        if (!compatible)
            ++c.rejected;
        else if (dx || dy)
            ++(dx && dy ? c.both : dx ? c.xOnly : c.yOnly);
        else if (contact)
            ++c.contact;
        else
            ++c.strict;
#endif
        auto visitor = [&](const auto& first, const auto& second) noexcept
        {
            check(cursor < expected.size() && expected[cursor++] == Pair{first.payload, second.payload});
            return QuadTree::Continue;
        };
        index.testPair(a, b, false, context, visitor, predicate);
        candidateOrigins[origin] += context.result.counters.candidateTests - before;
        reportOrigins[origin] += cursor - old;
#if defined(QT_ITEM_CENSUS)
        c.reports += cursor - old;
        // For a degenerate operand, endpoint inequalities can be strict even
        // when the intersection itself has zero extent. Default still accepts
        // every closed hit in that stratum, so the extent-based census agrees.
        check((cursor != old) == (compatible && !dx && !dy && (!contact || degenerate)));
#endif
    }

    void holder(std::uint32_t a, std::uint32_t b)
    {
        const auto& x = index.mNodes[a];
        const auto& y = index.mNodes[b];
        if (!x.ownCount || !y.count || !QuadTree::detail::closedOverlap(x.itemBounds, y.bounds))
            return;
        if (!QuadTree::detail::compatible(x.itemSummary, y.summary))
        {
            context.filtered();
            return;
        }
        const auto before = context.result.counters.nodeVisits;
        for (auto i = x.head; i != I::none; i = index.nextItem(x, i))
        {
            const auto test = [&](std::uint32_t j) noexcept { pair(i, j, 2); };
            index.walk(b, index.mEntries.mBoxes[i], index.mEntries.mMeta[i].filter, context, test);
        }
        holderVisits += context.result.counters.nodeVisits - before;
    }

    void cross(std::uint32_t a, std::uint32_t b, bool recursive)
    {
        const auto& x = index.mNodes[a];
        const auto& y = index.mNodes[b];
        const auto shape = std::size_t(x.firstChild != I::none) + std::size_t(y.firstChild != I::none);
        auto& stat = bins[recursive][shape];
        ++stat.calls;
        if (!x.count || !y.count)
        {
            ++stat.empty;
            return;
        }
        check(context.node());
        const bool dx = x.bounds.maxX < y.bounds.minX || y.bounds.maxX < x.bounds.minX;
        const bool dy = x.bounds.maxY < y.bounds.minY || y.bounds.maxY < x.bounds.minY;
        check((!dx && !dy) == QuadTree::detail::closedOverlap(x.bounds, y.bounds));
        if (dx || dy)
        {
            ++(dx && dy ? stat.both : dx ? stat.xOnly : stat.yOnly);
            return;
        }
        if (!QuadTree::detail::compatible(x.summary, y.summary))
        {
            ++stat.filtered;
            context.filtered();
            return;
        }
        ++stat.admitted;
        const auto tests = candidateOrigins[1], reports = reportOrigins[1];
        for (auto i = x.head; i != I::none; i = index.nextItem(x, i))
            for (auto j = y.head; j != I::none; j = index.nextItem(y, j))
                pair(i, j, 1);
        stat.candidates += candidateOrigins[1] - tests;
        stat.reports += reportOrigins[1] - reports;
        if (y.firstChild != I::none)
            for (std::uint32_t q = 0; q < 4; ++q)
                holder(a, y.firstChild + q);
        if (x.firstChild != I::none)
            for (std::uint32_t q = 0; q < 4; ++q)
                holder(b, x.firstChild + q);
        if (x.firstChild != I::none && y.firstChild != I::none)
            for (std::uint32_t q = 0; q < 4; ++q)
                for (std::uint32_t r = 0; r < 4; ++r)
                    cross(x.firstChild + q, y.firstChild + r, true);
    }

    void self(std::uint32_t a)
    {
        const auto& x = index.mNodes[a];
        if (!x.count)
            return;
        check(context.node());
        ++selfVisits;
        if (!QuadTree::detail::compatible(x.summary, x.summary))
        {
            context.filtered();
            return;
        }
        for (auto i = x.head; i != I::none; i = index.nextItem(x, i))
            for (auto j = index.nextItem(x, i); j != I::none; j = index.nextItem(x, j))
                pair(i, j, 0);
        if (x.firstChild == I::none)
            return;
        for (std::uint32_t q = 0; q < 4; ++q)
            holder(a, x.firstChild + q);
        for (std::uint32_t q = 0; q < 4; ++q)
            self(x.firstChild + q);
        for (std::uint32_t q = 0; q < 4; ++q)
            for (std::uint32_t r = q + 1; r < 4; ++r)
                cross(x.firstChild + q, x.firstChild + r, false);
    }
};

void fixture(const char* scene, std::size_t n)
{
    const auto generated = bench::generate(scene, n, bench::sceneSeed(1, scene, n, 0));
    const auto boxes = bench::convert<std::int32_t>(generated);
    Adapter adapter;
    adapter.build(boxes, bench::Boundary::Default);
    const I& index = *adapter.mTree;
    std::vector<Pair> stream;
    std::uint64_t checksum = 0;
    const auto actual = index.forEachPair([&](const auto& x, const auto& y) noexcept
    {
        stream.emplace_back(x.payload, y.payload);
        const auto lo = std::min(x.payload, y.payload), hi = std::max(x.payload, y.payload);
        checksum += bench::mix64((std::uint64_t(lo) << 32) | hi);
        return QuadTree::Continue;
    });
    Census shadow{index, stream};
    shadow.self(index.mRoot);
    const auto a = actual.counters, b = shadow.context.result.counters;
    check(actual.complete() && shadow.context.result.complete() && shadow.cursor == stream.size() &&
        a.nodeVisits == b.nodeVisits && a.candidateTests == b.candidateTests && a.reports == b.reports &&
        a.filterRejections == b.filterRejections && a.predicateCalls == b.predicateCalls);
    const auto hash = bench::mix64(n ^ bench::sceneHash(generated));
    std::printf("%s,%zu,%" PRIx64 ",%" PRIx64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64,
        scene, n, hash, bench::mix64(checksum), a.nodeVisits, a.candidateTests, a.reports, shadow.selfVisits, shadow.holderVisits);
    for (auto c : shadow.candidateOrigins) std::printf(",%" PRIu64, c);
    for (auto c : shadow.reportOrigins) std::printf(",%" PRIu64, c);
    std::puts("");
#if defined(QT_ITEM_CENSUS)
    for (std::size_t origin = 0; origin < shadow.items.size(); ++origin)
        for (std::size_t shape = 0; shape < shadow.items[origin].size(); ++shape)
        {
            const auto& c = shadow.items[origin][shape];
            std::fprintf(stderr, "%s,%zu,%zu,%zu", scene, n, origin, shape);
            for (auto v : {c.calls, c.rejected, c.xOnly, c.yOnly, c.both, c.contact, c.strict, c.reports})
                std::fprintf(stderr, ",%" PRIu64, v);
            std::fputc('\n', stderr);
        }
#else
    for (std::size_t origin = 0; origin < shadow.bins.size(); ++origin)
        for (std::size_t shape = 0; shape < shadow.bins[origin].size(); ++shape)
        {
            const auto& s = shadow.bins[origin][shape];
            std::fprintf(stderr, "%s,%zu,%zu,%zu", scene, n, origin, shape);
            for (auto v : {s.calls, s.empty, s.xOnly, s.yOnly, s.both, s.filtered, s.admitted, s.candidates, s.reports})
                std::fprintf(stderr, ",%" PRIu64, v);
            std::fputc('\n', stderr);
        }
#endif
}
} // namespace

int main()
{
    std::puts("scene,n,scene_hash,checksum,node_visits,candidates,pairs,self_visits,holder_visits,self_tests,cross_tests,holder_tests,self_reports,cross_reports,holder_reports");
#if defined(QT_ITEM_CENSUS)
    std::fputs("scene,n,origin,degenerate,calls,filter_reject,x_only,y_only,both,contact,strict,reports\n", stderr);
#else
    std::fputs("scene,n,origin,shape,calls,empty,x_only,y_only,both,filtered,admitted,candidates,reports\n", stderr);
#endif
    fixture("uniform", 100000);
    fixture("split-line", 10000);
    fixture("split-line-translated", 100000);
    fixture("mixed-size", 100000);
}
