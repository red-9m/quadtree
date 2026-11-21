// Untimed exact bulk topology and mutation streams, including empty leaves.
#include <quadtree-index.h>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

namespace
{
using namespace QuadTree;
using T = std::int32_t;
using I = Index<unsigned, T, Checked, true>;

void check(bool ok, std::source_location where = std::source_location::current())
{
    if (!ok)
    {
        std::fprintf(stderr, "leaf-state probe failed at line %u\n", static_cast<unsigned>(where.line()));
        std::abort();
    }
}

template<typename... Fields>
void emit(Fields... fields)
{
    check(((std::fwrite(&fields, sizeof(fields), 1, stdout) == 1) && ...));
}

void box(Box<T> b) { emit(b.minX, b.minY, b.maxX, b.maxY); }

void dump(const I& index, Workspace<T>& scratch, bool fresh)
{
    std::size_t leaves = 0, empty = 0, retry = 0, parents = 0;
    const auto walk = [&](const auto& self, std::uint32_t id) -> void
    {
        const auto& n = index.mNodes[id];
        emit(id, n.count, n.ownCount, n.depth, n.parent, n.firstChild, n.head, n.linear,
            n.splitRetry, n.itemChanges, n.itemDirty, n.summary.category, n.summary.mask,
            n.itemSummary.category, n.itemSummary.mask);
        box(n.cell.box); box(n.itemBounds); box(n.bounds);
        if (n.firstChild != I::none)
        {
            ++parents;
            for (std::uint32_t q = 0; q != 4; ++q)
                self(self, n.firstChild + q);
        }
        else
        {
            ++leaves;
            empty += n.ownCount == 0;
            retry += n.splitRetry != 0;
            if (fresh)
                check(!n.itemDirty && n.itemChanges == 0 && n.count == n.ownCount &&
                    n.bounds.minX == n.itemBounds.minX && n.bounds.minY == n.itemBounds.minY &&
                    n.bounds.maxX == n.itemBounds.maxX && n.bounds.maxY == n.itemBounds.maxY &&
                    n.summary.category == n.itemSummary.category && n.summary.mask == n.itemSummary.mask);
        }
    };
    walk(walk, index.mRoot);
    check(leaves != 0 && empty != 0 && parents != 0 && (!fresh || retry != 0));
    emit(leaves, empty, retry, parents);
    for (std::size_t i = 0; i < index.size(); ++i)
    {
        const auto& ref = index.mReferences[i];
        emit(ref.node, ref.previous, ref.next);
    }
    for (bool ordered : {false, true})
        for (auto budget : {WorkBudget{}, WorkBudget{81, 121}})
        {
            const auto result = index.forEachPair([](const auto& a, const auto& b) noexcept
            {
                emit('v', a.payload, b.payload);
                return Continue;
            }, {.ordered = ordered, .workspace = &scratch, .budget = budget});
            const auto& c = result.counters;
            emit(result.status, c.nodeVisits, c.candidateTests, c.filterRejections, c.reports);
        }
}

void fixture(Boundary boundary)
{
    auto made = I::create({.extent = Extent::Fixed, .root = Box<T>{-512, -512, 512, 512},
        .boundary = boundary, .splitThreshold = 4, .mergeThreshold = 2, .smallSetThreshold = 0});
    check(made.ok());
    auto& index = made.value();
    std::vector<BuildEntry<unsigned, T>> entries;
    for (unsigned i = 0; i < 193; ++i)
    {
        const auto x = static_cast<T>(i * 59u % 884u) - 445;
        const auto y = static_cast<T>(i * 73u % 433u) + 12;
        const Box<T> b = i == 192 ? Box<T>{-490,-490,490,490} : i < 24 ?
            Box<T>{-400,-400,-396,-396} : Box<T>{x,y,x + 21,y + 31};
        entries.push_back({b, i, i % 5u == 0 ? Filter{2, 3} : Filter{1, 3}, detail::mix64(i)});
    }
    Workspace<T> scratch;
    check(scratch.reserve({entries.size(), 0, entries.size()}) == Status::Ok);
    std::vector<I::HandleType> handles(entries.size());
    const BuildOptions<T> options{.workspace = &scratch};
    check(index.build(entries, handles, options) == Status::Ok);
    dump(index, scratch, true);
    check(index.update(handles[1], Box<T>{300, -405, 370, -319}) == Status::Ok);
    check(index.remove(handles[25]) == Status::Ok);
    check(index.insert(Box<T>{-405, -405, -397, -391}, 10000u, Filter{1, 3}, 10000u).ok());
    dump(index, scratch, false);
    check(index.build(entries, handles, options) == Status::Ok);
    dump(index, scratch, true);
}
} // namespace

int main()
{
    fixture(Boundary::Default);
    fixture(Boundary::TouchInclusive);
    check(std::fflush(stdout) == 0);
}
