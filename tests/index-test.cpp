// Independent edge-comparison oracle, mutation/build, numeric and failure tests.
#include "index-mixed-macros.h"

#include <array>
#include <bitset>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <forward_list>
#include <iterator>
#include <memory>
#include <numeric>
#include <set>
#include <span>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

using QuadTree::Box;
using QuadTree::Rect;
using QuadTree::Status;
using QuadTree::Boundary;
using QuadTree::Filter;
using QuadTree::Continue;
using QuadTree::Stop;

using MultiTuIndex = QuadTree::Index<int, int, QuadTree::Checked>;
MultiTuIndex::HandleType indexTu2Insert(MultiTuIndex& tree);
bool indexTu2Valid(const MultiTuIndex& tree, MultiTuIndex::HandleType handle);

namespace
{
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); ++failures; } } while (false)
#define REQUIRE(condition) do { if (!(condition)) { CHECK(condition); std::exit(2); } } while (false)

class TestResource final: public QuadTree::Resource
{
public:
    std::size_t allocations = 0, failAt = 0, bytes = 0, peak = 0, failedBytes = 0;
    bool failAll = false;
    std::unordered_map<void*, std::size_t> owned;
    std::vector<std::size_t>* requests = nullptr;

    void* allocate(std::size_t count, std::size_t alignment) noexcept override
    {
        CHECK(count != 0);
        ++allocations;
        if (requests) requests->push_back(count);
        if (failAll || allocations == failAt)
        {
            failedBytes = count;
            return nullptr;
        }
        void* ptr = QuadTree::defaultResource()->allocate(count, alignment);
        if (ptr)
        {
            CHECK(reinterpret_cast<std::uintptr_t>(ptr) % alignment == 0);
            owned.emplace(ptr, count);
            bytes += count;
            peak = std::max(peak, bytes);
        }
        return ptr;
    }
    void deallocate(void* ptr) noexcept override
    {
        if (!ptr) return;
        const auto it = owned.find(ptr);
        REQUIRE(it != owned.end());
        bytes -= it->second;
        owned.erase(it);
        QuadTree::defaultResource()->deallocate(ptr);
    }
    ~TestResource() override { CHECK(owned.empty()); CHECK(bytes == 0); }
};

struct Rng
{
    std::uint64_t state = 1;
    std::uint64_t next() noexcept
    {
        std::uint64_t z = (state += 0x9e3779b97f4a7c15u);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9u;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebu;
        return z ^ (z >> 31);
    }
    std::size_t below(std::size_t n) noexcept { return static_cast<std::size_t>(next() % n); }
    int range(int low, int high) noexcept { return low + static_cast<int>(below(static_cast<std::size_t>(high - low + 1))); }
};

// Does not use any library overlap, conversion, filter, or traversal helper.
template<typename T>
bool overlap(const Box<T>& a, const Box<T>& b, Boundary mode) noexcept
{
    const bool closed = mode == Boundary::TouchInclusive || a.minX == a.maxX || a.minY == a.maxY ||
        b.minX == b.maxX || b.minY == b.maxY;
    if (closed)
        return !(a.maxX < b.minX || b.maxX < a.minX || a.maxY < b.minY || b.maxY < a.minY);
    return !(a.maxX <= b.minX || b.maxX <= a.minX || a.maxY <= b.minY || b.maxY <= a.minY);
}

bool accepts(Filter a, Filter b) noexcept
{
    return (a.mask & b.category) > 0 && (b.mask & a.category) > 0;
}

template<typename T>
using Tree = QuadTree::Index<int, T, QuadTree::Checked>;

template<typename T>
struct Record
{
    Box<T> box;
    int id;
    Filter filter;
    std::uint64_t rank;
    typename Tree<T>::HandleType handle;
};

using Pair = std::pair<int, int>;
using RankPair = std::pair<std::uint64_t, std::uint64_t>;

void checkZeroCounters(const QuadTree::Counters& c)
{
    CHECK(c.nodeVisits == 0 && c.candidateTests == 0 && c.filterRejections == 0 && c.predicateCalls == 0 && c.reports == 0);
}

template<typename T>
std::vector<Pair> expectedPairs(const std::vector<Record<T>>& records, Boundary boundary,
    const std::set<int>* moved = nullptr, bool predicate = false)
{
    std::vector<Pair> result;
    for (std::size_t i = 0; i < records.size(); ++i)
        for (std::size_t j = i + 1; j < records.size(); ++j)
        {
            const auto& a = records[i];
            const auto& b = records[j];
            if (overlap(a.box, b.box, boundary) && accepts(a.filter, b.filter) &&
                (!moved || moved->contains(a.id) || moved->contains(b.id)) &&
                (!predicate || (a.id + b.id) % 3 != 0))
                result.emplace_back(std::min(a.id, b.id), std::max(a.id, b.id));
        }
    std::sort(result.begin(), result.end());
    return result;
}

template<typename T>
void verify(const Tree<T>& tree, const std::vector<Record<T>>& records, QuadTree::Workspace<T>& workspace,
    bool detailed = true)
{
    CHECK(tree.size() == records.size());
    CHECK(tree.empty() == records.empty());
    std::vector<int> entries;
    const auto iterated = tree.forEachEntry([&](const auto& entry) noexcept
    {
        entries.push_back(entry.payload);
        const auto found = std::find_if(records.begin(), records.end(), [&](const auto& r) { return r.id == entry.payload; });
        REQUIRE(found != records.end());
        CHECK(entry.handle == found->handle);
        CHECK(entry.rect == found->box);
        CHECK(entry.filter == found->filter);
        CHECK(entry.rank == found->rank);
        return Continue;
    });
    CHECK(iterated.complete());
    CHECK(entries.size() == records.size());
    std::size_t nodes = 0;
    CHECK(tree.forEachNode([&](const auto& node) noexcept
    {
        ++nodes;
        CHECK(node.ownEntries <= node.entries);
        CHECK(node.depth <= tree.config().maxDepth);
        return Continue;
    }).complete());
    CHECK(nodes == tree.stats().nodes);
    const Boundary boundary = tree.config().boundary;
    const auto expected = expectedPairs(records, boundary);
    std::vector<Pair> actual;
    auto visitor = [&](const auto& a, const auto& b) noexcept
    {
        CHECK(QuadTree::detail::HandleAccess::slot(a.handle) < QuadTree::detail::HandleAccess::slot(b.handle));
        actual.emplace_back(std::min(a.payload, b.payload), std::max(a.payload, b.payload));
        return Continue;
    };
    const auto result = tree.forEachPair(visitor, {.workspace = &workspace});
    CHECK(result.complete());
    CHECK(result.counters.reports == actual.size());
    std::sort(actual.begin(), actual.end());
    CHECK(actual == expected); // A multiset comparison also detects duplicate reports.
    for (const auto& record : records)
    {
        CHECK(tree.valid(record.handle));
        CHECK(tree.rect(record.handle) == record.box);
        CHECK(tree.filter(record.handle) == record.filter);
        CHECK(tree.rank(record.handle) == record.rank);
    }
    if (!detailed) return;
    actual.clear();
    std::vector<RankPair> sequence;
    const auto ordered = tree.forEachPair([&](const auto& a, const auto& b) noexcept
    {
        CHECK(*a.rank < *b.rank);
        sequence.emplace_back(*a.rank, *b.rank);
        actual.emplace_back(std::min(a.payload, b.payload), std::max(a.payload, b.payload));
        return Continue;
    }, {.ordered = true, .workspace = &workspace});
    CHECK(ordered.complete());
    CHECK(std::is_sorted(sequence.begin(), sequence.end()));
    std::sort(actual.begin(), actual.end());
    CHECK(actual == expected);

    std::vector<typename Tree<T>::HandleType> moved;
    std::set<int> moved_ids;
    for (std::size_t i = 0; i < records.size(); i += 3)
    {
        moved.push_back(records[i].handle);
        moved_ids.insert(records[i].id);
    }
    for (bool in_order : {false, true})
    {
        actual.clear(); sequence.clear();
        const auto changed = tree.forEachPairInvolving(moved, [&](const auto& a, const auto& b) noexcept
        {
            actual.emplace_back(std::min(a.payload, b.payload), std::max(a.payload, b.payload));
            if (in_order) sequence.emplace_back(*a.rank, *b.rank);
            return Continue;
        }, {.ordered = in_order, .workspace = &workspace}, [](const auto& a, const auto& b) noexcept
            { return (a.payload + b.payload) % 3 != 0; });
        CHECK(changed.complete());
        CHECK(std::is_sorted(sequence.begin(), sequence.end()));
        std::sort(actual.begin(), actual.end());
        CHECK(actual == expectedPairs(records, boundary, &moved_ids, true));
    }
    const Box<T> query{T(-50), T(-30), T(40), T(60)};
    for (bool filtered : {false, true})
    {
        std::vector<int> hits, expected_hits;
        const Filter filter{3, 6};
        for (const auto& entry : records)
            if (overlap(query, entry.box, boundary) && (!filtered || accepts(filter, entry.filter)))
                expected_hits.push_back(entry.id);
        auto hit = [&](const auto& entry) noexcept { hits.push_back(entry.payload); return Continue; };
        const auto queried = filtered ? tree.query(query, filter, hit) : tree.query(query, hit);
        CHECK(queried.complete());
        std::sort(hits.begin(), hits.end());
        std::sort(expected_hits.begin(), expected_hits.end());
        CHECK(hits == expected_hits);
    }
    if (!records.empty())
    {
        const auto& a = records.back();
        std::vector<int> hits, expected_hits;
        for (const auto& b : records)
            if (a.id != b.id && accepts(a.filter, b.filter) && overlap(a.box, b.box, boundary) && b.rank > 17)
                expected_hits.push_back(b.id);
        CHECK(tree.queryPartners(a.handle, [&](const auto& b) noexcept
            { hits.push_back(b.payload); return Continue; }, {.workspace = &workspace, .greaterRank = 17}).complete());
        std::sort(hits.begin(), hits.end());
        std::sort(expected_hits.begin(), expected_hits.end());
        CHECK(hits == expected_hits);
    }
}

template<typename T>
Box<T> randomBox(Rng& rng)
{
    const int x = rng.range(-120, 120), y = rng.range(-120, 120);
    return {T(x), T(y), T(x + rng.range(0, 28)), T(y + rng.range(0, 28))};
}

template<typename T>
void randomized(Boundary boundary, QuadTree::Extent extent)
{
    QuadTree::Config<T> config;
    config.boundary = boundary;
    config.extent = extent;
    config.smallSetThreshold = 4;
    config.splitThreshold = 3;
    config.mergeThreshold = 1;
    if (extent == QuadTree::Extent::Fixed) config.root = Box<T>{T(-256), T(-256), T(256), T(256)};
    if (extent == QuadTree::Extent::Snapshot) config.snapshotGrowth = QuadTree::SnapshotGrowth::Adaptive;
    auto created = Tree<T>::create(config);
    REQUIRE(created.ok());
    auto tree = std::move(created).value();
    QuadTree::Workspace<T> workspace;
    REQUIRE(workspace.reserve(256) == Status::Ok);
    std::vector<Record<T>> records;
    std::vector<typename Tree<T>::HandleType> dead;
    Rng rng{0x625f42};
    int next_id = 0;
    for (unsigned step = 0; step < 650; ++step)
    {
        const std::size_t op = rng.below(10);
        if (records.empty() || (op < 4 && records.size() < 160))
        {
            const Box<T> box = randomBox<T>(rng);
            const Filter filter{rng.next() & 7, rng.next() & 7};
            const auto rank = std::uint64_t(next_id) * 37;
            const auto inserted = tree.insert(box, next_id, filter, rank);
            REQUIRE(inserted.ok());
            records.push_back({box, next_id++, filter, rank, inserted.handle});
        }
        else if (op < 6)
        {
            auto& record = records[rng.below(records.size())];
            const Box<T> box = randomBox<T>(rng);
            CHECK(tree.update(record.handle, box) == Status::Ok);
            record.box = box;
        }
        else if (op == 6)
        {
            auto& record = records[rng.below(records.size())];
            record.filter = Filter{rng.next() & 7, rng.next() & 7};
            CHECK(tree.setFilter(record.handle, record.filter) == Status::Ok);
        }
        else if (op == 7)
        {
            const std::size_t index = rng.below(records.size());
            dead.push_back(records[index].handle);
            CHECK(tree.remove(records[index].handle) == Status::Ok);
            records[index] = records.back();
            records.pop_back();
        }
        else if (op == 8 && step % 5 == 0)
        {
            std::vector<QuadTree::BuildEntry<int, T>> input;
            for (const auto& record : records)
                input.push_back({record.box, record.id, record.filter, record.rank});
            std::vector<typename Tree<T>::HandleType> output(input.size());
            CHECK(tree.build(input, output, {.workspace = &workspace, .transactional = step % 2 == 0}) == Status::Ok);
            for (std::size_t i = 0; i < records.size(); ++i)
            {
                CHECK(!tree.valid(records[i].handle));
                dead.push_back(records[i].handle);
                records[i].handle = output[i];
            }
        }
        else if (step % 17 == 0)
            CHECK(tree.shrink() == Status::Ok);
        verify(tree, records, workspace, step % 19 == 0);
        for (auto handle : dead) CHECK(!tree.valid(handle));
    }
    verify(tree, records, workspace);
    tree.clear();
    CHECK(tree.empty());
    CHECK(tree.stats().nodes == 0);
    for (const auto& record : records) CHECK(!tree.valid(record.handle));
}

// Compare the full-pair fast path to the ordinary predicate traversal at
// candidate-budget and visitor-stop boundaries, including filtered mutations.
void int64PairPrefixes()
{
    using T = std::int64_t;
    auto made = Tree<T>::create({.splitThreshold = 3, .mergeThreshold = 1, .smallSetThreshold = 0});
    REQUIRE(made.ok());
    auto& tree = made.value();
    std::vector<QuadTree::BuildEntry<int, T>> entries;
    for (int i = 0; i < 30; ++i)
    {
        const T x = ((i % 6) - 2) * 9, y = (i / 6) * 9;
        entries.push_back({{x, y, x + 13, y + 13}, int(i), {std::uint64_t(4 | (i & 1)), 6}, std::uint64_t(i)});
    }
    std::vector<Tree<T>::HandleType> handles(entries.size());
    QuadTree::Workspace<T> scratch;
    REQUIRE(scratch.reserve(entries.size() + 1) == Status::Ok);
    const auto check = [&](bool limitNodes = false)
    {
        for (bool ordered : {false, true})
            for (std::uint64_t limit = 0; limit <= 256; ++limit)
                for (unsigned stop : {0u, 1u, 8u})
                {
                    std::vector<Pair> expected, actual;
                    const auto visit = [&](auto& dst) { return [&](const auto& a, const auto& b) noexcept
                    { dst.emplace_back(a.payload, b.payload); return dst.size() == stop ? Stop : Continue; }; };
                    QuadTree::QueryOptions<T> options{.ordered = ordered, .workspace = &scratch};
                    if (limitNodes) options.budget.nodeVisits = limit;
                    else options.budget.candidateTests = limit;
                    const auto a = tree.forEachPair(visit(expected), options, [](const auto&, const auto&) noexcept { return true; });
                    const auto b = tree.forEachPair(visit(actual), options);
                    CHECK(a.status == b.status && expected == actual);
                    CHECK(a.counters.candidateTests == b.counters.candidateTests && a.counters.reports == b.counters.reports);
                    CHECK(a.counters.nodeVisits == b.counters.nodeVisits && a.counters.filterRejections == b.counters.filterRejections);
                }
    };
    REQUIRE(tree.build(entries, handles) == Status::Ok);
    check();
    check(true);
    REQUIRE(tree.setFilter(handles[7], {0, 0}) == Status::Ok);
    check();
    REQUIRE(tree.build(entries, handles) == Status::Ok);
    REQUIRE(tree.insert(entries[7].rect, 30, {0, 0}, 30).ok());
    check();
}

// Long ordered rows span both bitmap levels; sparse points between the long
// rows must not inherit any of their marks. Full-width ranks are not ordinals.
void int64LongRows()
{
    using T = std::int64_t;
    auto made = Tree<T>::create({});
    REQUIRE(made.ok());
    auto& tree = made.value();
    std::vector<QuadTree::BuildEntry<int, T>> input;
    for (int i = 0; i < 4200; ++i)
    {
        const T x = i % 100, y = i / 100;
        const Box<T> box = i % 131 == 0 ? Box<T>{0, y, 100, 50} : Box<T>{x, y, x, y};
        input.push_back({box, int(i), {}, std::uint64_t(i) * UINT64_C(0x9e3779b97f4a7c15)});
    }
    QuadTree::Workspace<T> scratch;
    REQUIRE(scratch.reserve(input.size()) == Status::Ok);
    REQUIRE(tree.build(input, {.workspace = &scratch}) == Status::Ok);
    for (unsigned stop : {0u, 128u, 5000u})
    {
        std::vector<Pair> a, b;
        const auto visit = [&](auto& dst) { return [&](const auto& x, const auto& y) noexcept
        { dst.emplace_back(x.payload, y.payload); return dst.size() == stop ? Stop : Continue; }; };
        QuadTree::QueryOptions<T> opts{.ordered = true, .workspace = &scratch};
        const auto ref = tree.forEachPair(visit(a), opts, [](const auto&, const auto&) noexcept { return true; });
        const auto got = tree.forEachPair(visit(b), opts);
        CHECK(ref.status == got.status && a == b);
        CHECK(ref.counters.candidateTests == got.counters.candidateTests && ref.counters.nodeVisits == got.counters.nodeVisits);
    }
}

// Pair identities on translated snapshots, including low-word wrap, the
// narrow-span cutoff, large boxes, and growth beyond the narrow span.
void translatedInt64Snapshots()
{
    using T = std::int64_t;
    for (auto boundary : {Boundary::Default, Boundary::TouchInclusive})
        for (T origin : {T(INT64_MIN), -(T(1) << 40) - 16, (T(1) << 40) + INT32_MAX - 16})
            for (T span : {T(INT32_MAX), T(INT32_MAX) + 1})
                for (T width : {T(65535), T(65536)})
                {
                    QuadTree::Config<T> config;
                    config.boundary = boundary;
                    config.smallSetThreshold = 0;
                    config.mergeThreshold = 0;
                    config.splitThreshold = 2;
                    auto made = Tree<T>::create(config);
                    REQUIRE(made.ok());
                    auto& tree = made.value();
                    std::vector<QuadTree::BuildEntry<int, T>> input;
                    std::vector<Record<T>> records;
                    const auto add = [&](Box<T> box)
                    {
                        const auto rank = input.size();
                        input.push_back({box, int(rank), {}, rank});
                        records.push_back({box, int(rank), {}, rank, {}});
                    };
                    for (T i = 0; i < 16; ++i)
                    {
                        const T x = origin + i * 8;
                        add({x, x, x + 8, x + 8});
                        add({x, x, x, x});
                    }
                    add({origin, origin, origin + width, origin + width});
                    add({origin + span, origin, origin + span, origin});
                    std::vector<Tree<T>::HandleType> handles(input.size());
                    QuadTree::Workspace<T> workspace;
                    REQUIRE(workspace.reserve(input.size() + 1) == Status::Ok);
                    REQUIRE(tree.build(input, handles, {.workspace = &workspace}) == Status::Ok);
                    for (std::size_t i = 0; i < records.size(); ++i) records[i].handle = handles[i];
                    verify(tree, records, workspace);
                    const Box<T> far{origin, origin, origin + (T(1) << 33), origin};
                    const auto inserted = tree.insert(far, int(input.size()), {}, input.size());
                    REQUIRE(inserted.ok());
                    records.push_back({far, int(input.size()), {}, input.size(), inserted.handle});
                    verify(tree, records, workspace);
                }
}

