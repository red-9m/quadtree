// Untimed exact comparison on signed limits, one-axis splits and coincident boxes.
#include <quadtree-index.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <source_location>
#include <utility>
#include <vector>

namespace
{
using namespace QuadTree;
#ifndef PROBE_T
#define PROBE_T std::int32_t
#endif
using T = PROBE_T;
using I = Index<int, T, Checked, true>;
using B = Box<T>;
constexpr auto lo = std::numeric_limits<T>::lowest(), hi = std::numeric_limits<T>::max();

void check(bool ok, std::source_location where = std::source_location::current())
{
    if (!ok)
    {
        std::fprintf(stderr, "probe failure at line %u\n", static_cast<unsigned>(where.line()));
        std::abort();
    }
}

template<typename... Fields>
void emit(Fields... fields)
{
    check(((std::fwrite(&fields, sizeof(fields), 1, stdout) == 1) && ...));
}

void emitBox(const B& b) { emit(b.minX, b.minY, b.maxX, b.maxY); }

void probe(const std::vector<B>& boxes, std::uint32_t depth, Boundary boundary, bool ranked, bool fixed = false)
{
    const auto n = boxes.size();
    const Config<T> config{.extent = fixed ? Extent::Fixed : Extent::Adaptive,
        .root = fixed ? std::optional<B>{B{lo, lo, hi, hi}} : std::nullopt,
        .boundary = boundary, .splitThreshold = 2, .mergeThreshold = 1, .smallSetThreshold = 0, .maxDepth = depth};
    auto made = I::create(config);
    check(made.ok());
    auto& index = made.value();
    Workspace<T> scratch;
    check(scratch.reserve({n, n, n}) == Status::Ok);
    std::vector<BuildEntry<int, T>> input;
    for (std::size_t i = 0; i < n; ++i)
    {
        const auto rank = std::uint64_t(i) * UINT64_C(0xbf58476d1ce4e5b9);
        input.push_back({boxes[i], static_cast<int>(i), i % 5 == 0 ? Filter{2, 3} : Filter{1, 3},
            ranked ? std::optional<std::uint64_t>{rank} : std::nullopt});
    }
    std::vector<I::HandleType> handles(n);
    check(index.build(input, handles, {.workspace = &scratch}) == Status::Ok);
    if (fixed)
        check(index.stats().nodes > 1 + 4 * n); // More plans than scratch slots forces the sorted fallback.
    for (std::size_t i = 0; i < n; ++i)
    {
        check(index.valid(handles[i]) && index.payload(handles[i]) == input[i].payload && index.rect(handles[i]) == boxes[i]);
        emit(detail::HandleAccess::slot(handles[i]));
    }
    check(index.forEachEntry([](const auto& view) noexcept { emit(view.payload); return Continue; }).complete());
    check(index.forEachNode([](const auto& node) noexcept
    {
        emitBox(node.cell);
        if (node.entries != 0) emitBox(node.bounds);
        emit(node.depth, node.entries, node.ownEntries, node.summary.category, node.summary.mask);
        return Continue;
    }).complete());
    for (bool ordered : {false, true})
    {
        if (ordered && !ranked) continue;
        for (const auto limit : std::array<std::uint64_t, 5>{0, 1, 11, 71, 509})
        {
            const auto result = index.forEachPair([](const auto& a, const auto& b) noexcept
            {
                emit(a.payload, b.payload);
                return Continue;
            }, {.ordered = ordered, .workspace = &scratch, .budget = {.nodeVisits = limit}});
            const auto& c = result.counters;
            emit(result.status, c.candidateTests, c.nodeVisits, c.filterRejections, c.reports);
        }
    }
}
} // namespace

int main()
{
    std::vector<B> extremes{{lo, lo, hi, hi}, {lo, -1, hi, 0}, {lo, lo, hi - 1, lo + 1},
        {lo + 1, lo, hi, hi - 1}, {hi, hi, hi, hi}, {lo, lo, lo, lo}, {-1, -1, 0, 0}};
    std::vector<B> oneAxis;
    for (T i = 0; i < 260; ++i)
    {
        extremes.push_back({lo + i, -2, lo + 3 * i, 3});
        extremes.push_back({hi - 3 * i, -5, hi - i, 2});
        const auto x = (i * 61) % 260 - 130;
        oneAxis.push_back({x, 0, x + i % 9, 0});
    }
    const std::vector<B> coincident(261, B{-3, -5, -3, -5});
    for (const auto* boxes : {&std::as_const(extremes), &std::as_const(oneAxis), &coincident})
        for (auto mode : {Boundary::Default, Boundary::TouchInclusive})
            for (auto depth : std::array<std::uint32_t, 3>{0, 4, 64})
                for (bool ranked : {false, true})
                    probe(*boxes, depth, mode, ranked);
    probe({{lo, lo, lo, lo}, {lo + 2, lo + 2, lo + 2, lo + 2}, {lo + 4, lo + 4, lo + 4, lo + 4}},
        64, Boundary::TouchInclusive, true, true);
    check(std::fflush(stdout) == 0);
}