template<typename T>
void numeric()
{
    auto created = Tree<T>::create({.splitThreshold = 2, .mergeThreshold = 0, .smallSetThreshold = 0});
    REQUIRE(created.ok());
    auto tree = std::move(created).value();
    QuadTree::Workspace<T> workspace;
    REQUIRE(workspace.reserve(256) == Status::Ok);
    const T low = std::numeric_limits<T>::lowest(), high = std::numeric_limits<T>::max();
    const std::array<Box<T>, 9> boxes{{{low, low, high, high}, {low, T(0), low, T(0)},
        {high, T(0), high, T(0)}, {T(-1), T(-1), T(1), T(1)}, {T(0), T(0), T(0), T(0)},
        {T(0), T(0), T(1), T(0)}, {T(1), T(0), T(1), T(1)}, {T(1), T(1), T(2), T(2)},
        {T(0), T(-1), T(0), T(1)}}};
    std::vector<Record<T>> records;
    for (std::size_t i = 0; i < boxes.size(); ++i)
    {
        const int id = static_cast<int>(i);
        const auto inserted = tree.insert(boxes[i], id, {}, i);
        REQUIRE(inserted.ok());
        records.push_back({boxes[i], id, {}, i, inserted.handle});
    }
    verify(tree, records, workspace);
    CHECK(tree.insert(Rect<T>{high, T(0), T(1), T(1)}, 99).status == Status::InvalidGeometry);
    CHECK(tree.insert(Rect<T>{T(0), T(0), T(-1), T(1)}, 99).status == Status::InvalidGeometry);
    CHECK(tree.insert(Box<T>{T(1), T(0), T(0), T(1)}, 99).status == Status::InvalidGeometry);
    std::size_t callbacks = 0;
    CHECK(tree.query(Rect<T>{high, T(0), high, T(1)}, [&](const auto&) noexcept
        { ++callbacks; return Continue; }).status == Status::InvalidGeometry);
    CHECK(callbacks == 0);
    if constexpr (QuadTree::FloatCoordinate<T>)
    {
        const T infinity = std::numeric_limits<T>::infinity(), nan = std::numeric_limits<T>::quiet_NaN();
        CHECK(tree.insert(Box<T>{T(0), T(0), infinity, T(1)}, 99).status == Status::InvalidGeometry);
        CHECK(tree.update(records[0].handle, Box<T>{nan, T(0), T(1), T(1)}) == Status::InvalidGeometry);
        CHECK(tree.insert(Rect<T>{T(1), T(0), std::numeric_limits<T>::denorm_min(), T(1)}, 99).status == Status::InvalidGeometry);
        const T tiny = std::numeric_limits<T>::denorm_min();
        const auto subnormal = tree.insert(Rect<T>{T(0), T(0), tiny, tiny}, 99);
        REQUIRE(subnormal.ok());
        CHECK(tree.rect(subnormal.handle).maxX == tiny);
        const auto zero = tree.insert(Box<T>{-T(0), -T(0), T(0), T(0)}, 100);
        REQUIRE(zero.ok());
        CHECK(!std::signbit(tree.rect(zero.handle).minX));
        // QT-FIX-10: query and pair reproductions in separate trees.
        const T u = std::nextafter(T(1), T(2)) - T(1);
        QuadTree::Config<T> config;
        config.extent = QuadTree::Extent::Fixed;
        config.root = Box<T>{T(1), T(0), T(1) + T(5) * u, T(8)};
        config.smallSetThreshold = 0; config.splitThreshold = 1; config.mergeThreshold = 0;
        auto made = Tree<T>::create(config);
        REQUIRE(made.ok());
        auto probe = std::move(made).value();
        const auto queryItem = probe.insert(Rect<T>{T(1) + T(4) * u, T(1), u, T(1)}, 1);
        REQUIRE(queryItem.ok());
        REQUIRE(probe.insert(Box<T>{T(1), T(6), T(1), T(6)}, 1).ok());
        std::size_t hits = 0;
        CHECK(probe.query(Rect<T>{T(1) + T(5) * u, T(1.5), T(0), T(0)}, [&](const auto& entry) noexcept
            { CHECK(entry.handle == queryItem.handle); ++hits; return Continue; }).complete());
        CHECK(hits == 1);
        made = Tree<T>::create(config);
        REQUIRE(made.ok());
        probe = std::move(made).value();
        const auto item = probe.insert(Rect<T>{T(1) + T(4) * u, T(1), u, T(1)}, 1);
        const auto segment = probe.insert(Rect<T>{T(1) + T(5) * u, T(0), T(0), T(8)}, 1);
        REQUIRE(item.ok() && segment.ok());
        hits = 0;
        CHECK(probe.forEachPair([&](const auto& a, const auto& b) noexcept
        {
            CHECK(a.handle == item.handle && b.handle == segment.handle);
            ++hits;
            return Continue;
        }).complete());
        CHECK(hits == 1);

        // The same nominal x-span can round away the positive far edge in Rect form.
        const auto exact = tree.insert(Box<T>{T(-1), T(0), tiny, T(1)}, 101);
        const auto width = tree.insert(Rect<T>{T(-1), T(0), T(1), T(1)}, 101);
        REQUIRE(exact.ok() && width.ok());
        CHECK(tree.rect(exact.handle).maxX == tiny && tree.rect(width.handle).maxX == T(0));
        hits = 0;
        CHECK(tree.query(Box<T>{tiny, T(0.5), tiny, T(0.5)}, [&](const auto& entry) noexcept
        {
            if (entry.handle == exact.handle) ++hits;
            CHECK(entry.handle != width.handle);
            return Continue;
        }).complete());
        CHECK(hits == 1);
    }
}

void fixedRootContacts()
{
    using I = Tree<int>;
    using Entry = QuadTree::BuildEntry<int, int>;
    constexpr Box<int> root{0, 0, 32, 32};
    constexpr std::array<Box<int>, 19> boxes{{
        {0, 0, 16, 16}, {16, 0, 32, 16}, {0, 16, 16, 32}, {16, 16, 32, 32}, // Sibling tiles.
        {0, 0, 0, 0}, {32, 0, 32, 0}, {0, 32, 0, 32}, {32, 32, 32, 32},
        {16, 0, 16, 0}, {32, 16, 32, 16}, {16, 32, 16, 32}, {0, 16, 0, 16},
        {0, 0, 32, 0}, {32, 0, 32, 32}, {0, 32, 32, 32}, {0, 0, 0, 32},
        {8, 16, 24, 16}, {16, 8, 16, 24}, {16, 16, 16, 16}}};
    constexpr Box<int> outside[]{
        {-4, 0, 0, 32}, {32, 0, 36, 32}, {0, -4, 32, 0}, {0, 32, 32, 36},
        {-4, 0, 0, 0}, {32, 32, 36, 32}, {34, 34, 36, 36}};
    for (auto boundary : {Boundary::Default, Boundary::TouchInclusive})
        for (bool bulk : {false, true})
        {
            auto made = I::create({.extent = QuadTree::Extent::Fixed, .root = root, .boundary = boundary,
                .splitThreshold = 1, .mergeThreshold = 0, .smallSetThreshold = 0, .maxDepth = 1});
            REQUIRE(made.ok());
            auto tree = std::move(made).value();
            QuadTree::Workspace<int> workspace;
            REQUIRE(workspace.reserve(boxes.size()) == Status::Ok);
            std::vector<Record<int>> records;
            std::array<Entry, boxes.size()> input{};
            std::array<I::HandleType, boxes.size()> handles{};
            for (std::size_t i = 0; i < boxes.size(); ++i)
                input[i] = {boxes[i], 1, {}, i}; // The handles, not payloads, identify entries.
            const auto append = [&](std::size_t count)
            {
                if (bulk)
                {
                    REQUIRE(tree.build(std::span(input).first(count), handles, {.workspace = &workspace}) == Status::Ok);
                    records.clear();
                }
                for (std::size_t i = records.size(); i < count; ++i)
                {
                    if (!bulk)
                    {
                        const auto added = tree.insert(boxes[i], 1, {}, i);
                        REQUIRE(added.ok());
                        handles[i] = added.handle;
                    }
                    records.push_back({boxes[i], static_cast<int>(i), {}, i, handles[i]});
                }
                CHECK(tree.extent() == root && tree.size() == count);
            };
            const auto id = [&](I::HandleType handle) noexcept
            {
                const auto it = std::find_if(records.begin(), records.end(), [&](const auto& r) { return r.handle == handle; });
                REQUIRE(it != records.end());
                return it->id;
            };
            const auto pairStream = [&](bool ordered)
            {
                std::vector<Pair> pairs;
                const auto result = tree.forEachPair([&](const auto& a, const auto& b) noexcept
                {
                    const int x = id(a.handle), y = id(b.handle);
                    pairs.emplace_back(ordered ? Pair{x, y} : Pair{std::min(x, y), std::max(x, y)});
                    return Continue;
                }, {.ordered = ordered, .workspace = &workspace});
                CHECK(result.complete() && result.counters.reports == pairs.size());
                if (!ordered) std::sort(pairs.begin(), pairs.end());
                return pairs;
            };
            append(4);
            std::array<unsigned, 4> occupied{};
            REQUIRE(tree.forEachNode([&](const auto& node) noexcept
            {
                if (node.depth == 1 && node.entries != 0)
                {
                    const auto it = std::find(boxes.begin(), boxes.begin() + 4, node.cell);
                    REQUIRE(it != boxes.begin() + 4);
                    ++occupied[static_cast<std::size_t>(it - boxes.begin())];
                    CHECK(node.ownEntries == 1 && node.entries == 1 && node.bounds == node.cell);
                }
                return Continue;
            }).complete());
            CHECK((occupied == std::array<unsigned, 4>{1, 1, 1, 1}));
            // With only the tiles, every pair is an edge or diagonal corner contact.
            const auto siblingPairs = boundary == Boundary::Default ? std::vector<Pair>{} :
                std::vector<Pair>{{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
            CHECK(pairStream(false) == siblingPairs && pairStream(true) == siblingPairs);

            append(boxes.size());
            const auto expected = expectedPairs(records, boundary);
            CHECK(pairStream(false) == expected && pairStream(true) == expected);
            const auto checkQuery = [&](const Box<int>& box)
            {
                std::vector<int> hits, expectedHits;
                for (const auto& record : records)
                    if (overlap(box, record.box, boundary)) expectedHits.push_back(record.id);
                const auto result = tree.query(box, [&](const auto& entry) noexcept
                { hits.push_back(id(entry.handle)); return Continue; });
                CHECK(result.complete() && result.counters.reports == hits.size());
                std::sort(hits.begin(), hits.end());
                CHECK(hits == expectedHits);
            };
            for (const auto& box : boxes) checkQuery(box);
            for (const auto& box : outside) checkQuery(box);
        }
}

void contracts()
{
    using I = Tree<int>;
    CHECK(I::create({.extent = QuadTree::Extent::Fixed}).status == Status::InvalidConfiguration);
    CHECK(I::create({.extent = QuadTree::Extent::Snapshot}).status == Status::InvalidConfiguration);
    CHECK(I::create({.maxDepth = 65}).status == Status::InvalidConfiguration);
    CHECK(I::create({.splitThreshold = 0}).status == Status::InvalidConfiguration);
    CHECK(I::create({.extent = QuadTree::Extent::Fixed, .root = Box<int>{0, 0, 0, 1}}).status == Status::InvalidGeometry);
    auto created = I::create({.extent = QuadTree::Extent::Fixed, .root = Box<int>{0, 0, 10, 10}});
    REQUIRE(created.ok());
    auto tree = std::move(created).value();
    auto a = tree.insert(Box<int>{0, 0, 10, 10}, 1, {}, 0);
    REQUIRE(a.ok());
    CHECK(tree.insert(Box<int>{0, 0, 10, 10}, 1, {}, 0).status == Status::DuplicateRank);
    auto b = tree.insert(Box<int>{0, 0, 10, 10}, 1, {}, UINT64_MAX);
    REQUIRE(b.ok());
    CHECK(a.handle != b.handle); // Equal payload values are independent identities.
    CHECK(tree.update(a.handle, Box<int>{-1, 0, 1, 1}) == Status::OutOfBounds);
    CHECK((tree.rect(a.handle) == Box<int>{0, 0, 10, 10}));
    std::size_t count = 0;
    CHECK(tree.query(Box<int>{-100, -100, 100, 100}, [&](const auto&) noexcept { ++count; return Continue; }).complete());
    CHECK(count == 2);
    CHECK(tree.setFilter(a.handle, {0, 0}) == Status::Ok);
    CHECK(tree.forEachPair([](const auto&, const auto&) noexcept { return Stop; }).complete());
    CHECK(tree.setFilter(a.handle, {}) == Status::Ok);
    QuadTree::Workspace<int> workspace;
    REQUIRE(workspace.reserve(10) == Status::Ok);
    workspace.resetMovedMembership();
    const std::array duplicates{a.handle, a.handle};
    CHECK(tree.forEachPairInvolving(duplicates, [](const auto&, const auto&) noexcept { return Stop; },
        {.workspace = &workspace}).status == Status::DuplicateHandle);
    QuadTree::Workspace<int> unprepared;
    CHECK(tree.forEachPairInvolving(std::array{a.handle}, [](const auto&, const auto&) noexcept { return Stop; },
        {.workspace = &unprepared}).status == Status::InsufficientWorkspace);
    CHECK(tree.forEachPairInvolving({}, [](const auto&, const auto&) noexcept { return Stop; },
        {.ordered = true, .workspace = &unprepared}).complete());
    CHECK(tree.forEachPair([](const auto&, const auto&) noexcept { return Stop; }).status == Status::Stopped);
    std::vector<std::pair<I::HandleType, I::HandleType>> pairs(1);
    pairs.reserve(2);
    CHECK(tree.pairsTo(std::back_inserter(pairs)).complete());
    CHECK(pairs.size() == 2); // Append, never clear caller output.
    std::vector<I::HandleType> hits(1);
    hits.reserve(3);
    CHECK(tree.queryTo(Box<int>{0, 0, 10, 10}, std::back_inserter(hits)).complete());
    CHECK(hits.size() == 3);
    auto missing = tree.insert(Box<int>{0, 0, 1, 1}, 3);
    REQUIRE(missing.ok());
    CHECK(tree.forEachPair([](const auto&, const auto&) noexcept { return Stop; }, {.ordered = true}).status == Status::MissingRank);
    CHECK(tree.queryPartners(a.handle, [](const auto&) noexcept { return Stop; }, {.greaterRank = 0}).status == Status::MissingRank);
    CHECK(tree.remove(missing.handle) == Status::Ok);
    std::array<QuadTree::BuildEntry<int, int>, 2> input{{{{0, 0, 3, 3}, 3, {}, 8}, {{1, 1, 2, 2}, 4, {}, 8}}};
    std::array<I::HandleType, 2> output{a.handle, b.handle};
    CHECK(tree.build(input, output, {.workspace = &workspace}) == Status::DuplicateRank);
    CHECK(tree.valid(a.handle) && tree.valid(b.handle));
    CHECK(output[0] == a.handle && output[1] == b.handle);
    input[1].rank = 9;
    CHECK(tree.build(input, std::span<I::HandleType>(output).first(1)) == Status::InsufficientOutput);
    input[1].rect = {0, 0, 11, 1};
    CHECK(tree.build(input) == Status::OutOfBounds);
    CHECK(tree.valid(a.handle));
    std::array<QuadTree::BuildEntry<int, int>, 0> empty{};
    CHECK(tree.build(empty, {.workspace = &workspace}) == Status::Ok);
    CHECK(!tree.valid(a.handle) && tree.empty());
    for (auto growth : {QuadTree::SnapshotGrowth::Adaptive, QuadTree::SnapshotGrowth::Reject})
    {
        auto made = I::create({.extent = QuadTree::Extent::Snapshot, .snapshotGrowth = growth});
        REQUIRE(made.ok());
        auto snapshot = std::move(made).value();
        CHECK(snapshot.build(empty) == Status::Ok);
        CHECK(!snapshot.extent());
        const auto inserted = snapshot.insert(Box<int>{0, 0, 0, 0}, 0);
        CHECK(inserted.status == (growth == QuadTree::SnapshotGrowth::Reject ? Status::OutOfBounds : Status::Ok));
        std::array<QuadTree::BuildEntry<int, int>, 1> point{{{{2, 2, 2, 2}, 1}}};
        CHECK(snapshot.build(point) == Status::Ok);
        CHECK(snapshot.extent() == point[0].rect);
    }
}

template<bool Instrumented>
void boundedCollection()
{
    using I = QuadTree::Index<int, int, QuadTree::Checked, Instrumented>;
    using Handle = typename I::HandleType;
    using HandlePair = typename I::HandlePair;
    TestResource resource;
    constexpr Box<int> box{0, 0, 8, 8};
    auto made = I::create({}, &resource);
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    QuadTree::Workspace<int> workspace(&resource), unprepared(&resource);
    REQUIRE(workspace.reserve(3) == Status::Ok);
    const auto a = tree.insert(box, 30, {}, 3);
    const auto b = tree.insert(box, 10, {}, 1);
    const auto c = tree.insert(box, 20, {}, 2);
    REQUIRE(a.ok() && b.ok() && c.ok());

    resource.failAll = true;
    const auto allocations = resource.allocations;
    for (bool ordered : {false, true})
    {
        const auto options = QuadTree::QueryOptions<int>{.ordered = ordered, .workspace = ordered ? &workspace : nullptr};
        const std::array hits = ordered ? std::array{b.handle, c.handle, a.handle} : std::array{a.handle, b.handle, c.handle};
        const std::array pairs{HandlePair{hits[0], hits[1]}, HandlePair{hits[0], hits[2]}, HandlePair{hits[1], hits[2]}};
        for (std::size_t capacity = 0; capacity <= 4; ++capacity)
        {
            std::array<Handle, 4> output_hits{};
            std::array<HandlePair, 4> output_pairs{};
            const auto query = tree.queryTo(box, std::span(output_hits).first(capacity), options);
            const auto self = tree.pairsTo(std::span(output_pairs).first(capacity), options);
            const std::size_t written = std::min(capacity, hits.size());
            const auto status = capacity < hits.size() ? Status::InsufficientOutput : Status::Ok;
            const auto reports = Instrumented ? std::min(capacity + 1, hits.size()) : 0;
            for (const auto& result : {query, self})
            {
                CHECK(result.status == status && result.complete() == (status == Status::Ok));
                CHECK(result.count == written && result.counters.reports == reports);
            }
            for (std::size_t i = 0; i < output_hits.size(); ++i)
            {
                CHECK(output_hits[i] == (i < written ? hits[i] : Handle{}));
                CHECK(output_pairs[i] == (i < written ? pairs[i] : HandlePair{}));
            }
        }
        const auto no_hits = tree.queryTo(Box<int>{20, 20, 30, 30}, std::span<Handle>{}, options);
        CHECK(no_hits.complete() && no_hits.count == 0);
    }

    std::array<Handle, 3> hits{};
    std::array<HandlePair, 3> pairs{};
    const auto invalid = tree.queryTo(Rect<int>{0, 0, -1, 1}, std::span(hits));
    CHECK(invalid.status == Status::InvalidGeometry && invalid.count == 0);
    const auto no_query_space = tree.queryTo(box, std::span(hits), {.ordered = true, .workspace = &unprepared});
    const auto no_pair_space = tree.pairsTo(std::span(pairs), {.ordered = true, .workspace = &unprepared});
    CHECK(no_query_space.status == Status::InsufficientWorkspace && no_query_space.count == 0);
    CHECK(no_pair_space.status == Status::InsufficientWorkspace && no_pair_space.count == 0);
    CHECK(hits == decltype(hits){} && pairs == decltype(pairs){});

    const auto query_budget = tree.queryTo(box, std::span(hits), {.budget = {.candidateTests = 1}});
    const auto pair_budget = tree.pairsTo(std::span(pairs), {.budget = {.candidateTests = 1}});
    CHECK(query_budget.status == Status::BudgetExhausted && query_budget.count == 1 && hits[0] == a.handle);
    CHECK(pair_budget.status == Status::BudgetExhausted && pair_budget.count == 1);
    CHECK((pairs[0] == HandlePair{a.handle, b.handle}));
    CHECK(hits[1] == Handle{} && pairs[1] == HandlePair{});

    tree.clear();
    const auto empty = tree.pairsTo(std::span<HandlePair>{});
    CHECK(empty.complete() && empty.count == 0);
    CHECK(resource.allocations == allocations);
}

void budgets()
{
    auto made = Tree<int>::create({.splitThreshold = 2, .mergeThreshold = 0, .smallSetThreshold = 0});
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    std::vector<Tree<int>::HandleType> moved;
    for (int i = 0; i < 13; ++i)
    {
        auto inserted = tree.insert(Box<int>{i % 4, i % 5, i % 4 + 10, i % 5 + 10}, int(i), {}, 100 - std::uint64_t(i * 7));
        REQUIRE(inserted.ok());
        if (i % 2 == 0) moved.push_back(inserted.handle);
    }
    QuadTree::Workspace<int> workspace;
    REQUIRE(workspace.reserve(20) == Status::Ok);
    for (bool moved_only : {false, true})
    {
        std::vector<RankPair> complete;
        auto full = [&](const auto& a, const auto& b) noexcept { complete.emplace_back(*a.rank, *b.rank); return Continue; };
        const auto unlimited = moved_only ? tree.forEachPairInvolving(moved, full, {.ordered = true, .workspace = &workspace}) :
            tree.forEachPair(full, {.ordered = true, .workspace = &workspace});
        REQUIRE(unlimited.complete());
        for (bool nodes : {false, true})
        {
            const std::uint64_t total = nodes ? unlimited.counters.nodeVisits : unlimited.counters.candidateTests;
            for (std::uint64_t limit = 0; limit <= total + 1; ++limit)
            {
                std::vector<RankPair> partial;
                auto visitor = [&](const auto& a, const auto& b) noexcept { partial.emplace_back(*a.rank, *b.rank); return Continue; };
                QuadTree::QueryOptions<int> options{.ordered = true, .workspace = &workspace};
                if (nodes) options.budget.nodeVisits = limit;
                else options.budget.candidateTests = limit;
                const auto result = moved_only ? tree.forEachPairInvolving(moved, visitor, options) : tree.forEachPair(visitor, options);
                CHECK(result.status == (limit >= total ? Status::Ok : Status::BudgetExhausted));
                CHECK(partial.size() <= complete.size());
                CHECK(std::equal(partial.begin(), partial.end(), complete.begin()));
                CHECK((nodes ? result.counters.nodeVisits : result.counters.candidateTests) <= limit);
                CHECK(result.counters.reports == partial.size());
            }
        }
        for (std::size_t stop = 1; stop <= complete.size(); ++stop)
        {
            std::vector<RankPair> partial;
            auto visitor = [&](const auto& a, const auto& b) noexcept
            { partial.emplace_back(*a.rank, *b.rank); return partial.size() == stop ? Stop : Continue; };
            const auto result = moved_only ? tree.forEachPairInvolving(moved, visitor, {.ordered = true, .workspace = &workspace}) :
                tree.forEachPair(visitor, {.ordered = true, .workspace = &workspace});
            CHECK(result.status == Status::Stopped);
            CHECK(partial.size() == stop);
            CHECK(std::equal(partial.begin(), partial.end(), complete.begin()));
        }
    }
    auto pair_result = tree.forEachPair([](const auto&, const auto&) noexcept { return Continue; });
    CHECK(pair_result.counters.candidateTests == 78);
    pair_result = tree.forEachPair([](const auto&, const auto&) noexcept { return Continue; }, {.budget = {.candidateTests = 78}});
    CHECK(pair_result.complete());
    CHECK(tree.forEachPair([](const auto&, const auto&) noexcept { return Continue; },
        {.budget = {.candidateTests = 0}}).status == Status::BudgetExhausted);
}

void orderedPermutations()
{
    using I = Tree<int>;
    const std::vector<Record<int>> records{{{0, 0, 32, 32}, 0, {}, 47, {}},
        {{2, 2, 8, 8}, 1, {}, UINT64_MAX, {}}, {{6, 6, 12, 12}, 2, {}, 0, {}},
        {{12, 8, 16, 12}, 3, {}, 11, {}}, {{24, 24, 29, 29}, 4, {}, 40, {}},
        {{27, 27, 31, 31}, 5, {}, 3, {}}, {{16, 12, 16, 12}, 6, {}, 23, {}},
        {{4, 4, 7, 7}, 7, {0, 0}, 17, {}}};
    constexpr std::array<std::array<std::size_t, 8>, 3> permutations{{
        {0, 1, 2, 3, 4, 5, 6, 7}, {7, 6, 5, 4, 3, 2, 1, 0}, {3, 5, 0, 2, 4, 7, 1, 6}}};
    const auto rank = [&](int id) noexcept { return records[static_cast<std::size_t>(id)].rank; };
    for (Boundary boundary : {Boundary::Default, Boundary::TouchInclusive})
    {
        auto expected = expectedPairs(records, boundary);
        REQUIRE(!expected.empty());
        for (auto& [a, b] : expected)
            if (rank(b) < rank(a)) std::swap(a, b);
        std::sort(expected.begin(), expected.end(), [&](Pair a, Pair b)
        { return RankPair{rank(a.first), rank(a.second)} < RankPair{rank(b.first), rank(b.second)}; });

        TestResource resource;
        auto config = QuadTree::Config<int>{.boundary = boundary, .splitThreshold = 2,
            .mergeThreshold = 0, .smallSetThreshold = 0};
        auto inserted = I::create(config, &resource);
        config.extent = QuadTree::Extent::Snapshot;
        config.snapshotGrowth = QuadTree::SnapshotGrowth::Reject;
        auto rebuilt = I::create(config, &resource);
        REQUIRE(inserted.ok() && rebuilt.ok());
        auto persistent = std::move(inserted).value();
        auto snapshot = std::move(rebuilt).value();
        QuadTree::Workspace<int> workspace(&resource);
        REQUIRE(workspace.reserve(records.size()) == Status::Ok);
        using Handles = std::array<I::HandleType, 8>;
        const auto checkSequence = [&](const I& tree, const Handles& handles)
        {
            REQUIRE(tree.stats().nodes > 1);
            const auto allocations = resource.allocations;
            std::size_t next = 0;
            const auto result = tree.forEachPair([&](const auto& a, const auto& b) noexcept
            {
                REQUIRE(next < expected.size());
                const auto [x, y] = expected[next++];
                CHECK(a.handle == handles[static_cast<std::size_t>(x)] && b.handle == handles[static_cast<std::size_t>(y)]);
                CHECK(a.rank == rank(x) && b.rank == rank(y));
                return Continue;
            }, {.ordered = true, .workspace = &workspace});
            CHECK(result.complete() && next == expected.size() && result.counters.reports == next);
            CHECK(resource.allocations == allocations);
        };
        Handles prior{};
        for (const auto& permutation : permutations)
        {
            persistent.clear();
            std::array<QuadTree::BuildEntry<int, int>, 8> input{};
            Handles byIdentity{}, output{};
            for (std::size_t i = 0; i < permutation.size(); ++i)
            {
                const auto id = permutation[i];
                const auto& r = records[id];
                input[i] = {r.box, 1, r.filter, r.rank}; // Equal payloads cannot identify endpoints.
                const auto entry = persistent.insert(r.box, 1, r.filter, r.rank);
                REQUIRE(entry.ok());
                byIdentity[id] = entry.handle;
            }
            checkSequence(persistent, byIdentity);
            REQUIRE(snapshot.build(input, output, {.workspace = &workspace}) == Status::Ok);
            for (std::size_t i = 0; i < output.size(); ++i)
            {
                CHECK(!snapshot.valid(prior[i]));
                byIdentity[permutation[i]] = output[i];
            }
            checkSequence(snapshot, byIdentity);
            prior = output;
        }
    }
}

void crossAndSelections()
{
    auto made = Tree<int>::create({.smallSetThreshold = 0});
    auto other_made = QuadTree::Index<long, int>::create({.smallSetThreshold = 0});
    REQUIRE(made.ok() && other_made.ok());
    auto tree = std::move(made).value();
    auto other = std::move(other_made).value();
    for (int i = 0; i < 40; ++i)
    {
        REQUIRE(tree.insert(Box<int>{i, i, i + 4, i + 4}, int(i), {}, std::uint64_t(40 - i)).ok());
        REQUIRE(other.insert(Box<int>{i, i, i + 2, i + 2}, long(i), {}, std::uint64_t(i)).ok());
    }
    QuadTree::Workspace<int> workspace;
    REQUIRE(workspace.reserve(80) == Status::Ok);
    for (bool ordered : {false, true})
    {
        std::vector<std::pair<int, long>> found, expected;
        std::vector<RankPair> ranks;
        for (int a = 0; a < 40; ++a)
            for (long b = 0; b < 40; ++b)
                if (overlap(Box<int>{a, a, a + 4, a + 4}, Box<int>{int(b), int(b), int(b) + 2, int(b) + 2}, Boundary::Default) && a < b)
                    expected.emplace_back(a, b);
        const auto result = tree.forEachPair(other, [&](const auto& a, const auto& b) noexcept
        { found.emplace_back(a.payload, b.payload); ranks.emplace_back(*a.rank, *b.rank); return Continue; },
            {.ordered = ordered, .workspace = &workspace}, [](const auto& a, const auto& b) noexcept { return a.payload < b.payload; });
        CHECK(result.complete());
        if (ordered) CHECK(std::is_sorted(ranks.begin(), ranks.end()));
        std::sort(found.begin(), found.end());
        CHECK(found == expected);
        std::size_t left_calls = 0, right_calls = 0;
        found.clear(); expected.clear(); ranks.clear();
        for (int a = 0; a < 40; a += 2)
            for (long b = 1; b < 40; b += 2)
                if (overlap(Box<int>{a, a, a + 4, a + 4}, Box<int>{int(b), int(b), int(b) + 4, int(b) + 4}, Boundary::Default))
                    expected.emplace_back(a, b);
        CHECK(tree.forEachPairSelected([&](const auto& a) noexcept { ++left_calls; return a.payload % 2 == 0; },
            [&](const auto& a) noexcept { ++right_calls; return a.payload % 2 == 1; },
            [&](const auto& a, const auto& b) noexcept
            { found.emplace_back(a.payload, b.payload); ranks.emplace_back(*a.rank, *b.rank); return Continue; },
            {.ordered = ordered, .workspace = &workspace}).complete());
        CHECK(left_calls == tree.size() && right_calls == tree.size());
        if (ordered) CHECK(std::is_sorted(ranks.begin(), ranks.end()));
        std::sort(found.begin(), found.end());
        CHECK(found == expected);
    }
    CHECK(tree.forEachPair(tree, [](const auto&, const auto&) noexcept { return Stop; }).status == Status::InvalidArgument);
    CHECK(tree.forEachPairSelected([](const auto&) noexcept { return true; }, [](const auto&) noexcept { return true; },
        [](const auto&, const auto&) noexcept { return Stop; }, {.workspace = &workspace}).status == Status::InvalidArgument);
    auto mismatch = QuadTree::Index<long, int>::create({.boundary = Boundary::TouchInclusive});
    REQUIRE(mismatch.ok());
    CHECK(tree.forEachPair(mismatch.value(), [](const auto&, const auto&) noexcept { return Stop; }).status == Status::IncompatiblePolicy);
    auto other_coords = QuadTree::Index<long, double>::create();
    REQUIRE(other_coords.ok());
    CHECK(tree.forEachPair(other_coords.value(), [](const auto&, const auto&) noexcept { return Stop; }).status == Status::IncompatiblePolicy);
}

template<bool Instrumented>
void crossPrefixes()
{
    using I = QuadTree::Index<int, int, QuadTree::Checked, Instrumented>;
    using Other = QuadTree::Index<long, int, QuadTree::Unchecked, Instrumented>;
    TestResource leftResource, rightResource;
    auto left = I::create({.smallSetThreshold = 0}, &leftResource);
    auto right = Other::create({.extent = QuadTree::Extent::Snapshot,
        .snapshotGrowth = QuadTree::SnapshotGrowth::Reject, .smallSetThreshold = 0}, &rightResource);
    REQUIRE(left.ok() && right.ok());
    constexpr Box<int> box{0, 0, 8, 8};
    const auto a = left.value().insert(box, 1, {}, 7);
    const auto b = left.value().insert(box, 1, {}, 5);
    REQUIRE(a.ok() && b.ok());
    const std::array<QuadTree::BuildEntry<long, int>, 3> input{{{box, 1, {}, 30}, {box, 1, {}, 10}, {box, 1, {}, 20}}};
    std::array<typename Other::HandleType, 3> partners;
    REQUIRE(right.value().build(input, partners) == Status::Ok);
    QuadTree::Workspace<int> workspace(&leftResource);
    REQUIRE(workspace.reserve(3) == Status::Ok);
    const auto allocations = leftResource.allocations + rightResource.allocations;
    leftResource.failAll = rightResource.failAll = true;
    using HandlePair = std::pair<typename I::HandleType, typename Other::HandleType>;
    const std::vector<HandlePair> orderedPairs{{b.handle, partners[1]}, {b.handle, partners[2]}, {b.handle, partners[0]},
        {a.handle, partners[1]}, {a.handle, partners[2]}, {a.handle, partners[0]}};
    for (bool ordered : {false, true})
    {
        std::vector<HandlePair> actual, discovered;
        actual.reserve(orderedPairs.size());
        discovered.reserve(orderedPairs.size());
        auto run = [&](QuadTree::WorkBudget budget, std::size_t stop = 0)
        {
            actual.clear(); discovered.clear();
            const auto result = left.value().forEachPair(right.value(), [&](const auto& x, const auto& y) noexcept
            {
                REQUIRE(actual.size() < orderedPairs.size());
                actual.emplace_back(x.handle, y.handle);
                return actual.size() == stop ? Stop : Continue;
            }, {.ordered = ordered, .workspace = &workspace, .budget = budget},
            [&](const auto& x, const auto& y) noexcept
            {
                REQUIRE(discovered.size() < orderedPairs.size());
                discovered.emplace_back(x.handle, y.handle);
                return true;
            });
            if constexpr (Instrumented)
            {
                const auto c = result.counters;
                CHECK(c.candidateTests == discovered.size() && c.predicateCalls == discovered.size() &&
                    c.reports == actual.size() && c.filterRejections == 0);
            }
            else checkZeroCounters(result.counters);
            return result;
        };
        const auto full = run({});
        REQUIRE(full.ok() && actual.size() == orderedPairs.size());
        REQUIRE(discovered.size() == orderedPairs.size());
        const auto expected = actual, expectedDiscovery = discovered;
        if (ordered)
        {
            CHECK(expected == orderedPairs);
            CHECK(expectedDiscovery.front().first == expected.front().first &&
                expectedDiscovery.front().second != expected.front().second);
        }
        else CHECK(std::is_permutation(expected.begin(), expected.end(), orderedPairs.begin()));
        if constexpr (Instrumented) CHECK(full.counters.nodeVisits == 2);
        for (std::uint64_t nodes = 0; nodes <= 3; ++nodes)
            for (std::uint64_t candidates = 0; candidates <= 7; ++candidates)
            {
                const auto result = run({.candidateTests = candidates, .nodeVisits = nodes});
                // Each left entry scans one node and tests three right entries.
                const auto work = std::min({candidates, 3 * nodes, std::uint64_t{6}});
                const auto prefix = ordered ? work / 3 * 3 : work;
                CHECK(result.status == (work == 6 ? Status::Ok : Status::BudgetExhausted));
                if constexpr (Instrumented) CHECK(result.counters.nodeVisits == std::min({nodes, candidates / 3 + 1, std::uint64_t{2}}));
                CHECK(discovered.size() == work && std::equal(discovered.begin(), discovered.end(), expectedDiscovery.begin()));
                CHECK(actual.size() == prefix && std::equal(actual.begin(), actual.end(), expected.begin()));
            }
        for (std::size_t stop = 1; stop <= expected.size(); ++stop)
        {
            CHECK(run({}, stop).status == Status::Stopped);
            CHECK(actual.size() == stop && std::equal(actual.begin(), actual.end(), expected.begin()));
        }
        CHECK(run({.candidateTests = 3, .nodeVisits = 1}, 1).status == Status::Stopped);
        CHECK(actual.size() == 1 && actual.front() == expected.front());
        CHECK(run({}).ok() && actual == expected && discovered == expectedDiscovery); // Reuse after partial rows.
    }
    CHECK(leftResource.allocations + rightResource.allocations == allocations);

    if constexpr (Instrumented)
    {
        // Convenience ordered cross allocates on the left resource only.
        leftResource.failAll = false;
        const auto leftBytes = leftResource.bytes;
        const auto rightAllocations = rightResource.allocations;
        for (std::size_t failure = 1; failure <= 5; ++failure)
        {
            const auto count = leftResource.allocations;
            leftResource.failAt = count + failure;
            std::vector<HandlePair> actual;
            const auto result = left.value().forEachPair(right.value(), [&](const auto& x, const auto& y) noexcept
            { actual.emplace_back(x.handle, y.handle); return Continue; }, {.ordered = true});
            if (failure <= 4)
            {
                CHECK(result.status == Status::AllocationFailed && actual.empty());
                CHECK(leftResource.allocations == count + failure);
            }
            else
                CHECK(result.complete() && actual == orderedPairs && leftResource.allocations == count + 4);
            CHECK(leftResource.bytes == leftBytes && rightResource.allocations == rightAllocations);
            leftResource.failAt = 0;
        }
    }
}

void crossAfterMutations()
{
    using I = Tree<int>;
    using Other = QuadTree::Index<long, int>;
    using HandlePair = std::pair<I::HandleType, Other::HandleType>;
    constexpr Box<int> far{4096, 4096, 4100, 4100};
    for (Boundary boundary : {Boundary::Default, Boundary::TouchInclusive})
    {
        TestResource leftResource, rightResource;
        auto dynamic = I::create({.boundary = boundary, .splitThreshold = 2, .mergeThreshold = 1,
            .smallSetThreshold = 0}, &leftResource);
        auto snapshot = Other::create({.extent = QuadTree::Extent::Snapshot,
            .snapshotGrowth = QuadTree::SnapshotGrowth::Reject, .boundary = boundary,
            .splitThreshold = 2, .mergeThreshold = 1, .smallSetThreshold = 0}, &rightResource);
        REQUIRE(dynamic.ok() && snapshot.ok());
        auto& left = dynamic.value();
        const auto& right = snapshot.value();
        // Oracle records in rank order, independent of dense storage and traversal.
        constexpr Filter leftFilter{1, 2}, rightFilter{2, 1};
        std::array<QuadTree::BuildEntry<int, int>, 3> entries{{
            {{8, 8, 12, 12}, 1, leftFilter, 10}, {{10, 10, 14, 14}, 1, leftFilter, 40},
            {{48, 48, 52, 52}, 1, leftFilter, 70}}};
        const std::array<QuadTree::BuildEntry<long, int>, 5> input{{
            {far, 1, rightFilter, 10}, {{12, 12, 16, 16}, 1, rightFilter, 20},
            {{9, 9, 11, 11}, 1, rightFilter, 30}, {{48, 48, 52, 52}, 1, rightFilter, 40},
            {{52, 52, 56, 56}, 1, rightFilter, 50}}};
        std::array<Other::HandleType, input.size()> rhs{};
        REQUIRE(snapshot.value().build(input, rhs) == Status::Ok);
        REQUIRE(right.stats().nodes > 1);
        rightResource.failAll = true;
        std::array<I::HandleType, entries.size()> lhs{};
        const auto insert = [&](std::size_t i)
        {
            const auto& entry = entries[i];
            const auto added = left.insert(entry.rect, entry.payload, entry.filter, entry.rank);
            REQUIRE(added.ok());
            lhs[i] = added.handle;
        };
        for (std::size_t i : {2u, 0u, 1u}) insert(i);
        QuadTree::Workspace<int> workspace(&leftResource);
        REQUIRE(workspace.reserve(input.size()) == Status::Ok);
        const auto check = [&](std::size_t strict, std::size_t touching)
        {
            std::vector<HandlePair> expected;
            for (std::size_t i = 0; i < lhs.size(); ++i)
                for (std::size_t j = 0; j < rhs.size(); ++j)
                    if (lhs[i] != I::HandleType{} && overlap(entries[i].rect, input[j].rect, boundary) &&
                        accepts(entries[i].filter, input[j].filter))
                        expected.emplace_back(lhs[i], rhs[j]);
            CHECK(expected.size() == (boundary == Boundary::Default ? strict : touching));
            const auto leftAllocations = leftResource.allocations, rightAllocations = rightResource.allocations;
            leftResource.failAll = true;
            for (bool ordered : {false, true})
            {
                std::vector<HandlePair> actual;
                actual.reserve(lhs.size() * rhs.size());
                const auto result = left.forEachPair(right, [&](const auto& a, const auto& b) noexcept
                {
                    REQUIRE(actual.size() < actual.capacity());
                    actual.emplace_back(a.handle, b.handle);
                    return Continue;
                }, {.ordered = ordered, .workspace = &workspace});
                CHECK(result.complete() && result.counters.reports == expected.size());
                if (ordered) CHECK(actual == expected);
                else CHECK(std::is_permutation(actual.begin(), actual.end(), expected.begin(), expected.end()));
            }
            CHECK(leftResource.allocations == leftAllocations && rightResource.allocations == rightAllocations);
            leftResource.failAll = false;
        };

        REQUIRE(left.extent() && left.extent()->maxX < far.minX);
        check(4, 6);
        const auto near = entries[1].rect;
        REQUIRE(left.update(lhs[1], far) == Status::Ok);
        CHECK(left.extent() && left.extent()->maxX >= far.maxX);
        entries[1].rect = far;
        check(3, 5);
        REQUIRE(left.setFilter(lhs[1], {1, 0}) == Status::Ok);
        entries[1].filter = {1, 0};
        check(2, 4);
        REQUIRE(left.setFilter(lhs[1], leftFilter) == Status::Ok);
        entries[1].filter = leftFilter;
        check(3, 5);
        const auto removed = lhs[0];
        REQUIRE(left.remove(removed) == Status::Ok);
        CHECK(!left.valid(removed));
        lhs[0] = {};
        check(2, 3);
        insert(0);
        CHECK(lhs[0] != removed && !left.valid(removed));
        check(3, 5);
        REQUIRE(left.update(lhs[1], near) == Status::Ok);
        entries[1].rect = near;
        check(4, 6);
    }
}

template<bool Instrumented>
void selectionPrefixes()
{
    using I = QuadTree::Index<int, int, QuadTree::Checked, Instrumented>;
    using HandlePair = std::pair<typename I::HandleType, typename I::HandleType>;
    TestResource resource;
    auto made = I::create({.smallSetThreshold = 0}, &resource);
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    constexpr Box<int> box{0, 0, 8, 8};
    const std::array<QuadTree::BuildEntry<int, int>, 6> input{{
        {box, 1, {}, 70}, {box, 1, {}, 30}, {box, 1, {}, 50}, {box, 1, {}, 10}, {box, 1, {}, 20}, {box, 1}}};
    std::array<typename I::HandleType, 6> h{};
    QuadTree::Workspace<int> workspace(&resource);
    REQUIRE(workspace.reserve(input.size()) == Status::Ok);
    REQUIRE(tree.build(input, h, {.workspace = &workspace}) == Status::Ok);
    REQUIRE(tree.stats().nodes == 1);
    const auto allocations = resource.allocations;
    resource.failAll = true;
    // One unranked entry belongs to neither selection; equal payloads cannot identify pairs.
    const std::vector<HandlePair> ranked{{h[2], h[3]}, {h[2], h[4]}, {h[2], h[1]},
        {h[0], h[3]}, {h[0], h[4]}, {h[0], h[1]}};
    for (bool ordered : {false, true})
    {
        std::vector<HandlePair> actual, discovered;
        actual.reserve(ranked.size());
        discovered.reserve(ranked.size());
        auto run = [&](QuadTree::WorkBudget budget, std::size_t stop = 0)
        {
            actual.clear(); discovered.clear();
            std::array<unsigned, 6> leftCalls{}, rightCalls{};
            const auto mark = [&](auto handle, auto& calls)
            {
                const auto it = std::find(h.begin(), h.end(), handle);
                REQUIRE(it != h.end());
                ++calls[static_cast<std::size_t>(it - h.begin())];
            };
            const auto result = tree.forEachPairSelected([&](const auto& entry) noexcept
                { mark(entry.handle, leftCalls); return entry.handle == h[0] || entry.handle == h[2]; },
                [&](const auto& entry) noexcept
                { mark(entry.handle, rightCalls); return entry.handle == h[1] || entry.handle == h[3] || entry.handle == h[4]; },
                [&](const auto& a, const auto& b) noexcept
                {
                    REQUIRE(actual.size() < ranked.size());
                    actual.emplace_back(a.handle, b.handle);
                    return actual.size() == stop ? Stop : Continue;
                }, {.ordered = ordered, .workspace = &workspace, .budget = budget},
                [&](const auto& a, const auto& b) noexcept
                {
                    REQUIRE(discovered.size() < ranked.size());
                    discovered.emplace_back(a.handle, b.handle);
                    return true;
                });
            CHECK(std::all_of(leftCalls.begin(), leftCalls.end(), [](unsigned n) { return n == 1; }));
            CHECK(std::all_of(rightCalls.begin(), rightCalls.end(), [](unsigned n) { return n == 1; }));
            const auto c = result.counters;
            CHECK(c.filterRejections == 0);
            if constexpr (Instrumented)
                CHECK(c.candidateTests == discovered.size() && c.predicateCalls == discovered.size() && c.reports == actual.size());
            else
                CHECK(c.candidateTests == 0 && c.nodeVisits == 0 && c.predicateCalls == 0 && c.reports == 0);
            return result;
        };
        REQUIRE(run({}).complete() && actual.size() == ranked.size());
        const auto expected = actual;
        if (ordered)
        {
            CHECK(expected == ranked);
            REQUIRE(discovered.size() == ranked.size());
            CHECK(discovered.front().first == ranked.front().first && discovered.front().second != ranked.front().second);
        }
        else
            CHECK(std::is_permutation(expected.begin(), expected.end(), ranked.begin()));
        const auto discoveredFull = discovered;
        for (std::uint64_t nodes = 0; nodes <= 2; ++nodes)
            for (std::uint64_t candidates = 0; candidates <= 6; ++candidates)
            {
                const auto result = run({.candidateTests = candidates, .nodeVisits = nodes});
                // Each source scans one node and tests three selected partners.
                const auto work = std::min(candidates, 3 * nodes);
                const auto prefix = ordered ? work / 3 * 3 : work;
                CHECK(result.status == (work == 6 ? Status::Ok : Status::BudgetExhausted));
                if constexpr (Instrumented) CHECK(result.counters.nodeVisits == std::min(nodes, candidates / 3 + 1));
                CHECK(discovered.size() == work && std::equal(discovered.begin(), discovered.end(), discoveredFull.begin()));
                CHECK(actual.size() == prefix && std::equal(actual.begin(), actual.end(), expected.begin()));
            }
        for (std::size_t stop = 1; stop <= expected.size(); ++stop)
        {
            CHECK(run({}, stop).status == Status::Stopped);
            CHECK(actual.size() == stop && std::equal(actual.begin(), actual.end(), expected.begin()));
        }
        CHECK(run({.candidateTests = 3, .nodeVisits = 1}, 1).status == Status::Stopped);
        CHECK(actual.size() == 1 && actual.front() == expected.front());
        CHECK(run({}).complete() && actual == expected);
    }
    CHECK(resource.allocations == allocations);
}

void filterPolicyTruthTable()
{
    using I = Tree<int>;
    constexpr Box<int> base{0, 0, 4, 4};
    struct Contact { Box<int> box; bool strict, touch; };
    constexpr Contact contacts[]{
        {{3, 3, 5, 5}, true, true},  // Interior overlap.
        {{4, 1, 6, 3}, false, true}, // Edge contact.
        {{4, 4, 6, 6}, false, true}, // Corner contact.
        {{5, 1, 7, 3}, false, false},
        {{4, 4, 4, 4}, true, true},  // Point on the corner.
        {{4, 4, 4, 6}, true, true},  // Segment endpoint on the corner; both axes are closed.
        {{4, 5, 4, 6}, false, false},
    };
    constexpr std::array<std::uint64_t, 4> bits{0, 1, std::uint64_t{1} << 63, UINT64_MAX};
    // Literal intersection table for the four bit sets, independent of the runtime bitwise test.
    constexpr bool intersects[4][4]{{false, false, false, false}, {false, true, false, true},
        {false, false, true, true}, {false, true, true, true}};
    for (auto boundary : {Boundary::Default, Boundary::TouchInclusive})
        for (bool snapshot : {false, true})
        {
            TestResource resource;
            auto config = QuadTree::Config<int>{.boundary = boundary, .splitThreshold = 1,
                .mergeThreshold = 0, .smallSetThreshold = 0};
            if (snapshot)
            {
                config.extent = QuadTree::Extent::Snapshot;
                config.snapshotGrowth = QuadTree::SnapshotGrowth::Reject;
            }
            auto made = I::create(config, &resource);
            REQUIRE(made.ok());
            auto tree = std::move(made).value();
            QuadTree::Workspace<int> workspace(&resource);
            REQUIRE(workspace.reserve(3) == Status::Ok);
            for (const auto& contact : contacts)
            {
                const std::array<QuadTree::BuildEntry<int, int>, 3> input{{
                    {base, 1, {}, 30}, {base, 1, {}, 20}, {contact.box, 1, {}, 10}}};
                std::array<I::HandleType, 3> handles{};
                if (snapshot)
                    REQUIRE(tree.build(input, handles, {.workspace = &workspace}) == Status::Ok);
                else
                {
                    tree.clear();
                    for (std::size_t i = 0; i < input.size(); ++i)
                    {
                        const auto& entry = input[i];
                        const auto added = tree.insert(entry.rect, entry.payload, entry.filter, entry.rank);
                        REQUIRE(added.ok());
                        handles[i] = added.handle;
                    }
                }
                const auto allocations = resource.allocations;
                resource.failAll = true;
                const bool touches = boundary == Boundary::Default ? contact.strict : contact.touch;
                for (std::size_t ac = 0; ac < bits.size(); ++ac)
                    for (std::size_t am = 0; am < bits.size(); ++am)
                        for (std::size_t bc = 0; bc < bits.size(); ++bc)
                            for (std::size_t bm = 0; bm < bits.size(); ++bm)
                            {
                                const Filter a{bits[ac], bits[am]}, b{bits[bc], bits[bm]};
                                REQUIRE(tree.setFilter(handles[0], a) == Status::Ok);
                                REQUIRE(tree.setFilter(handles[1], a) == Status::Ok);
                                REQUIRE(tree.setFilter(handles[2], b) == Status::Ok);
                                const unsigned ab = touches && intersects[ac][bm] && intersects[bc][am];
                                const std::array<unsigned, 3> expected{intersects[ac][am], ab, ab}; // 01, 02, 12.
                                const auto count = [&](auto x, auto y, auto& pairs) noexcept
                                {
                                    const auto left = std::find(handles.begin(), handles.end(), x);
                                    const auto right = std::find(handles.begin(), handles.end(), y);
                                    REQUIRE(left != handles.end() && right != handles.end() && left != right);
                                    ++pairs[static_cast<std::size_t>((left - handles.begin()) + (right - handles.begin()) - 1)];
                                };
                                for (bool ordered : {false, true})
                                    for (bool accept : {false, true})
                                    {
                                        std::array<unsigned, 3> predicates{}, reports{};
                                        const auto result = tree.forEachPair([&](const auto& x, const auto& y) noexcept
                                        { count(x.handle, y.handle, reports); return Continue; },
                                            {.ordered = ordered, .workspace = &workspace},
                                            [&](const auto& x, const auto& y) noexcept
                                            { count(x.handle, y.handle, predicates); return accept; });
                                        CHECK(result.complete() && predicates == expected);
                                        CHECK(reports == (accept ? expected : std::array<unsigned, 3>{}));
                                        CHECK(result.counters.predicateCalls == std::accumulate(predicates.begin(), predicates.end(), 0u));
                                        CHECK(result.counters.reports == std::accumulate(reports.begin(), reports.end(), 0u));
                                    }
                            }
                CHECK(resource.allocations == allocations);
                resource.failAll = false;
            }
        }
}

void filterPruningWork()
{
    using I = Tree<int>;
    constexpr std::array<Filter, 2> filters{{{1, 2}, {2, 1}}};
    std::array<QuadTree::Counters, 2> work{};
    for (std::size_t layout = 0; layout < work.size(); ++layout)
    {
        auto made = I::create({.extent = QuadTree::Extent::Fixed, .root = Box<int>{0, 0, 32, 32},
            .splitThreshold = 2, .mergeThreshold = 0, .smallSetThreshold = 0, .maxDepth = 1});
        REQUIRE(made.ok());
        std::vector<Record<int>> records;
        for (int i = 0; i < 32; ++i)
        {
            const int x = i < 16 ? 8 : 24;
            const Box<int> box{x, 8, x, 8};
            const auto filter = filters[static_cast<std::size_t>(layout == 0 ? i / 16 : i % 2)];
            auto result = made.value().insert(box, int(i), filter, std::uint64_t(i));
            REQUIRE(result.ok());
            records.push_back({box, i, filter, std::uint64_t(i), result.handle});
        }
        std::vector<Pair> pairs;
        const auto result = made.value().forEachPair([&](const auto& a, const auto& b) noexcept
        { pairs.emplace_back(std::min(a.payload, b.payload), std::max(a.payload, b.payload)); return Continue; });
        std::sort(pairs.begin(), pairs.end());
        CHECK(result.ok() && pairs == expectedPairs(records, Boundary::Default));
        CHECK(result.counters.reports == pairs.size());
        work[layout] = result.counters;
        std::printf("filter-pruning %s: %llu nodes, %llu candidates, %llu filter rejections, %llu reports\n",
            layout == 0 ? "homogeneous" : "interleaved", static_cast<unsigned long long>(result.counters.nodeVisits),
            static_cast<unsigned long long>(result.counters.candidateTests),
            static_cast<unsigned long long>(result.counters.filterRejections), static_cast<unsigned long long>(result.counters.reports));
    }
    CHECK(work[0].candidateTests == 0 && work[1].candidateTests == 128);
    CHECK(work[0].nodeVisits != 0 && work[0].nodeVisits <= work[1].nodeVisits);
    CHECK(work[0].filterRejections != 0 && work[0].filterRejections < work[1].filterRejections);
}

void hardCapacity()
{
    TestResource resource;
    auto made = Tree<int>::create({.smallSetThreshold = 4, .hardCapacity = true}, &resource);
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    REQUIRE(tree.reserve({.entries = 256, .rankedEntries = 256, .nodes = 8193, .descriptors = 256}) == Status::Ok);
    QuadTree::Workspace<int> workspace(&resource);
    REQUIRE(workspace.reserve(256) == Status::Ok);
    std::vector<Record<int>> records;
    Rng rng{555};
    const auto allocations = resource.allocations;
    resource.failAll = true;
    for (int i = 0; i < 160; ++i)
    {
        const auto box = randomBox<int>(rng);
        auto inserted = tree.insert(box, int(i), {}, std::uint64_t(i));
        REQUIRE(inserted.ok());
        records.push_back({box, i, {}, std::uint64_t(i), inserted.handle});
    }
    for (int round = 0; round < 10; ++round)
    {
        for (auto& record : records)
        {
            record.box = randomBox<int>(rng);
            CHECK(tree.update(record.handle, record.box) == Status::Ok);
        }
        verify(tree, records, workspace);
        std::vector<QuadTree::BuildEntry<int, int>> input;
        std::vector<Tree<int>::HandleType> output(records.size());
        for (const auto& record : records) input.push_back({record.box, record.id, {}, record.rank});
        CHECK(tree.build(input, output, {.workspace = &workspace, .transactional = round % 2 == 0}) == Status::Ok);
        for (std::size_t i = 0; i < records.size(); ++i) records[i].handle = output[i];
    }
    CHECK(resource.allocations == allocations);
    CHECK(tree.shrink() == Status::AllocationFailed);
    verify(tree, records, workspace);
    tree.clear();
    CHECK(resource.allocations == allocations + 1); // Only the deliberately failed shrink.
    CHECK(tree.stats().nodeCapacity == 8193);
    resource.failAll = false;

    auto limited = Tree<int>::create({.smallSetThreshold = 0, .hardCapacity = true}, &resource);
    REQUIRE(limited.ok());
    auto small = std::move(limited).value();
    REQUIRE(small.reserve({.entries = 1, .nodes = 1, .descriptors = 1}) == Status::Ok);
    const auto old = small.insert(Box<int>{0, 0, 1, 1}, 1);
    REQUIRE(old.ok());
    std::array<QuadTree::BuildEntry<int, int>, 2> input{{{{0, 0, 1, 1}, 1}, {{5, 5, 6, 6}, 2}}};
    CHECK(small.build(input, {.workspace = &workspace}) == Status::CapacityExhausted);
    CHECK(small.valid(old.handle) && small.size() == 1);
}

void isolatedCapacity()
{
    using I = Tree<int>;
    using Entry = QuadTree::BuildEntry<int, int>;
    TestResource resource;
    auto made = I::create({.smallSetThreshold = 0, .hardCapacity = true}, &resource);
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    REQUIRE(tree.reserve({.entries = 2, .rankedEntries = 2, .nodes = 1, .descriptors = 1}) == Status::Ok);
    const std::array<Entry, 2> initial{{{{0, 0, 2, 2}, 0, {}, 0}, {{1, 1, 3, 3}, 1, {}, 1}}};
    std::array<I::HandleType, 2> handles{};
    QuadTree::Workspace<int> workspace(&resource);
    REQUIRE(workspace.reserve(2) == Status::Ok);
    auto allocations = resource.allocations;
    resource.failAll = true;
    REQUIRE(tree.build(initial, handles, {.workspace = &workspace}) == Status::Ok);
    // Build scratch was sufficient, but expanding the extent needs two mutation keys per buffer.
    const auto extent = tree.extent();
    const Box<int> outside{4, 4, 6, 6};
    CHECK(tree.update(handles[1], outside) == Status::CapacityExhausted);
    CHECK(tree.extent() == extent && tree.stats().descriptorCapacity == 1);
    verify(tree, {{initial[0].rect, 0, {}, 0, handles[0]}, {initial[1].rect, 1, {}, 1, handles[1]}}, workspace);
    CHECK(resource.allocations == allocations);
    resource.failAll = false;
    REQUIRE(tree.reserve({.descriptors = 2}) == Status::Ok);
    allocations = resource.allocations;
    resource.failAll = true;
    REQUIRE(tree.update(handles[1], outside) == Status::Ok);
    CHECK(tree.valid(handles[0]) && tree.valid(handles[1]) && tree.rect(handles[1]) == outside);
    CHECK(resource.allocations == allocations);

    // Isolate query entry, moved-handle and build capacities by undersizing just one at a time.
    std::size_t callbacks = 0;
    const auto pair = [&](const auto&, const auto&) noexcept { ++callbacks; return Continue; };
    for (bool shortEntries : {false, true})
    {
        TestResource scratchResource;
        QuadTree::Workspace<int> scratch(&scratchResource);
        REQUIRE(scratch.reserve(shortEntries ? QuadTree::WorkspaceReservation{1, 2, 2} :
            QuadTree::WorkspaceReservation{2, 1, 2}) == Status::Ok);
        const auto before = scratch.retainedBytes();
        const auto count = scratchResource.allocations;
        scratchResource.failAll = true;
        for (bool ordered : {false, true})
            CHECK(tree.forEachPairInvolving(handles, pair, {.ordered = ordered, .workspace = &scratch},
                [&](const auto&, const auto&) noexcept { ++callbacks; return true; }).status == Status::InsufficientWorkspace);
        CHECK(callbacks == 0 && scratchResource.allocations == count && scratch.retainedBytes() == before);
        scratchResource.failAll = false;
        REQUIRE(scratch.reserve(shortEntries ? QuadTree::WorkspaceReservation{2, 0, 0} :
            QuadTree::WorkspaceReservation{0, 2, 0}) == Status::Ok);
        REQUIRE(tree.forEachPairInvolving(handles, pair, {.ordered = true, .workspace = &scratch}).complete());
        CHECK(callbacks == 0); // Updated boxes are disjoint.
    }
    resource.failAll = false;
    QuadTree::Workspace<int> shortBuild(&resource);
    REQUIRE(shortBuild.reserve({2, 2, 1}) == Status::Ok);
    const auto oldHandles = handles;
    allocations = resource.allocations;
    resource.failAll = true;
    for (bool transactional : {false, true})
        CHECK(tree.build(initial, handles, {.workspace = &shortBuild, .transactional = transactional}) == Status::InsufficientWorkspace);
    CHECK(handles == oldHandles && resource.allocations == allocations);
    verify(tree, {{initial[0].rect, 0, {}, 0, handles[0]}, {outside, 1, {}, 1, handles[1]}}, workspace);
}

void referenceGrowthFailure()
{
    using I = Tree<int>;
    TestResource resource;
    auto made = I::create({.smallSetThreshold = 4}, &resource);
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    std::array<QuadTree::BuildEntry<int, int>, 5> input{};
    std::vector<Record<int>> records;
    for (std::size_t i = 0; i < input.size(); ++i)
    {
        input[i] = {{0, 0, 2, 2}, static_cast<int>(i), {}, i};
        if (i < 4)
        {
            const auto entry = tree.insert(input[i].rect, input[i].payload, {}, i);
            REQUIRE(entry.ok());
            records.push_back({input[i].rect, input[i].payload, {}, i, entry.handle});
        }
    }
    // Flat insertion grows dense/slot storage, but does not allocate references.
    REQUIRE(tree.reserve({.rankedEntries = input.size(), .nodes = 1}) == Status::Ok);
    const auto before = tree.stats();
    REQUIRE(before.entryCapacity >= input.size() && before.referenceCapacity == 0);
    QuadTree::Workspace<int> workspace(&resource);
    REQUIRE(workspace.reserve(input.size()) == Status::Ok);
    std::array<I::HandleType, 5> output;
    output.fill(records[0].handle);
    const auto sentinels = output;
    auto allocations = resource.allocations;
    resource.failAll = true;
    CHECK(tree.build(input, output, {.workspace = &workspace}) == Status::AllocationFailed);
    CHECK(resource.allocations == allocations + 1 && output == sentinels);
    CHECK(tree.stats().retainedBytes == before.retainedBytes && tree.stats().referenceCapacity == 0);
    verify(tree, records, workspace);
    resource.failAll = false;
    REQUIRE(tree.reserve({.references = input.size()}) == Status::Ok);
    CHECK(tree.stats().retainedBytes == before.retainedBytes + input.size() * before.referenceBytes);
    allocations = resource.allocations;
    resource.failAll = true;
    REQUIRE(tree.build(input, output, {.workspace = &workspace}) == Status::Ok);
    CHECK(tree.stats().references == input.size() && resource.allocations == allocations);
    for (const auto& r : records) CHECK(!tree.valid(r.handle));
    for (std::size_t i = 0; i < output.size(); ++i)
        CHECK(tree.valid(output[i]) && tree.payload(output[i]) == input[i].payload);
}

void workspaceAllocationFailures()
{
    using I = Tree<int>;
    constexpr QuadTree::WorkspaceReservation larger{11, 7, 13};
    constexpr std::array requestBytes{11 * sizeof(std::uint32_t), 11 * sizeof(std::uint32_t), 7 * sizeof(std::uint32_t),
        11 * sizeof(std::uint32_t), 11 * sizeof(std::uint8_t), 13 * sizeof(Box<int>),
        13 * sizeof(QuadTree::detail::Descriptor), 13 * sizeof(QuadTree::detail::SpatialKey),
        13 * sizeof(QuadTree::detail::SpatialKey)};
    auto made = I::create();
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    const std::array<QuadTree::BuildEntry<int, int>, 2> input{{{{0, 0, 2, 2}, 0, {}, 1}, {{1, 1, 3, 3}, 1, {}, 0}}};
    std::array<I::HandleType, 2> handles{};
    for (std::size_t i = 0; i < requestBytes.size(); ++i)
    {
        TestResource resource;
        QuadTree::Workspace<int> scratch(&resource);
        REQUIRE(scratch.reserve({2, 1, 2}) == Status::Ok);
        REQUIRE(tree.build(input, handles, {.workspace = &scratch}) == Status::Ok);
        auto checkMoved = [&]
        {
            const auto result = tree.forEachPairInvolving(std::array{handles[0]}, [&](const auto& a, const auto& b) noexcept
            { CHECK(a.handle == handles[1] && b.handle == handles[0]); return Continue; }, {.ordered = true, .workspace = &scratch});
            CHECK(result.complete() && result.counters.reports == 1);
        };
        checkMoved(); // Stamp membership before testing rollback.
        const auto retained = scratch.retainedBytes();
        const auto allocations = resource.allocations;
        const auto owned = resource.owned;
        resource.failAt = allocations + i + 1;
        CHECK(scratch.reserve(larger) == Status::AllocationFailed);
        CHECK(resource.allocations == resource.failAt && resource.failedBytes == requestBytes[i]);
        CHECK(scratch.retainedBytes() == retained && resource.bytes == retained && resource.owned == owned);
        resource.failAll = true;
        checkMoved();
        REQUIRE(tree.build(input, handles, {.workspace = &scratch}) == Status::Ok);
        CHECK(resource.allocations == resource.failAt);
        resource.failAll = false;
        resource.failAt = 0;
        const auto retry = resource.allocations;
        REQUIRE(scratch.reserve(larger) == Status::Ok);
        CHECK(resource.allocations == retry + requestBytes.size());
        CHECK(scratch.retainedBytes() == std::accumulate(requestBytes.begin(), requestBytes.end(), std::size_t{}));
        CHECK(scratch.retainedBytes() == resource.bytes);
        checkMoved();
    }
}

struct AllocationRequest
{
    const char* buffer;
    std::size_t bytes;
};

void appendStoreRequests(std::vector<AllocationRequest>& requests, std::size_t entries, std::size_t payloadBytes,
    std::size_t rankCells)
{
    // Mirror private Meta/Slot/RankCell layouts without assuming LP64 padding.
    struct Meta { Filter filter; std::uint64_t rank; std::uint32_t slot; bool hasRank; };
    struct Slot { std::uint32_t generation, index; };
    struct RankCell { std::uint64_t rank; std::uint32_t first, second; };
    requests.insert(requests.end(), {{"boxes", entries * sizeof(Box<int>)}, {"payloads", entries * payloadBytes},
        {"metadata", entries * sizeof(Meta)}, {"slots", entries * sizeof(Slot)}, {"rank cells", rankCells * sizeof(RankCell)}});
}

void appendBuildScratchRequests(std::vector<AllocationRequest>& requests, std::size_t count)
{
    requests.insert(requests.end(), {{"build boxes", count * sizeof(Box<int>)},
        {"build descriptors", count * sizeof(QuadTree::detail::Descriptor)},
        {"build keys", count * sizeof(QuadTree::detail::SpatialKey)}, {"build temp", count * sizeof(QuadTree::detail::SpatialKey)}});
}

void checkAllocationRequests(const char* operation, const std::vector<std::size_t>& actual,
    std::span<const AllocationRequest> expected)
{
    if (actual.size() != expected.size())
        std::fprintf(stderr, "%s: expected %zu requests, got %zu\n", operation, expected.size(), actual.size());
    CHECK(actual.size() == expected.size());
    for (std::size_t i = 0; i < std::min(actual.size(), expected.size()); ++i)
    {
        if (actual[i] != expected[i].bytes)
            std::fprintf(stderr, "%s allocation %zu (%s): expected %zu bytes, got %zu\n",
                operation, i + 1, expected[i].buffer, expected[i].bytes, actual[i]);
        CHECK(actual[i] == expected[i].bytes);
    }
}

void indexBufferAllocationFailures()
{
    using I = Tree<int>;
    enum Operation { Insert, Update, Reserve, Shrink, Build, TransactionalBuild };
    constexpr const char* names[]{"insert", "update", "reserve", "shrink", "build", "transactional build"};
    constexpr Box<int> local{0, 0, 4, 4}, distant{64, 64, 68, 68};
    constexpr std::size_t count = 8;
    constexpr QuadTree::Reservation reservation{.entries = 23, .rankedEntries = 23, .nodes = 19, .descriptors = 13};
    for (const auto operation : {Insert, Update, Reserve, Shrink, Build, TransactionalBuild})
    {
        for (std::size_t failed = 0;; ++failed)
        {
            TestResource resource;
            auto made = I::create({.splitThreshold = 2, .mergeThreshold = 0, .smallSetThreshold = 0, .maxDepth = 1}, &resource);
            REQUIRE(made.ok());
            auto tree = std::move(made).value();
            REQUIRE(tree.reserve(operation == Shrink ? reservation :
                QuadTree::Reservation{.entries = count, .rankedEntries = count, .nodes = 1}) == Status::Ok);
            QuadTree::Workspace<int> workspace;
            REQUIRE(workspace.reserve(count + 1) == Status::Ok);
            std::array<QuadTree::BuildEntry<int, int>, count> initial{};
            for (std::size_t i = 0; i < count; ++i) initial[i] = {local, static_cast<int>(i), {1, i % 2}, i};
            std::array<I::HandleType, count> handles{};
            REQUIRE(tree.build(initial, handles, {.workspace = &workspace}) == Status::Ok);
            std::vector<Record<int>> records;
            for (std::size_t i = 0; i < count; ++i)
                records.push_back({local, initial[i].payload, initial[i].filter, i, handles[i]});
            const auto extent = tree.extent();
            const auto before = tree.stats();
            const auto owned = resource.owned;
            REQUIRE(before.nodes == 1);

            // Only insert/update grow retained scratch while planning. Reserve/shrink/build stage
            // their buffers and must release every new allocation if preparation fails.
            const bool mutation = operation == Insert || operation == Update;
            const bool build = operation == Build || operation == TransactionalBuild;
            const auto keyBytes = sizeof(QuadTree::detail::SpatialKey);
            const auto scratchCount = count + (operation == Insert ? 1 : 0);
            std::vector<AllocationRequest> expected;
            if (mutation)
                expected = {{"mutation keys", scratchCount * keyBytes}, {"mutation temp", scratchCount * keyBytes}};
            if (operation == Shrink || build) appendBuildScratchRequests(expected, count + (build ? 1 : 0));
            expected.push_back({"nodes", before.nodeBytes * (operation == Reserve ? reservation.nodes : operation == Shrink ? 1 : 8)});
            if (operation != Update)
                expected.push_back({"references", before.referenceBytes *
                    (operation == Reserve ? reservation.entries : operation == Shrink ? count : 16)});
            if (operation == Reserve)
                expected.insert(expected.end(), {{"mutation keys", reservation.descriptors * keyBytes},
                    {"mutation temp", reservation.descriptors * keyBytes}});
            const auto storeStart = expected.size();
            if (operation != Update)
                appendStoreRequests(expected, operation == Reserve ? reservation.entries : operation == Shrink ? count :
                    operation == Insert ? 16 : count + 1, sizeof(int), operation == Reserve ? 64 : operation == Shrink ? 16 : 32);

            std::array<QuadTree::BuildEntry<int, int>, count + 1> replacement{};
            std::copy(initial.begin(), initial.end(), replacement.begin());
            replacement.back() = {distant, 8, {}, 8};
            std::array<I::HandleType, count + 2> output;
            output.fill(handles[0]);
            const auto sentinels = output;
            I::InsertResultType inserted{};
            auto run = [&]
            {
                if (operation == Insert)
                {
                    inserted = tree.insert(distant, 8, {}, 8);
                    return inserted.status;
                }
                if (operation == Update) return tree.update(handles[0], distant);
                if (operation == Reserve) return tree.reserve(reservation);
                if (operation == Shrink) return tree.shrink();
                return tree.build(replacement, output, {.transactional = operation == TransactionalBuild});
            };
            std::vector<std::size_t> requests;
            resource.requests = &requests;
            resource.failAt = resource.allocations + failed + 1;
            REQUIRE(run() == Status::AllocationFailed);
            // Once dense allocation starts, all three arrays are attempted even if one fails.
            const auto attempted = failed >= storeStart && failed < storeStart + 3 ? storeStart + 3 : failed + 1;
            checkAllocationRequests(names[operation], requests, std::span(expected).first(attempted));
            CHECK(resource.failedBytes == expected[failed].bytes);
            CHECK(output == sentinels && tree.extent() == extent);
            if (operation == Insert) CHECK(inserted.handle == I::HandleType{});
            verify(tree, records, workspace);
            const auto after = tree.stats();
            CHECK(after.nodes == before.nodes && after.references == before.references && after.liveBytes == before.liveBytes);
            CHECK(after.nodeCapacity == before.nodeCapacity && after.referenceCapacity == before.referenceCapacity &&
                after.entryCapacity == before.entryCapacity);
            const bool retainedScratch = mutation && failed >= 2;
            CHECK(after.descriptorCapacity == (retainedScratch ? scratchCount : before.descriptorCapacity));
            CHECK(resource.owned.size() == owned.size() + (retainedScratch ? 2 : 0));
            for (const auto& [ptr, bytes] : owned)
            {
                const auto kept = resource.owned.find(ptr);
                CHECK(kept != resource.owned.end() && kept->second == bytes);
            }
            for (const auto& [ptr, bytes] : resource.owned)
                if (!owned.contains(ptr)) CHECK(retainedScratch && bytes == scratchCount * keyBytes);
            CHECK(after.retainedBytes == before.retainedBytes + (retainedScratch ? 2 * scratchCount * keyBytes : 0));
            CHECK(resource.bytes == after.retainedBytes);

            resource.failAt = 0;
            requests.clear();
            REQUIRE(run() == Status::Ok);
            resource.requests = nullptr;
            checkAllocationRequests(names[operation], requests, std::span(expected).subspan(retainedScratch ? 2 : 0));
            if (operation == Insert) records.push_back({distant, 8, {}, 8, inserted.handle});
            if (operation == Update) records[0].box = distant;
            if (build)
            {
                for (const auto handle : handles) CHECK(!tree.valid(handle));
                CHECK(output.back() == sentinels.back());
                records.clear();
                for (std::size_t i = 0; i < replacement.size(); ++i)
                    records.push_back({replacement[i].rect, replacement[i].payload, replacement[i].filter, i, output[i]});
            }
            verify(tree, records, workspace);
            if (failed + 1 == expected.size()) break;
        }
    }
}

void failureInjection()
{
    for (int operation = 0; operation < 11; ++operation)
    {
        bool succeeded = false;
        for (std::size_t failure = 1; failure < 40 && !succeeded; ++failure)
        {
            TestResource resource;
            {
                auto made = Tree<int>::create({.smallSetThreshold = 0}, &resource);
                REQUIRE(made.ok());
                auto tree = std::move(made).value();
                std::vector<Record<int>> old;
                for (int i = 0; i < 8; ++i)
                {
                    const Box<int> box{i * 8, 0, i * 8 + 4, 4};
                    const auto added = tree.insert(box, int(i), {}, std::uint64_t(i));
                    REQUIRE(added.ok());
                    old.push_back({box, i, {}, std::uint64_t(i), added.handle});
                }
                QuadTree::Workspace<int> workspace;
                REQUIRE(workspace.reserve(100) == Status::Ok);
                std::vector<QuadTree::BuildEntry<int, int>> input;
                for (int i = 0; i < 50; ++i) input.push_back({{i * 2, i * 2, i * 2 + 3, i * 2 + 3}, i, {}, std::uint64_t(i)});
                std::vector<Tree<int>::HandleType> output(input.size(), old[0].handle);
                resource.failAt = resource.allocations + failure;
                Status status = Status::Ok;
                if (operation == 0) status = tree.insert(Box<int>{50000, 50000, 50003, 50003}, 9, {}, 9).status;
                if (operation == 1) status = tree.update(old[0].handle, Box<int>{-50000, -50000, -49997, -49997});
                if (operation == 2) status = tree.reserve({.entries = 200, .rankedEntries = 200, .nodes = 5000, .descriptors = 200});
                if (operation == 3) status = tree.shrink();
                if (operation == 4 || operation == 5) status = tree.build(input, output, {.transactional = operation == 5});
                if (operation == 6) status = tree.buildConsume(input, output);
                if (operation >= 7)
                {
                    std::size_t callbacks = 0;
                    const auto entry = [&](const auto&) noexcept { ++callbacks; return Continue; };
                    const auto pair = [&](const auto&, const auto&) noexcept { ++callbacks; return Continue; };
                    const QuadTree::QueryOptions<int> options{.ordered = true};
                    if (operation == 7) status = tree.forEachPairInvolving(std::array{old[0].handle}, pair, options).status;
                    if (operation == 8) status = tree.forEachPair(pair, options).status;
                    if (operation == 9) status = tree.queryPartners(old[0].handle, entry, {.ordered = true}).status;
                    if (operation == 10) status = tree.forEachPairSelected([](const auto& e) noexcept { return e.payload % 2 == 0; },
                        [](const auto& e) noexcept { return e.payload % 2 != 0; }, pair, options).status;
                    if (status != Status::Ok) CHECK(callbacks == 0);
                }
                resource.failAt = 0;
                CHECK(status == Status::Ok || status == Status::AllocationFailed);
                if (status == Status::AllocationFailed)
                {
                    verify(tree, old, workspace);
                    for (auto handle : output) CHECK(handle == old[0].handle);
                }
                else succeeded = true;
            }
        }
        CHECK(succeeded);
    }
}

void rebuildPeaks()
{
    std::array<std::size_t, 2> peaks{};
    for (bool transactional : {false, true})
    {
        TestResource resource;
        auto made = Tree<int>::create({.splitThreshold = 2, .mergeThreshold = 0, .smallSetThreshold = 0}, &resource);
        REQUIRE(made.ok());
        auto tree = std::move(made).value();
        REQUIRE(tree.insert(Box<int>{0, 0, 2, 2}, 0, {}, 0).ok());
        const auto old = resource.bytes;
        CHECK(old == tree.stats().retainedBytes);
        std::vector<QuadTree::BuildEntry<int, int>> input;
        for (int i = 0; i < 128; ++i)
            input.push_back({{i * 4, i * 4, i * 4 + 5, i * 4 + 5}, int(i), {}, std::uint64_t(i)});
        QuadTree::Workspace<int> workspace(&resource);
        REQUIRE(workspace.reserve({0, 0, input.size()}) == Status::Ok);
        const auto scratch = workspace.retainedBytes();
        resource.peak = resource.bytes;
        REQUIRE(tree.build(input, {.workspace = &workspace, .transactional = transactional}) == Status::Ok);
        const auto grown = tree.stats().retainedBytes;
        CHECK(resource.bytes == grown + scratch);
        // Every entry/node/reference buffer grows here. All new buffers coexist
        // with the old storage and captured workspace before publication.
        CHECK(resource.peak <= old + grown + scratch && resource.peak > grown + scratch);
        peaks[transactional] = resource.peak;
        std::printf("rebuild-peak %s: old %zu, scratch %zu, retained %zu, peak %zu bytes\n",
            transactional ? "transactional" : "default", old, scratch, grown, resource.peak);
        const auto allocations = resource.allocations;
        resource.failAll = true;
        resource.peak = resource.bytes;
        REQUIRE(tree.build(input, {.workspace = &workspace, .transactional = transactional}) == Status::Ok);
        CHECK(resource.allocations == allocations && resource.peak == grown + scratch);
    }
    CHECK(peaks[0] == peaks[1]);
}

void preparedSmallBuild()
{
    TestResource resource;
    auto made = Tree<int>::create({.extent = QuadTree::Extent::Snapshot,
        .snapshotGrowth = QuadTree::SnapshotGrowth::Adaptive, .smallSetThreshold = 2,
        .maxEntries = 3, .hardCapacity = true}, &resource);
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    REQUIRE(tree.reserve({.entries = 3, .rankedEntries = 3, .nodes = 1}) == Status::Ok);
    QuadTree::Workspace<int> scratch(&resource);
    REQUIRE(scratch.reserve({0, 0, 3}) == Status::Ok);
    const auto allocations = resource.allocations;
    resource.failAll = true;

    using E = QuadTree::BuildEntry<int, int>;
    std::array<E, 3> entries{{{{0, 0, 3, 3}, 1, {}, 7}, {{1, 1, 2, 2}, 2, {2, 3}, 8},
        {{9, 9, 10, 10}, 3, {}, 9}}};
    std::array<Tree<int>::HandleType, 4> output{};
    REQUIRE(tree.build(entries, output, {.workspace = &scratch}) == Status::Ok);
    const auto previous = output;
    CHECK(tree.payload(output[0]) == 1 && tree.payload(output[1]) == 2 && tree.payload(output[2]) == 3);
    CHECK(output[3] == Tree<int>::HandleType{});
    CHECK(tree.stats().references == 3);

    std::forward_list<E> replacement{entries[1]};
    REQUIRE(tree.build(replacement, output, {.workspace = &scratch, .transactional = true}) == Status::Ok);
    CHECK(!tree.valid(previous[0]) && !tree.valid(previous[1]) && !tree.valid(previous[2]));
    CHECK(tree.valid(output[0]) && tree.payload(output[0]) == 2 && output[1] == previous[1]);
    CHECK(tree.rank(output[0]) == 8 && tree.filter(output[0]) == entries[1].filter);
    CHECK(tree.extent() == entries[1].rect && tree.stats().references == 0);
    const auto ranked = output[0];
    replacement.front().rank.reset();
    REQUIRE(tree.build(replacement, output, {.workspace = &scratch}) == Status::Ok);
    CHECK(!tree.valid(ranked) && !tree.rank(output[0]));
    const auto noPair = [](const auto&, const auto&) noexcept { CHECK(false); return Continue; };
    CHECK(tree.forEachPair(noPair, {.ordered = true}).status == Status::MissingRank);
    CHECK(tree.forEachPair(noPair, {.budget = {.candidateTests = 0}}).complete());

    entries[0].rank.reset();
    replacement.push_front(entries[0]);
    const auto single = output[0];
    REQUIRE(tree.build(replacement, output, {.workspace = &scratch}) == Status::Ok);
    CHECK(!tree.valid(single) && tree.size() == 2);
    CHECK(tree.payload(output[0]) == 1 && tree.payload(output[1]) == 2 && output[2] == previous[2]);
    int pairs = 0;
    CHECK(tree.forEachPair([&](const auto& a, const auto& b) noexcept
        {
            CHECK(a.handle == output[0] && b.handle == output[1]);
            ++pairs;
            return Continue;
        }).complete());
    CHECK(pairs == 1);
    replacement.push_front(entries[0]);
    replacement.push_front(entries[0]);
    CHECK(tree.build(replacement, output, {.workspace = &scratch}) == Status::SizeLimitExceeded);
    CHECK(tree.valid(output[0]) && tree.size() == 2);
    replacement.clear();
    REQUIRE(tree.build(replacement, output, {.workspace = &scratch}) == Status::Ok);
    CHECK(!tree.valid(output[0]) && !tree.valid(output[1]) && tree.empty() && !tree.extent());
    CHECK(tree.forEachPair(noPair, {.ordered = true}).complete());
    CHECK(tree.build(replacement, {.workspace = &scratch}) == Status::Ok);

    // The contiguous integer path must validate every box before replacing
    // anything, and must defer ranked input to the general rank checks.
    entries[1].rank.reset();
    auto flat = std::span(entries).first(2);
    REQUIRE(tree.build(flat, output, {.workspace = &scratch}) == Status::Ok);
    const auto flatHandles = output;
    CHECK(tree.extent() == entries[0].rect && tree.filter(output[1]) == entries[1].filter);
    const auto box = entries[1].rect;
    entries[1].rect = {2, 3, 1, 4};
    CHECK(tree.build(flat, output, {.workspace = &scratch}) == Status::InvalidGeometry);
    CHECK(output == flatHandles && tree.valid(output[0]) && tree.rect(output[1]) == box);
    entries[1].rect = box;
    entries[0].rank = entries[1].rank = 6;
    CHECK(tree.build(flat, output, {.workspace = &scratch}) == Status::DuplicateRank);
    CHECK(output == flatHandles && tree.valid(output[1]) && !tree.rank(output[0]));
    entries[1].rank = 7;
    REQUIRE(tree.build(flat, output, {.workspace = &scratch}) == Status::Ok);
    CHECK(!tree.valid(flatHandles[0]) && !tree.valid(flatHandles[1]));
    CHECK(tree.rank(output[0]) == 6 && tree.rank(output[1]) == 7);

    REQUIRE(tree.build(entries, output, {.workspace = &scratch}) == Status::Ok);
    const auto topologyHandles = output;
    entries[0].rank.reset();
    const auto singleton = flat.first(1);
    entries[0].rect = {3, 0, 2, 3};
    CHECK(tree.build(singleton, output, {.workspace = &scratch}) == Status::InvalidGeometry);
    CHECK(output == topologyHandles && tree.valid(output[0]) && tree.stats().references == 3);
    entries[0].rect = {0, 0, 3, 3};
    REQUIRE(tree.build(singleton, output, {.workspace = &scratch}) == Status::Ok);
    CHECK(tree.extent() == entries[0].rect && tree.stats().references == 0);
    CHECK(tree.payload(output[0]) == 1 && !tree.rank(output[0]) && output[1] == topologyHandles[1]);
    for (std::size_t i = 0; i < 3; ++i)
        CHECK(!tree.valid(topologyHandles[i]));
    CHECK(tree.insert(entries[1].rect, 2, entries[1].filter, 7).ok());
    CHECK(resource.allocations == allocations);
}

template<typename T>
void preparedFloatBoxes()
{
    auto made = Tree<T>::create();
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    REQUIRE(tree.reserve({.entries = 3}) == Status::Ok);
    QuadTree::Workspace<T> scratch;
    REQUIRE(scratch.reserve({0, 0, 3}) == Status::Ok);
    const T zero = -T(0), limit = std::numeric_limits<T>::max();
    std::array<QuadTree::BuildEntry<int, T>, 3> entries{{{{zero, zero, zero, zero}, 0},
        {{-limit, -limit, limit, limit}, 1}, {{1, 2, 3, 4}, 2}}};
    std::array<typename Tree<T>::HandleType, 3> handles{};
    for (std::size_t n : {1u, 2u, 3u})
    {
        auto input = std::span(entries).first(n);
        REQUIRE(tree.build(input, handles, {.workspace = &scratch}) == Status::Ok);
        const auto stored = tree.rect(handles[0]);
        CHECK(!std::signbit(stored.minX) && !std::signbit(stored.maxX) &&
            !std::signbit(stored.minY) && !std::signbit(stored.maxY));
        const auto extent = tree.extent();
        CHECK(extent == (n == 1 ? stored : entries[1].rect));
        const auto committed = handles;
        auto& last = input.back().rect;
        const auto original = last;
        const auto reject = [&]
        {
            CHECK(tree.build(input, handles, {.workspace = &scratch}) == Status::InvalidGeometry);
            CHECK(handles == committed && tree.valid(handles[n - 1]) && tree.extent() == extent);
            last = original;
        };
        last = {1, 0, 0, 0};
        reject();
        last = {0, 1, 0, 0};
        reject();
        for (auto field : {&Box<T>::minX, &Box<T>::minY, &Box<T>::maxX, &Box<T>::maxY})
            for (T bad : {std::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::infinity(),
                -std::numeric_limits<T>::infinity()})
            {
                last.*field = bad;
                reject();
            }
    }
}

void tinyPairs()
{
    for (const auto boundary : {Boundary::Default, Boundary::TouchInclusive})
    {
        auto made = Tree<int>::create({.boundary = boundary});
        REQUIRE(made.ok());
        auto tree = std::move(made).value();
        using E = QuadTree::BuildEntry<int, int>;
        std::array<E, 4> input{{{{0, 0, 4, 4}, 0}, {{4, 0, 8, 4}, 1, {3, 3}, 7},
            {{4, 2, 4, 2}, 2}, {{4, 2, 4, 2}, 3, {0, 0}}}};
        std::array<Tree<int>::HandleType, 4> handles{};
        REQUIRE(tree.build(input, handles) == Status::Ok);
        REQUIRE(tree.remove(handles[0]) == Status::Ok);
        // The final dense entry reuses the first slot: callbacks must still be slot-oriented.
        const auto inserted = tree.insert(input[0].rect, 0);
        REQUIRE(inserted.ok());
        handles[0] = inserted.handle;
        const auto checkView = [&](const auto& view) noexcept
        {
            const auto k = static_cast<std::size_t>(view.payload);
            CHECK(k < input.size());
            if (k < input.size())
            {
                CHECK(view.handle == handles[k] && view.rect == input[k].rect);
                CHECK(view.filter == input[k].filter && view.rank == input[k].rank);
            }
        };
        const bool touch = boundary == Boundary::TouchInclusive;
        for (std::uint64_t budget : {0u, 2u, 3u, 6u})
        {
            std::uint64_t reports = 0;
            const auto result = tree.forEachPair([&](const auto& a, const auto& b) noexcept
            {
                checkView(a);
                checkView(b);
                CHECK((touch && a.payload == 0 && b.payload == 1) || (a.payload == 1 && b.payload == 2));
                ++reports;
                return Continue;
            }, {.budget = {.candidateTests = budget}}, [&](const auto& a, const auto& b) noexcept
            {
                checkView(a);
                checkView(b);
                CHECK(a.payload < b.payload);
                return a.payload != 0 || b.payload != 2;
            });
            CHECK(result.status == (budget < 3 ? Status::BudgetExhausted : Status::Ok));
            CHECK(result.counters.candidateTests == std::min(budget, std::uint64_t{3}));
            if (budget >= 3)
            {
                CHECK(result.counters.filterRejections == 3 && result.counters.predicateCalls == (touch ? 3u : 2u));
                CHECK(reports == (touch ? 2u : 1u));
            }
            CHECK(reports == result.counters.reports);
        }
        const auto stop = [](const auto&, const auto&) noexcept { return Stop; };
        const auto stopped = tree.forEachPair(stop, {.budget = {.candidateTests = 6}});
        CHECK(stopped.status == Status::Stopped && stopped.counters.reports == 1);
        REQUIRE(tree.build(std::span(input).first(2)) == Status::Ok);
        const auto contact = tree.forEachPair(stop, {.budget = {.candidateTests = 1}});
        CHECK(contact.status == (touch ? Status::Stopped : Status::Ok));
        CHECK(contact.counters.candidateTests == 1 && contact.counters.reports == (touch ? 1u : 0u));

        REQUIRE(tree.build(std::span(input).first(3), handles) == Status::Ok);
        REQUIRE(tree.remove(handles[0]) == Status::Ok);
        CHECK(tree.forEachPair(stop, {.budget = {.candidateTests = 0}}).status == Status::BudgetExhausted);
        const auto checkPair = [&](const auto& a, const auto& b) noexcept
        {
            checkView(a);
            checkView(b);
            CHECK(a.payload == 1 && b.payload == 2);
        };
        for (bool accept : {false, true})
        {
            const auto result = tree.forEachPair([&](const auto& a, const auto& b) noexcept
            {
                checkPair(a, b);
                return Stop;
            }, {.budget = {.candidateTests = 1}}, [&](const auto& a, const auto& b) noexcept
            {
                checkPair(a, b);
                return accept;
            });
            CHECK(result.status == (accept ? Status::Stopped : Status::Ok));
            CHECK(result.counters.candidateTests == 1 && result.counters.predicateCalls == 1);
            CHECK(result.counters.reports == (accept ? 1u : 0u));
        }
        REQUIRE(tree.build(std::span(input).last(2)) == Status::Ok);
        for (std::uint64_t budget : {0u, 1u})
        {
            const auto filtered = tree.forEachPair(stop, {.budget = {.candidateTests = budget}});
            CHECK(filtered.complete() && filtered.counters.filterRejections == 1 && filtered.counters.candidateTests == 0);
        }
    }
}

struct LimitedIdentities
{
    static constexpr bool checkHandles = true;
    static inline bool exhausted = false;
    static std::uint64_t acquireIdentity() noexcept
    {
        return exhausted ? 0 : QuadTree::detail::acquireModuleIdentity();
    }
};

void identities()
{
    using I = QuadTree::Index<int, int, LimitedIdentities>;
    auto made = I::create();
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    const auto old = tree.insert(Box<int>{0, 0, 1, 1}, 1);
    REQUIRE(old.ok());
    std::array<QuadTree::BuildEntry<int, int>, 1> input{{{{0, 0, 2, 2}, 2}}};
    std::array<I::HandleType, 1> output{old.handle};
    LimitedIdentities::exhausted = true;
    CHECK(tree.build(input, output) == Status::IdentityExhausted);
    CHECK(tree.valid(old.handle) && output[0] == old.handle && tree.payload(old.handle) == 1);
    REQUIRE(tree.reserve({.entries = 1}) == Status::Ok);
    QuadTree::Workspace<int> scratch;
    REQUIRE(scratch.reserve({0, 0, 1}) == Status::Ok);
    CHECK(tree.build(input, output, {.workspace = &scratch}) == Status::IdentityExhausted);
    CHECK(tree.valid(old.handle) && output[0] == old.handle && tree.payload(old.handle) == 1);
    tree.clear();
    CHECK(tree.insert(Box<int>{0, 0, 1, 1}, 1).status == Status::IdentityExhausted);
    LimitedIdentities::exhausted = false;
    auto first = MultiTuIndex::create(), second = MultiTuIndex::create();
    REQUIRE(first.ok() && second.ok());
    const auto a = indexTu2Insert(first.value());
    const auto b = indexTu2Insert(second.value());
    CHECK(indexTu2Valid(first.value(), a) && !indexTu2Valid(first.value(), b));
    CHECK(second.value().valid(b) && !second.value().valid(a));
}

// Run last: the module counter must never be reset after exhaustion.
void flatIdentityExhaustion()
{
    using I = QuadTree::Index<int, int>;
    auto made = I::create();
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    REQUIRE(tree.reserve({.entries = 2}) == Status::Ok);
    QuadTree::Workspace<int> scratch;
    REQUIRE(scratch.reserve({0, 0, 2}) == Status::Ok);
    const auto old = tree.insert(Box<int>{0, 0, 1, 1}, 1);
    REQUIRE(old.ok());
    std::array<QuadTree::BuildEntry<int, int>, 2> input{{{{1, 0, 2, 1}, 2}, {{0, 0, 2, 1}, 3}}};
    std::array<I::HandleType, 3> output{old.handle, old.handle, old.handle};
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    auto& counter = QuadTree::detail::moduleIdentityCounter;
    counter.store(maximum - 1, std::memory_order_relaxed);

    input[1].rect.maxX = -1;
    CHECK(tree.build(input, output, {.workspace = &scratch}) == Status::InvalidGeometry);
    CHECK(counter.load(std::memory_order_relaxed) == maximum - 1);
    CHECK(tree.valid(old.handle) && output[0] == old.handle && output[1] == old.handle);
    input[1].rect.maxX = 2;
    REQUIRE(tree.build(input, output, {.workspace = &scratch}) == Status::Ok);
    CHECK(counter.load(std::memory_order_relaxed) == maximum && !tree.valid(old.handle));
    REQUIRE(tree.valid(output[0]) && tree.valid(output[1]));
    CHECK(tree.payload(output[0]) == 2 && tree.payload(output[1]) == 3 && output[2] == old.handle);

    const auto committed = output;
    const auto extent = tree.extent();
    input[1].rect.maxX = 3;
    CHECK(tree.build(input, output, {.workspace = &scratch}) == Status::IdentityExhausted);
    CHECK(output == committed && tree.valid(committed[0]) && tree.valid(committed[1]));
    CHECK(tree.extent() == extent && counter.load(std::memory_order_relaxed) == maximum);
}

struct MoveOnly
{
    std::unique_ptr<int> value;
    explicit MoveOnly(int i): value(std::make_unique<int>(i)) {}
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;
};

void payloadsAndMoves()
{
    TestResource first, second;
    auto a = QuadTree::Index<MoveOnly, int>::create({.smallSetThreshold = 0}, &first);
    auto b = QuadTree::Index<MoveOnly, int>::create({}, &second);
    REQUIRE(a.ok() && b.ok());
    auto tree = std::move(a).value();
    auto other = std::move(b).value();
    std::array<QuadTree::BuildEntry<MoveOnly, int>, 2> input{{{{0, 0, 3, 3}, MoveOnly(1)}, {{4, 4, 2, 2}, MoveOnly(2)}}};
    REQUIRE(tree.insert(Box<int>{0, 0, 1, 1}, MoveOnly(5)).ok());
    CHECK(tree.buildConsume(input) == Status::InvalidGeometry);
    CHECK(input[0].payload.value && input[1].payload.value);
    input[1].rect = {1, 1, 2, 2};
    std::array<decltype(tree)::HandleType, 2> handles;
    CHECK(tree.buildConsume(input, handles, {.transactional = true}) == Status::Ok);
    CHECK(!input[0].payload.value && !input[1].payload.value);
    CHECK(*tree.payload(handles[0]).value == 1 && *tree.payload(handles[1]).value == 2);
    const auto stale = other.insert(Box<int>{0, 0, 1, 1}, MoveOnly(3));
    REQUIRE(stale.ok());
    const auto allocations = first.allocations + second.allocations;
    other = std::move(tree);
    CHECK(other.resource() == &first && tree.resource() == &first);
    CHECK(other.valid(handles[0]) && !tree.valid(handles[0]) && !other.valid(stale.handle));
    CHECK(first.allocations + second.allocations == allocations);
    CHECK(tree.empty());
    CHECK(tree.query(Box<int>{0, 0, 10, 10}, [](const auto&) noexcept { return Stop; }).complete());
    REQUIRE(tree.insert(Box<int>{0, 0, 1, 1}, MoveOnly(6)).ok());
    auto& alias = other;
    other = std::move(alias);
    CHECK(other.valid(handles[0]));
    QuadTree::Workspace<int> wa(&first), wb(&second);
    REQUIRE(wa.reserve(50) == Status::Ok && wb.reserve(20) == Status::Ok);
    const auto work_allocations = first.allocations + second.allocations;
    wb = std::move(wa);
    CHECK(wb.resource() == &first && wa.resource() == &first);
    CHECK(wa.retainedBytes() == 0);
    CHECK(first.allocations + second.allocations == work_allocations);
    REQUIRE(wa.reserve(3) == Status::Ok);

    auto projected = Tree<int>::create();
    REQUIRE(projected.ok());
    std::array<int, 3> payloads{1, 2, 3};
    int calls = 0, offset = 7;
    auto accessor = [&](const int& value) noexcept { ++calls; return Rect<int>{value + offset, 0, 2, 2}; };
    CHECK(projected.value().buildWith(payloads, accessor) == Status::Ok);
    CHECK(calls == 3);
    auto inserted = projected.value().insertWith(4, accessor);
    REQUIRE(inserted.ok());
    CHECK(calls == 4 && projected.value().rect(inserted.handle).minX == 11);
    offset = 100;
    CHECK(projected.value().rect(inserted.handle).minX == 11);
}

void stagedSelfRebuild()
{
    using I = Tree<int>;
    using Entry = QuadTree::BuildEntry<int, int>;
    TestResource resource;
    auto made = I::create({.smallSetThreshold = 0}, &resource);
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    REQUIRE(tree.reserve({.entries = 2, .rankedEntries = 2, .nodes = 1, .descriptors = 2}) == Status::Ok);
    Rect<int> rect{1, 2, 3, 4};
    Box<int> box{2, 3, 5, 7};
    constexpr Box<int> originalRect{1, 2, 4, 6}, originalBox{2, 3, 5, 7};
    const auto a = tree.insert(rect, 10, {1, 3}, 20);
    const auto b = tree.insert(box, 11, {2, 3}, 10);
    REQUIRE(a.ok() && b.ok());
    rect = {100, 100, 1, 1};
    box = {200, 200, 201, 201};
    CHECK(tree.rect(a.handle) == originalRect && tree.rect(b.handle) == originalBox);
    REQUIRE(tree.query(box, [](const auto&) noexcept { CHECK(false); return Continue; }).complete());

    QuadTree::Workspace<int> workspace(&resource);
    REQUIRE(workspace.reserve(2) == Status::Ok);
    const auto allocations = resource.allocations;
    resource.failAll = true;
    std::array old{a.handle, b.handle};
    for (bool transactional : {false, true})
    {
        // Copy values out before replacement, without keeping references to entry views.
        std::array<Entry, 2> input{};
        std::size_t used = 0;
        REQUIRE(tree.forEachEntry([&](const auto& entry) noexcept
        {
            REQUIRE(used < input.size());
            input[used++] = {entry.rect, entry.payload, entry.filter, entry.rank};
            return Continue;
        }).complete());
        REQUIRE(used == input.size());
        if (input[0].rank == 20) std::swap(input[0], input[1]);
        std::array<I::HandleType, 3> output{old[0], old[1], old[0]};
        REQUIRE(tree.build(input, output, {.workspace = &workspace, .transactional = transactional}) == Status::Ok);
        CHECK(!tree.valid(old[0]) && !tree.valid(old[1]) && output[2] == old[0]);
        for (std::size_t i = 0; i < input.size(); ++i)
        {
            REQUIRE(tree.valid(output[i]));
            CHECK(tree.rect(output[i]) == input[i].rect && tree.payload(output[i]) == input[i].payload);
            CHECK(tree.filter(output[i]) == input[i].filter && tree.rank(output[i]) == input[i].rank);
            input[i].rect = box;
        }
        CHECK(tree.rect(output[0]) == originalBox && tree.rect(output[1]) == originalRect);
        std::size_t pairs = 0;
        REQUIRE(tree.forEachPair([&](const auto& x, const auto& y) noexcept
        { CHECK(x.handle == output[0] && y.handle == output[1]); ++pairs; return Continue; },
            {.ordered = true, .workspace = &workspace}).complete());
        CHECK(pairs == 1);
        old = {output[0], output[1]};
    }
    CHECK(resource.allocations == allocations);
}

void moveOnlyBuildAllocationFailures()
{
    using I = QuadTree::Index<MoveOnly, int, QuadTree::Checked>;
    constexpr Box<int> oldBox{0, 0, 4, 4};
    constexpr std::size_t count = 9;
    for (bool transactional : {false, true})
    {
        TestResource resource;
        auto made = I::create({.splitThreshold = 1, .mergeThreshold = 0, .smallSetThreshold = 0, .maxDepth = 1}, &resource);
        REQUIRE(made.ok());
        auto tree = std::move(made).value();
        REQUIRE(tree.reserve({.entries = 2, .rankedEntries = 2, .nodes = 1, .descriptors = 2}) == Status::Ok);
        const auto a = tree.insert(oldBox, MoveOnly(80), {}, 80);
        const auto b = tree.insert(oldBox, MoveOnly(90), {}, 90);
        REQUIRE(a.ok() && b.ok());
        const auto extent = tree.extent();
        const auto stats = tree.stats();
        const auto owned = resource.owned;
        const auto aValue = tree.payload(a.handle).value.get(), bValue = tree.payload(b.handle).value.get();
        std::vector<QuadTree::BuildEntry<MoveOnly, int>> input;
        input.reserve(count);
        std::array<const int*, count> values{};
        for (std::size_t i = 0; i < count; ++i)
        {
            const int p = 16 + static_cast<int>(i) * 8;
            input.push_back({{p, p, p + 5, p + 5}, MoveOnly(static_cast<int>(i)), {}, i});
            values[i] = input.back().payload.value.get();
        }
        std::array<I::HandleType, count + 1> output;
        output.fill(a.handle);
        const auto sentinel = output;
        std::vector<AllocationRequest> expected;
        appendBuildScratchRequests(expected, count);
        expected.push_back({"nodes", 8 * stats.nodeBytes});
        expected.push_back({"references", count * stats.referenceBytes});
        const auto storeStart = expected.size();
        appendStoreRequests(expected, count, sizeof(MoveOnly), 32);
        std::vector<std::size_t> requests;
        resource.requests = &requests;
        const auto* name = transactional ? "transactional buildConsume" : "buildConsume";
        for (std::size_t failed = 0; failed < expected.size(); ++failed)
        {
            requests.clear();
            resource.failAt = resource.allocations + failed + 1;
            REQUIRE(tree.buildConsume(input, output, {.transactional = transactional}) == Status::AllocationFailed);
            const auto attempted = failed >= storeStart && failed < storeStart + 3 ? storeStart + 3 : failed + 1;
            checkAllocationRequests(name, requests, std::span(expected).first(attempted));
            CHECK(resource.failedBytes == expected[failed].bytes && output == sentinel && resource.owned == owned);
            CHECK(tree.extent() == extent && tree.size() == 2 && tree.stats().retainedBytes == stats.retainedBytes);
            REQUIRE(tree.valid(a.handle) && tree.valid(b.handle));
            CHECK(tree.rect(a.handle) == oldBox && tree.rect(b.handle) == oldBox);
            CHECK(tree.payload(a.handle).value.get() == aValue && tree.payload(b.handle).value.get() == bValue);
            CHECK(tree.rank(a.handle) == 80 && tree.rank(b.handle) == 90);
            for (std::size_t i = 0; i < count; ++i)
                REQUIRE(input[i].payload.value.get() == values[i] && *values[i] == static_cast<int>(i));
            std::size_t pairs = 0;
            REQUIRE(tree.forEachPair([&](const auto& x, const auto& y) noexcept
            { CHECK(x.handle == a.handle && y.handle == b.handle); ++pairs; return Continue; }).complete());
            CHECK(pairs == 1);
        }
        requests.clear();
        resource.failAt = 0;
        REQUIRE(tree.buildConsume(input, output, {.transactional = transactional}) == Status::Ok);
        resource.requests = nullptr;
        checkAllocationRequests(name, requests, expected);
        CHECK(!tree.valid(a.handle) && !tree.valid(b.handle) && output.back() == sentinel.back());
        CHECK(tree.size() == count && tree.stats().nodes > stats.nodes);
        for (std::size_t i = 0; i < count; ++i)
        {
            REQUIRE(!input[i].payload.value && tree.valid(output[i]));
            CHECK(tree.payload(output[i]).value.get() == values[i]);
            CHECK(tree.rect(output[i]) == input[i].rect && tree.rank(output[i]) == i);
        }
    }
}

void cacheAndChurn()
{
    auto made = Tree<int>::create({.splitThreshold = 2, .mergeThreshold = 0, .smallSetThreshold = 0});
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    std::array<Tree<int>::HandleType, 3> handles;
    for (int i = 0; i < 3; ++i)
    {
        auto added = tree.insert(Box<int>{0, 0, 3, 3}, int(i));
        REQUIRE(added.ok()); handles[static_cast<std::size_t>(i)] = added.handle;
    }
    std::set<Pair> cache;
    auto predicate = [](const auto& a, const auto& b) noexcept { return a.payload >= 0 && b.payload >= 0; };
    const auto id = [&](auto handle) noexcept
    {
        const auto it = std::find(handles.begin(), handles.end(), handle);
        REQUIRE(it != handles.end());
        return static_cast<int>(it - handles.begin());
    };
    const auto key = [&](const auto& a, const auto& b) noexcept
    {
        const int x = id(a.handle), y = id(b.handle);
        return Pair{std::min(x, y), std::max(x, y)};
    };
    auto collect = [&](const auto& a, const auto& b) noexcept { cache.insert(key(a, b)); return Continue; };
    REQUIRE(tree.forEachPair(collect, {}, predicate).complete());
    QuadTree::Workspace<int> workspace;
    REQUIRE(workspace.reserve(10) == Status::Ok);
    const auto checkCache = [&]
    {
        std::set<Pair> full;
        REQUIRE(tree.forEachPair([&](const auto& a, const auto& b) noexcept
            { full.insert(key(a, b)); return Continue; }, {}, predicate).complete());
        CHECK(cache == full);
    };
    const auto refresh = [&](int dirty, bool stop = false)
    {
        std::set<Pair> delta;
        const auto result = tree.forEachPairInvolving(std::span(handles).subspan(static_cast<std::size_t>(dirty), 1),
            [&](const auto& a, const auto& b) noexcept
            { delta.insert(key(a, b)); return stop ? Stop : Continue; }, {.workspace = &workspace}, predicate);
        if (stop)
        {
            CHECK(result.status == Status::Stopped && delta.size() == 1);
            return; // Discard partial output rather than replacing the cache.
        }
        REQUIRE(result.complete());
        for (auto it = cache.begin(); it != cache.end();)
            if (it->first == dirty || it->second == dirty) it = cache.erase(it);
            else ++it;
        cache.insert(delta.begin(), delta.end());
        checkCache();
    };
    refresh(2, true);
    CHECK(cache.size() == 3);
    refresh(2);
    CHECK(tree.update(handles[0], Box<int>{4, 4, 3, 3}) == Status::InvalidGeometry);
    checkCache();
    CHECK(tree.update(handles[0], Box<int>{10, 10, 13, 13}) == Status::Ok); refresh(0);
    CHECK(tree.setFilter(handles[1], {0, 0}) == Status::Ok); refresh(1);
    CHECK(tree.setFilter(handles[1], {}) == Status::Ok); refresh(1);
    tree.payload(handles[2]) = -2; refresh(2);
    tree.payload(handles[2]) = 2; refresh(2);
    CHECK(tree.remove(handles[1]) == Status::Ok);
    for (auto it = cache.begin(); it != cache.end();)
        if (it->first == 1 || it->second == 1) it = cache.erase(it);
        else ++it;
    CHECK(cache.empty() && !tree.valid(handles[1]));
    checkCache();
    // Dense swap removal, pooling, split/merge, adaptive growth and contraction.
    for (int round = 0; round < 80; ++round)
    {
        std::vector<Tree<int>::HandleType> spawned;
        for (int i = 0; i < 80; ++i)
        {
            const int x = (round % 2 == 0 ? 1000000 : -1000000) + i * 8;
            const auto added = tree.insert(Box<int>{x, i, x + 4, i + 4}, int(i + 10));
            REQUIRE(added.ok()); spawned.push_back(added.handle);
        }
        for (auto handle : spawned) CHECK(tree.remove(handle) == Status::Ok);
        CHECK(tree.size() == 2);
        CHECK(tree.stats().nodes < 200);
        if (round % 9 == 0) CHECK(tree.shrink() == Status::Ok);
    }
}

void movedPredicateCache()
{
    using I = Tree<int>;
    TestResource resource;
    auto made = I::create({.extent = QuadTree::Extent::Fixed, .root = Box<int>{0, 0, 64, 64},
        .splitThreshold = 2, .mergeThreshold = 0, .smallSetThreshold = 0}, &resource);
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    std::array<I::HandleType, 6> handles{};
    for (std::size_t i : {3u, 1u, 5u, 0u, 4u, 2u})
    {
        const int x = i < 3 ? 8 : 40;
        const auto added = tree.insert(Box<int>{x, x, x + 4, x + 4}, 1, {}, i);
        REQUIRE(added.ok());
        handles[i] = added.handle;
    }
    REQUIRE(tree.stats().nodes > 1);
    const std::array all{handles[5], handles[4], handles[3], handles[2], handles[1], handles[0]};
    const std::array dirty{handles[4], handles[1]};
    const auto id = [&](I::HandleType handle) noexcept
    {
        const auto it = std::find(handles.begin(), handles.end(), handle);
        REQUIRE(it != handles.end());
        return static_cast<int>(it - handles.begin());
    };
    const auto incident = [](Pair pair) noexcept
        { return pair.first == 1 || pair.second == 1 || pair.first == 4 || pair.second == 4; };
    const std::vector<Pair> allPairs{{0, 1}, {0, 2}, {1, 2}, {3, 4}, {3, 5}, {4, 5}};
    QuadTree::Workspace<int> workspace(&resource);
    REQUIRE(workspace.reserve(handles.size()) == Status::Ok);
    const auto allocations = resource.allocations, bytes = workspace.retainedBytes();
    resource.failAll = true;
    for (bool ordered : {false, true})
    {
        std::array<bool, 6> eligible{true, true, true, true, true, true};
        const auto predicate = [&](const auto& a, const auto& b) noexcept
            { return eligible[static_cast<std::size_t>(id(a.handle))] && eligible[static_cast<std::size_t>(id(b.handle))]; };
        auto collect = [&](bool full, std::span<const I::HandleType> moved)
        {
            std::vector<Pair> pairs;
            pairs.reserve(allPairs.size());
            const auto visitor = [&](const auto& a, const auto& b) noexcept
            {
                REQUIRE(pairs.size() < allPairs.size());
                const int x = id(a.handle), y = id(b.handle);
                pairs.push_back(ordered ? Pair{x, y} : Pair{std::min(x, y), std::max(x, y)});
                return Continue;
            };
            const QuadTree::QueryOptions<int> options{.ordered = ordered, .workspace = &workspace};
            const auto result = full ? tree.forEachPair(visitor, options, predicate) :
                tree.forEachPairInvolving(moved, visitor, options, predicate);
            CHECK(result.complete() && result.counters.reports == pairs.size());
            if (!ordered) std::sort(pairs.begin(), pairs.end());
            return pairs;
        };
        auto cache = collect(true, {});
        CHECK(cache == allPairs);
        CHECK(collect(false, all) == allPairs);
        for (bool enabled : {false, true})
        {
            // No Index mutation: eligibility changes only in client state.
            eligible[1] = eligible[4] = enabled;
            auto expected = allPairs;
            if (!enabled) std::erase_if(expected, incident);
            const auto delta = collect(false, dirty);
            auto expectedDelta = expected;
            std::erase_if(expectedDelta, [&](Pair pair) { return !incident(pair); });
            CHECK(delta == expectedDelta);
            std::erase_if(cache, incident);
            cache.insert(cache.end(), delta.begin(), delta.end());
            std::sort(cache.begin(), cache.end());
            CHECK(cache == expected);
            CHECK(collect(true, {}) == expected);
            CHECK(collect(false, all) == expected);
        }
    }
    CHECK(resource.allocations == allocations && workspace.retainedBytes() == bytes);
}

struct MutationProbe : QuadTree::Checked
{
    static inline std::size_t splitAttempts = 0, splitEntries = 0, refitEntries = 0;
    static void onSplitPreparation(std::size_t entries) noexcept { ++splitAttempts; splitEntries += entries; }
    static void onItemRefit(std::size_t entries) noexcept { refitEntries += entries; }
    static void reset() noexcept { splitAttempts = 0; splitEntries = 0; refitEntries = 0; }
};

void mutationWork()
{
    using I = QuadTree::Index<int, int, MutationProbe>;
    constexpr int count = 4096;
    for (bool oversized : {false, true})
    {
        TestResource resource;
        auto made = I::create({.extent = QuadTree::Extent::Fixed, .root = Box<int>{0, 0, 16384, 16384},
            .smallSetThreshold = 0}, &resource);
        REQUIRE(made.ok());
        auto tree = std::move(made).value();
        const Box<int> original = oversized ? Box<int>{0, 0, 16384, 16384} : Box<int>{16, 16, 24, 24};
        const Box<int> moved = oversized ? Box<int>{1, 0, 16384, 16384} : Box<int>{17, 16, 24, 24};
        std::vector<I::HandleType> handles;
        MutationProbe::reset();
        for (int i = 0; i < count; ++i)
        {
            const auto added = tree.insert(original, int(i));
            REQUIRE(added.ok());
            handles.push_back(added.handle);
        }
        CHECK(tree.stats().nodes == 1);
        CHECK(MutationProbe::splitAttempts > 0);
        CHECK(MutationProbe::splitEntries < 8 * std::size_t(count));
        CHECK(MutationProbe::refitEntries == 0); // Insertion extends cached own summaries.
        REQUIRE(tree.reserve({.entries = count, .descriptors = count}) == Status::Ok);
        const auto allocations = resource.allocations;
        resource.failAll = true;
        MutationProbe::reset();
        constexpr int changes = 4 * count;
        for (int i = 0; i < changes; ++i)
            CHECK(tree.update(handles[0], i % 2 == 0 ? moved : original) == Status::Ok);
        CHECK(MutationProbe::splitAttempts > 0 && MutationProbe::splitAttempts < 16);
        CHECK(MutationProbe::splitEntries < 4 * std::size_t(changes));
        CHECK(MutationProbe::refitEntries > 0 && MutationProbe::refitEntries < 4 * std::size_t(changes));
        CHECK(tree.stats().nodes == 1);
        CHECK(tree.setFilter(handles[0], {0, 0}) == Status::Ok);
        std::size_t reports = 0;
        CHECK(tree.queryPartners(handles[0], [&](const auto&) noexcept { ++reports; return Continue; }).complete());
        CHECK(reports == 0);
        CHECK(tree.query(Box<int>{20, 20, 20, 20}, [&](const auto&) noexcept { ++reports; return Continue; }).complete());
        CHECK(reports == std::size_t(count));
        MutationProbe::reset();
        for (std::size_t i = 1; i < handles.size(); ++i)
            CHECK(tree.remove(handles[i]) == Status::Ok);
        CHECK(MutationProbe::refitEntries < 4 * std::size_t(count) + 64);
        CHECK(tree.size() == 1 && tree.valid(handles[0]));
        CHECK(tree.setFilter(handles[0], {}) == Status::Ok);
        reports = 0;
        CHECK(tree.query(original, Filter{1, 1}, [&](const auto& entry) noexcept
            { CHECK(entry.handle == handles[0]); ++reports; return Continue; }).complete());
        CHECK(reports == 1);
        CHECK(resource.allocations == allocations);
    }

    // Failed preparation must not consume the retry counter or change caches.
    auto made = I::create({.extent = QuadTree::Extent::Fixed, .root = Box<int>{0, 0, 64, 64},
        .splitThreshold = 2, .mergeThreshold = 0, .smallSetThreshold = 0, .hardCapacity = true});
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    REQUIRE(tree.reserve({.entries = 6, .nodes = 1, .descriptors = 6}) == Status::Ok);
    std::array<I::HandleType, 6> handles;
    for (std::size_t i = 0; i < handles.size(); ++i)
    {
        const auto added = tree.insert(Box<int>{1, 1, 2, 2}, static_cast<int>(i));
        REQUIRE(added.ok()); handles[i] = added.handle;
    }
    for (int i = 0; i < 32; ++i)
    {
        CHECK(tree.update(handles[0], Box<int>{1, 1, 2, 2}) == Status::Ok);
        CHECK(tree.setFilter(handles[0], {}) == Status::Ok);
    }
    CHECK(tree.update(handles[0], Box<int>{33, 33, 34, 34}) == Status::Ok);
    MutationProbe::reset();
    for (int i = 0; i < 3; ++i)
        CHECK(tree.update(handles[1], Box<int>{49, 49, 50, 50}) == Status::CapacityExhausted);
    CHECK(MutationProbe::splitAttempts == 3);
    CHECK((tree.rect(handles[1]) == Box<int>{1, 1, 2, 2}));
    REQUIRE(tree.reserve({.nodes = 64}) == Status::Ok);
    CHECK(tree.update(handles[1], Box<int>{49, 49, 50, 50}) == Status::Ok);
    CHECK(tree.stats().nodes > 1);
    for (auto handle : handles) CHECK(tree.valid(handle));
}

void coarseAncestorCaches()
{
    using I = QuadTree::Index<int, int, MutationProbe>;
    constexpr int coarse = 2000, small = 64;
    TestResource resource;
    auto made = I::create({.extent = QuadTree::Extent::Fixed, .root = Box<int>{0, 0, 1024, 1024},
        .smallSetThreshold = 0}, &resource);
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    std::vector<QuadTree::BuildEntry<int, int>> input;
    for (int i = 0; i < coarse; ++i)
        input.push_back({{0, 0, 1024, 1024}, i, {2, 2}});
    for (int i = 0; i < small; ++i)
    {
        const int x = 32 + (i % 8) * 112, y = 32 + (i / 8) * 112;
        input.push_back({{x, y, x + 8, y + 8}, coarse + i, {1, 1}});
    }
    QuadTree::Workspace<int> scratch(&resource);
    REQUIRE(scratch.reserve(input.size()) == Status::Ok);
    std::vector<I::HandleType> handles(input.size());
    REQUIRE(tree.build(input, handles, {.workspace = &scratch}) == Status::Ok);
    CHECK(tree.stats().nodes > 1);
    const auto allocations = resource.allocations;
    resource.failAll = true;
    MutationProbe::reset();
    for (int i = 0; i < 4000; ++i)
        CHECK(tree.update(handles[coarse], Box<int>{32 + i % 2, 32, 40, 40}) == Status::Ok);
    CHECK(MutationProbe::splitEntries == 0);
    CHECK(MutationProbe::refitEntries <= 8 * 4000);
    CHECK(tree.setFilter(handles[0], {4, 4}) == Status::Ok);
    CHECK(tree.update(handles[coarse], Box<int>{33, 32, 40, 40}) == Status::Ok);
    const auto query_unique = [&](Filter f, const Box<int>& box, int expected) noexcept
    {
        int reports = 0;
        CHECK(tree.query(box, f, [&](const auto&) noexcept { ++reports; return Continue; }).complete());
        CHECK(reports == expected);
    };
    query_unique({4, 4}, {800, 800, 801, 801}, 1);
    CHECK(tree.setFilter(handles[0], {2, 2}) == Status::Ok);
    query_unique({4, 4}, {800, 800, 801, 801}, 0);
    // Both changed own caches must stay conservative when moving between a
    // child and its coarse ancestor, not just between disjoint leaf paths.
    CHECK(tree.setFilter(handles[coarse], {8, 8}) == Status::Ok);
    CHECK(tree.update(handles[coarse], Box<int>{0, 0, 1024, 1024}) == Status::Ok);
    query_unique({8, 8}, {800, 800, 801, 801}, 1);
    CHECK(tree.update(handles[coarse], Box<int>{32, 32, 40, 40}) == Status::Ok);
    query_unique({8, 8}, {36, 36, 36, 36}, 1);
    query_unique({8, 8}, {800, 800, 801, 801}, 0);
    CHECK(resource.allocations == allocations);
}

template<bool Instrumented>
void orderedMovedSources()
{
    using I = QuadTree::Index<int, int, QuadTree::Checked, Instrumented>;
    using HandlePair = std::pair<typename I::HandleType, typename I::HandleType>;
    TestResource resource;
    auto made = I::create({.smallSetThreshold = 0}, &resource);
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    const std::array<std::uint64_t, 7> ranks{10, 50, 20, 90, 30, 70, 60};
    std::array<typename I::HandleType, ranks.size()> handles{};
    for (std::size_t i = 0; i < ranks.size(); ++i)
    {
        const auto added = tree.insert(Box<int>{0, 0, 3, 3}, static_cast<int>(i), {}, ranks[i]);
        REQUIRE(added.ok()); handles[i] = added.handle;
    }
    QuadTree::Workspace<int> workspace(&resource);
    REQUIRE(workspace.reserve(7) == Status::Ok);
    REQUIRE(tree.stats().nodes == 1);
    const auto allocations = resource.allocations;
    resource.failAll = true;
    const std::vector<HandlePair> ranked{{handles[0], handles[1]}, {handles[2], handles[1]}, {handles[4], handles[1]},
        {handles[1], handles[6]}, {handles[1], handles[5]}, {handles[1], handles[3]}};
    // Discovery tests three lower ranks. Then three clean rows emit one each,
    // and the moved row emits three only after all of its candidates are tested.
    constexpr std::array<std::size_t, 10> orderedPrefix{0, 0, 0, 0, 1, 2, 3, 3, 3, 6};
    for (bool ordered : {false, true})
    {
        std::vector<HandlePair> pairs, qualified;
        pairs.reserve(ranked.size()); qualified.reserve(ranked.size());
        const auto run = [&](QuadTree::WorkBudget budget, std::size_t stop = 0)
        {
            pairs.clear(); qualified.clear();
            const auto result = tree.forEachPairInvolving(std::array{handles[1]}, [&](const auto& a, const auto& b) noexcept
            {
                REQUIRE(pairs.size() < ranked.size());
                pairs.emplace_back(a.handle, b.handle);
                return pairs.size() == stop ? Stop : Continue;
            }, {.ordered = ordered, .workspace = &workspace, .budget = budget},
            [&](const auto& a, const auto& b) noexcept
            {
                REQUIRE(qualified.size() < ranked.size());
                if (ordered) CHECK(*a.rank < *b.rank);
                qualified.emplace_back(a.handle, b.handle);
                return true;
            });
            if constexpr (Instrumented)
                CHECK(result.counters.predicateCalls == qualified.size() && result.counters.reports == pairs.size());
            else checkZeroCounters(result.counters);
            return result;
        };
        REQUIRE(run({}).complete() && pairs.size() == ranked.size() && qualified.size() == ranked.size());
        const auto expected = pairs, expectedQualified = qualified;
        if (ordered) CHECK(expected == ranked);
        else CHECK(std::is_permutation(expected.begin(), expected.end(), ranked.begin(), ranked.end(),
            [](const HandlePair& a, const HandlePair& b) { return a == b || (a.first == b.second && a.second == b.first); }));
        const std::uint64_t total = ordered ? 9 : 6;
        for (std::uint64_t nodes = 0; nodes <= 3; ++nodes)
            for (std::uint64_t candidates = 0; candidates <= total + 1; ++candidates)
            {
                const auto result = run({.candidateTests = candidates, .nodeVisits = nodes});
                const auto work = std::min(candidates, nodes == 0 ? std::uint64_t{0} : nodes == 1 ? std::uint64_t{6} : total);
                const auto prefix = ordered ? orderedPrefix[work] : work;
                const auto predicateCalls = ordered ? (work > 3 ? work - 3 : 0) : work;
                CHECK(result.status == (work == total ? Status::Ok : Status::BudgetExhausted));
                if constexpr (Instrumented)
                {
                    CHECK(result.counters.candidateTests == work && result.counters.filterRejections == 0);
                    CHECK(result.counters.nodeVisits == (nodes == 0 ? 0u : ordered && nodes >= 2 && candidates >= 6 ? 2u : 1u));
                }
                CHECK(qualified.size() == predicateCalls && std::equal(qualified.begin(), qualified.end(), expectedQualified.begin()));
                CHECK(pairs.size() == prefix && std::equal(pairs.begin(), pairs.end(), expected.begin()));
            }
        for (std::size_t stop = 1; stop <= expected.size(); ++stop)
        {
            CHECK(run({}, stop).status == Status::Stopped);
            CHECK(pairs.size() == stop && std::equal(pairs.begin(), pairs.end(), expected.begin()));
        }
        CHECK(run({.candidateTests = ordered ? 4u : 1u, .nodeVisits = 1}, 1).status == Status::Stopped);
        CHECK(pairs.size() == 1 && pairs.front() == expected.front());
        CHECK(run({}).complete() && pairs == expected && qualified == expectedQualified);
    }
    CHECK(resource.allocations == allocations);
    resource.failAll = false;
    // Source-only epoch marks must not become dirty entries on the next call.
    for (bool ordered : {false, true})
    {
        std::size_t reports = 0;
        CHECK(tree.forEachPairInvolving(std::array{handles[0]}, [&](const auto& a, const auto& b) noexcept
        {
            CHECK(a.payload == 0 || b.payload == 0);
            ++reports; return Continue;
        }, {.ordered = ordered, .workspace = &workspace}).complete());
        CHECK(reports == 6);
    }
    CHECK(tree.forEachPairSelected([](const auto& a) noexcept { return a.payload == 0; },
        [](const auto& a) noexcept { return a.payload != 0; },
        [](const auto&, const auto&) noexcept { return Continue; }, {.workspace = &workspace}).complete());
    std::size_t reports = 0;
    CHECK(tree.forEachPairInvolving(std::array{handles[1]}, [&](const auto&, const auto&) noexcept
        { ++reports; return Continue; }, {.ordered = true, .workspace = &workspace}).complete());
    CHECK(reports == 6);
    REQUIRE(tree.insert(Box<int>{0, 0, 3, 3}, 7, {0, 0}).ok());
    REQUIRE(workspace.reserve(8) == Status::Ok);
    std::size_t callbacks = 0;
    const auto missing = tree.forEachPairInvolving(std::array{handles[1]},
        [&](const auto&, const auto&) noexcept { ++callbacks; return Stop; },
        {.ordered = true, .workspace = &workspace},
        [&](const auto&, const auto&) noexcept { ++callbacks; return true; });
    CHECK(missing.status == Status::MissingRank && callbacks == 0);
}

void orderedMovedWork()
{
    constexpr int count = 8192;
    TestResource resource;
    auto made = Tree<int>::create({.smallSetThreshold = 0}, &resource);
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    QuadTree::Workspace<int> workspace(&resource);
    REQUIRE(workspace.reserve(count) == Status::Ok);
    std::vector<QuadTree::BuildEntry<int, int>> input;
    for (int i = 0; i < count; ++i)
    {
        const int x = (i % 128) * 16, y = (i / 128) * 16;
        input.push_back({{x, y, x + 20, y + 20}, i, {}, i == 0 ? UINT64_MAX : std::uint64_t(count - i - 1)});
    }
    std::vector<Tree<int>::HandleType> handles(input.size());
    REQUIRE(tree.build(input, handles, {.workspace = &workspace}) == Status::Ok);
    const auto allocations = resource.allocations;
    resource.failAll = true;
    std::vector<RankPair> pairs, expected;
    for (std::size_t i = 1; i < input.size(); ++i)
        if (overlap(input[0].rect, input[i].rect, Boundary::Default))
            expected.emplace_back(*input[i].rank, *input[0].rank);
    std::sort(expected.begin(), expected.end());
    const auto result = tree.forEachPairInvolving(std::array{handles[0]}, [&](const auto& a, const auto& b) noexcept
        { pairs.emplace_back(*a.rank, *b.rank); return Continue; }, {.ordered = true, .workspace = &workspace});
    CHECK(result.complete() && pairs == expected);
    CHECK(result.counters.nodeVisits < 512 && result.counters.candidateTests < 256);
    CHECK(resource.allocations == allocations);
    resource.failAll = false;
    // A highest-ranked mover in a pile needs n-1 rows, but each clean row can
    // query S directly rather than scan all n entries again.
    input.resize(2048);
    handles.resize(input.size());
    for (std::size_t i = 0; i < input.size(); ++i)
        input[i] = {{0, 0, 1, 1}, static_cast<int>(i), {}, i};
    REQUIRE(tree.build(input, handles, {.workspace = &workspace}) == Status::Ok);
    resource.failAll = true;
    pairs.clear();
    const auto pile = tree.forEachPairInvolving(std::array{handles.back()}, [&](const auto& a, const auto& b) noexcept
        { pairs.emplace_back(*a.rank, *b.rank); return Continue; }, {.ordered = true, .workspace = &workspace});
    CHECK(pile.complete() && pairs.size() == input.size() - 1);
    CHECK(std::is_sorted(pairs.begin(), pairs.end()));
    CHECK(pile.counters.nodeVisits == 1 && pile.counters.candidateTests == 2 * (input.size() - 1));
}

void pairWorkspaceGrowth()
{
    using I = Tree<int>;
    constexpr std::size_t count = 256, stride = 16;
    TestResource storage, scratch;
    auto made = I::create({.extent = QuadTree::Extent::Fixed, .root = Box<int>{0, 0, 1024, 1024},
        .smallSetThreshold = 0}, &storage);
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    QuadTree::Workspace<int> build(&storage), work(&scratch);
    REQUIRE(build.reserve({0, 0, count}) == Status::Ok);
    REQUIRE(work.reserve({count, count / stride, 0}) == Status::Ok);
    const auto scratchAllocations = scratch.allocations, bytes = work.retainedBytes();
    REQUIRE(bytes != 0 && scratch.bytes == bytes);
    scratch.failAll = true;

    std::array<QuadTree::BuildEntry<int, int>, count> input{};
    std::array<I::HandleType, count> handles{};
    std::array<I::HandleType, count / stride> moved{};
    for (std::size_t group : {1u, 16u, 256u})
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            const int x = static_cast<int>(i / group) * 4;
            input[i] = {{x, 0, x + 2, 2}, 1, {}, i};
        }
        REQUIRE(tree.build(input, handles, {.workspace = &build}) == Status::Ok);
        REQUIRE(tree.size() == count && tree.stats().references == count);
        for (std::size_t i = 0; i < moved.size(); ++i)
            moved[i] = handles[i * stride];
        const auto storageAllocations = storage.allocations;
        storage.failAll = true;
        for (bool movedOnly : {false, true})
        {
            // Test-only bounded identity oracle. No pair buffer in the measured resources.
            std::bitset<count * count> expected;
            for (std::size_t i = 0; i < count; ++i)
                for (std::size_t j = i + 1; j < count; ++j)
                    if ((!movedOnly || i % stride == 0 || j % stride == 0) &&
                        overlap(input[i].rect, input[j].rect, Boundary::Default))
                        expected.set(i * count + j);
            const auto pairs = expected.count();
            for (bool ordered : {false, true})
            {
                auto pending = expected;
                std::size_t last = 0;
                const auto visit = [&](const auto& a, const auto& b) noexcept
                {
                    REQUIRE(a.rank && b.rank && *a.rank < count && *b.rank < count);
                    const auto x = static_cast<std::size_t>(*a.rank), y = static_cast<std::size_t>(*b.rank);
                    CHECK(a.handle == handles[x] && b.handle == handles[y]);
                    const auto key = std::min(x, y) * count + std::max(x, y);
                    CHECK(pending.test(key));
                    pending.reset(key);
                    if (ordered) CHECK(x < y && key > last);
                    last = key;
                    return Continue;
                };
                const QuadTree::QueryOptions<int> options{.ordered = ordered, .workspace = &work};
                const auto result = movedOnly ? tree.forEachPairInvolving(moved, visit, options) : tree.forEachPair(visit, options);
                CHECK(result.complete() && result.counters.reports == pairs && pending.none());
                CHECK(storage.allocations == storageAllocations && scratch.allocations == scratchAllocations);
                CHECK(work.retainedBytes() == bytes && scratch.bytes == bytes && scratch.peak == bytes);
            }
            std::printf("pair-workspace %s group=%zu: %zu entries, %zu pairs, %zu bytes retained/peak\n",
                movedOnly ? "moved" : "full", group, count, pairs, bytes);
        }
        storage.failAll = false;
    }
}

void coincidentMotion()
{
    auto made = Tree<int>::create({.extent = QuadTree::Extent::Fixed, .root = Box<int>{0, 0, 1024, 1024},
        .smallSetThreshold = 0});
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    std::vector<Tree<int>::HandleType> handles;
    for (int i = 0; i < 128; ++i)
    {
        const auto added = tree.insert(Box<int>{0, 0, 1, 1}, int(i));
        REQUIRE(added.ok()); handles.push_back(added.handle);
    }
    CHECK(tree.stats().nodes == 1);
    auto result = tree.forEachPair([](const auto&, const auto&) noexcept { return Continue; });
    CHECK(result.counters.candidateTests == 8128 && result.counters.reports == 8128);
    for (std::size_t i = 0; i < handles.size(); ++i)
    {
        const int x = static_cast<int>(i % 16) * 64, y = static_cast<int>(i / 16) * 64;
        CHECK(tree.update(handles[i], Box<int>{x, y, x + 1, y + 1}) == Status::Ok);
    }
    CHECK(tree.stats().nodes > 1);
    result = tree.forEachPair([](const auto&, const auto&) noexcept { return Continue; });
    CHECK(result.complete() && result.counters.reports == 0 && result.counters.candidateTests < 1024);
}

void concurrency()
{
    auto made = Tree<int>::create();
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    for (int i = 0; i < 80; ++i) REQUIRE(tree.insert(Box<int>{i, 0, i + 2, 2}, int(i), {}, std::uint64_t(i)).ok());
    const Tree<int>& read_only = tree;
    std::array<std::size_t, 4> reports{};
    std::array<std::thread, 4> threads;
    for (std::size_t t = 0; t < threads.size(); ++t)
        threads[t] = std::thread([&, t]
        {
            QuadTree::Workspace<int> outer, nested;
            if (outer.reserve(80) != Status::Ok || nested.reserve(80) != Status::Ok) std::abort();
            const auto result = read_only.forEachPair([&](const auto& a, const auto&) noexcept
            {
                ++reports[t];
                const auto queried = read_only.queryPartners(a.handle, [](const auto&) noexcept { return Continue; },
                    {.ordered = true, .workspace = &nested});
                if (!queried.complete()) std::abort();
                return Continue;
            }, {.ordered = true, .workspace = &outer});
            if (!result.complete()) std::abort();
        });
    for (auto& thread : threads) thread.join();
    for (auto count : reports) CHECK(count == 79);
}

void uninstrumented()
{
    using I = QuadTree::Index<int, int, QuadTree::Unchecked, false>;
    auto made = I::create();
    REQUIRE(made.ok());
    auto tree = std::move(made).value();
    REQUIRE(tree.insert(Box<int>{0, 0, 3, 3}, 1).ok());
    REQUIRE(tree.insert(Box<int>{1, 1, 2, 2}, 2).ok());
    struct BorrowedPredicate
    {
        BorrowedPredicate() = default;
        BorrowedPredicate(const BorrowedPredicate&) = delete;
        bool operator()(const I::View&, const I::View&) const noexcept { return true; }
    } predicate;
    std::size_t reports = 0;
    auto visitor = [&](const auto&, const auto&) noexcept { ++reports; return Continue; };
    const auto complete = tree.forEachPair(visitor, {}, predicate);
    CHECK(complete.complete() && reports == 1 && complete.counters.reports == 0 && complete.counters.candidateTests == 0);
    CHECK(tree.forEachPair(visitor, {.budget = {.candidateTests = 0}}, predicate).status == Status::BudgetExhausted);
    CHECK(reports == 1);
}

struct ConstPayloadEntry
{
    Box<int> rect;
    const int payload;
    Filter filter;
    std::optional<std::uint64_t> rank;
};
template<typename R>
concept CanConsume = requires(Tree<int>& tree, R& range) { tree.buildConsume(range); };
static_assert(!CanConsume<std::array<ConstPayloadEntry, 1>>);
static_assert(!CanConsume<const std::array<QuadTree::BuildEntry<int, int>, 1>>);
static_assert(CanConsume<std::array<QuadTree::BuildEntry<int, int>, 1>>);

struct ThrowVisitor { QuadTree::Visit operator()(const Tree<int>::View&, const Tree<int>::View&) const { return Continue; } };
struct BoolVisitor { bool operator()(const Tree<int>::View&, const Tree<int>::View&) const noexcept { return true; } };
template<typename V>
concept PairVisitor = requires(const Tree<int>& tree, V visitor) { tree.forEachPair(visitor); };
static_assert(!PairVisitor<ThrowVisitor> && !PairVisitor<BoolVisitor>);

namespace nonthrowingApi
{
// Use exception specifications, not throw expressions: these assertions also run under -fno-exceptions.
struct ThrowingMove { ThrowingMove(ThrowingMove&&) noexcept(false) {} };
struct ThrowingDestructor { ~ThrowingDestructor() noexcept(false) {} };
struct ThrowingCopy
{
    ThrowingCopy(const ThrowingCopy&) noexcept(false) {}
    ThrowingCopy(ThrowingCopy&&) noexcept = default;
};

template<typename P>
concept IndexPayload = requires { typename QuadTree::Index<P, int>; };
static_assert(!IndexPayload<ThrowingMove> && !IndexPayload<ThrowingDestructor>);

template<typename P, typename Geometry = Box<int>, bool Nothrow = true>
struct Accessor
{
    Geometry operator()(const P&) & noexcept(Nothrow) { return {}; }
};

template<typename P, bool Copy>
consteval bool payloadOperations()
{
    using I = QuadTree::Index<P, int>;
    using Entries = std::array<QuadTree::BuildEntry<P, int>, 1>;
    using Output = std::span<typename I::HandleType>;
    static_assert(requires(I& tree, const P& value) { tree.insert(Box<int>{}, value); } == Copy);
    static_assert(requires(I& tree, const P& value, Accessor<P>& accessor) { tree.insertWith(value, accessor); } == Copy);
    static_assert(requires(I& tree, const Entries& entries) { tree.build(entries); } == Copy);
    static_assert(requires(I& tree, const Entries& entries, Output out) { tree.build(entries, out); } == Copy);
    static_assert(requires(I& tree, const std::array<P, 1>& values, Accessor<P>& accessor) { tree.buildWith(values, accessor); } == Copy);
    static_assert(requires(I& tree, P&& value, Accessor<P>& accessor, Entries& entries, Output out, std::array<P, 1>& values)
    {
        tree.insert(Box<int>{}, std::move(value));
        tree.insertWith(std::move(value), accessor);
        tree.buildConsume(entries);
        tree.buildConsume(entries, out);
        tree.buildConsumeWith(values, accessor);
    });
    return true;
}
static_assert(payloadOperations<int, true>());
static_assert(payloadOperations<ThrowingCopy, false>());
static_assert(payloadOperations<MoveOnly, false>());

template<typename A, bool Accept>
consteval bool accessorOperations()
{
    static_assert(requires(Tree<int>& tree, A& accessor) { tree.insertWith(1, accessor); } == Accept);
    static_assert(requires(Tree<int>& tree, A& accessor, std::array<int, 1>& values) { tree.buildWith(values, accessor); } == Accept);
    static_assert(requires(Tree<int>& tree, A& accessor, std::array<int, 1>& values) { tree.buildConsumeWith(values, accessor); } == Accept);
    return true;
}
static_assert(accessorOperations<Accessor<int>, true>());
static_assert(accessorOperations<Accessor<int, Rect<int>>, true>());
static_assert(accessorOperations<Accessor<int, Box<int>, false>, false>());
static_assert(accessorOperations<Accessor<int, int>, false>());

using View = Tree<int>::View;
template<bool Nothrow, typename Result = bool>
struct Selector { Result operator()(const View&) & noexcept(Nothrow) { return {}; } };
struct ThrowingBool { operator bool() const noexcept(false) { return true; } };
struct PairReport { QuadTree::Visit operator()(const View&, const View&) & noexcept { return Continue; } };
template<bool Nothrow>
struct PairPredicate { bool operator()(const View&, const View&) & noexcept(Nothrow) { return true; } };

template<typename L, typename R, typename P = PairPredicate<true>>
concept SelectsPairs = requires(const Tree<int>& tree, L& left, R& right, PairReport& report, P& predicate)
{
    tree.forEachPairSelected(left, right, report, {}, predicate);
};
static_assert(SelectsPairs<Selector<true>, Selector<true>>);
static_assert(!SelectsPairs<Selector<false>, Selector<true>>);
static_assert(!SelectsPairs<Selector<true>, Selector<false>>);
static_assert(!SelectsPairs<Selector<true, ThrowingBool>, Selector<true>>);
static_assert(!SelectsPairs<Selector<true>, Selector<true>, PairPredicate<false>>);

// Losing noexcept on these virtual functions would allow throwing overrides.
static_assert(requires(QuadTree::Resource& resource, void* ptr)
{
    { resource.allocate(1, 1) } noexcept;
    { resource.deallocate(ptr) } noexcept;
});
static_assert(std::is_nothrow_destructible_v<QuadTree::Resource>);
} // namespace nonthrowingApi

static_assert(std::is_nothrow_move_constructible_v<Tree<int>> && !std::is_copy_constructible_v<Tree<int>>);
static_assert(std::is_nothrow_move_assignable_v<QuadTree::Workspace<int>>);
static_assert(!std::same_as<Tree<int>::HandleType, Tree<float>::HandleType>);
static_assert(!std::same_as<Tree<int>::HandleType, QuadTree::Index<int, int>::HandleType>);
static_assert(sizeof(Tree<int>::HandleType) == 16 && std::is_trivially_copyable_v<Tree<int>::HandleType>);
} // namespace

int main()
{
    contracts(); boundedCollection<true>(); boundedCollection<false>();
    fixedRootContacts(); budgets(); orderedPermutations();
    crossAndSelections(); crossPrefixes<true>(); crossPrefixes<false>(); crossAfterMutations();
    selectionPrefixes<true>(); selectionPrefixes<false>();
    filterPolicyTruthTable(); filterPruningWork();
    hardCapacity(); isolatedCapacity(); referenceGrowthFailure(); workspaceAllocationFailures();
    indexBufferAllocationFailures(); failureInjection();
    rebuildPeaks(); preparedSmallBuild(); preparedFloatBoxes<float>(); preparedFloatBoxes<double>(); tinyPairs(); identities();
    payloadsAndMoves(); stagedSelfRebuild(); moveOnlyBuildAllocationFailures();
    cacheAndChurn(); movedPredicateCache(); mutationWork(); coarseAncestorCaches();
    orderedMovedSources<true>(); orderedMovedSources<false>(); orderedMovedWork();
    pairWorkspaceGrowth(); coincidentMotion(); concurrency(); uninstrumented();
    numeric<std::int32_t>(); numeric<std::int64_t>(); numeric<float>(); numeric<double>();
    translatedInt64Snapshots(); int64PairPrefixes(); int64LongRows();
    for (auto boundary : {Boundary::Default, Boundary::TouchInclusive})
        for (auto extent : {QuadTree::Extent::Fixed, QuadTree::Extent::Adaptive, QuadTree::Extent::Snapshot})
        {
            randomized<std::int32_t>(boundary, extent);
            randomized<std::int64_t>(boundary, extent);
            randomized<float>(boundary, extent);
            randomized<double>(boundary, extent);
        }
    flatIdentityExhaustion();
    std::printf("index-test: %d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
