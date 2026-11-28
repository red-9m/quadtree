// Randomized differential fuzzer for QuadTree::Index (bench/MANIFEST.md).
//
// Random insert/update/setFilter/remove/build/shrink/clear sequences on every
// coordinate type and several scenes. After every step the internal tree is
// checked (links, counts, conservative bounds and filter summaries, placement,
// pooled node accounting); every few steps the public results are compared with
// a brute-force oracle: entries, full unordered/ordered pairs, unordered/ordered
// moved sets, region and partner queries. Build with sanitizers for memory checks.
//
// Usage: quadtree-index-fuzz [--quick] [--seeds=N] [--seed=S] [--steps=N] [--max-entries=N]
//                            [--types=int32,int64,float,double] [--scenes=LIST]
// Scenes: uniform, clustered, mixed, pile, outliers. A failure prints the
// options that reproduce it; exit code 1.

// Standard headers used by the library come first: the access override below
// must not reach them.
#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <numeric>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// Internal invariants need the tree's private state. Development tool only.
#ifdef __clang__
#pragma clang diagnostic ignored "-Wkeyword-macro"
#endif
#define private public
#include "quadtree-index.h"
#undef private

namespace
{
using QuadTree::Boundary;
using QuadTree::Box;
using QuadTree::Continue;
using QuadTree::Filter;
using QuadTree::Status;

constexpr std::uint32_t kNone = QuadTree::detail::kNone;

struct Context
{
    const char* type = "";
    const char* scene = "";
    std::uint64_t seed = 0;
    unsigned step = 0;
    int failures = 0;
} gContext;

#define CHECK(condition) do { if (!(condition)) { \
    if (gContext.failures++ < 10) \
        std::fprintf(stderr, "%s %s seed %llu step %u, line %d: %s\n", gContext.type, gContext.scene, \
            static_cast<unsigned long long>(gContext.seed), gContext.step, __LINE__, #condition); \
    } } while (false)

struct Rng
{
    std::uint64_t state;
    std::uint64_t next() noexcept
    {
        std::uint64_t z = (state += 0x9e3779b97f4a7c15u);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9u;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebu;
        return z ^ (z >> 31);
    }
    std::size_t below(std::size_t n) noexcept { return static_cast<std::size_t>(next() % n); }
    long long range(long long low, long long high) noexcept
    {
        return low + static_cast<long long>(next() % static_cast<std::uint64_t>(high - low));
    }
};

enum class Scene { Uniform, Clustered, Mixed, Pile, Outliers };
constexpr std::string_view kSceneNames[] = {"uniform", "clustered", "mixed", "pile", "outliers"};

template<typename T> constexpr const char* typeName();
template<> constexpr const char* typeName<std::int32_t>() { return "int32"; }
template<> constexpr const char* typeName<std::int64_t>() { return "int64"; }
template<> constexpr const char* typeName<float>() { return "float"; }
template<> constexpr const char* typeName<double>() { return "double"; }

// Oracle helpers share no code with the library.
template<typename T>
bool overlap(const Box<T>& a, const Box<T>& b, Boundary mode) noexcept
{
    const bool closed = mode == Boundary::TouchInclusive || a.minX == a.maxX || a.minY == a.maxY ||
        b.minX == b.maxX || b.minY == b.maxY;
    if (closed)
        return !(a.maxX < b.minX || b.maxX < a.minX || a.maxY < b.minY || b.maxY < a.minY);
    return !(a.maxX <= b.minX || b.maxX <= a.minX || a.maxY <= b.minY || b.maxY <= a.minY);
}

bool accepts(Filter a, Filter b) noexcept { return (a.mask & b.category) != 0 && (b.mask & a.category) != 0; }

template<typename T>
bool inside(const Box<T>& outer, const Box<T>& inner) noexcept
{
    return outer.minX <= inner.minX && outer.minY <= inner.minY && inner.maxX <= outer.maxX &&
        inner.maxY <= outer.maxY;
}

template<typename T>
Box<T> box(long long x, long long y, long long w, long long h) noexcept
{
    return {static_cast<T>(x), static_cast<T>(y), static_cast<T>(x + w), static_cast<T>(y + h)};
}

// Extreme outlier: full-range box, or a small box near the coordinate limits.
template<typename T>
Box<T> extreme(Rng& rng) noexcept
{
    constexpr T low = std::numeric_limits<T>::lowest();
    constexpr T high = std::numeric_limits<T>::max();
    if (rng.below(3) == 0)
        return {low, low, high, high};
    const bool negative = rng.below(2) == 0;
    if constexpr (std::is_integral_v<T>)
    {
        const T offset = static_cast<T>(rng.below(1000));
        const T x = negative ? static_cast<T>(low + offset) : static_cast<T>(high - offset - 1);
        return {x, static_cast<T>(-1 - x / 2), static_cast<T>(x + 1), static_cast<T>(-x / 2)};
    }
    else
    {
        const T scale = high / T(400) * static_cast<T>(rng.below(100) + 1); // Up to max/4
        const T x = negative ? -scale : scale;
        return {x, -x, x + scale / T(1024), -x + scale / T(1024)};
    }
}

template<typename T>
Box<T> generate(Rng& rng, Scene scene) noexcept
{
    switch (scene)
    {
    case Scene::Uniform:
        return box<T>(rng.range(-500, 500), rng.range(-500, 500), rng.range(0, 30), rng.range(0, 30));
    case Scene::Clustered:
    {
        const long long size = rng.range(0, 3);
        return box<T>(rng.range(0, 20), rng.range(0, 20), size, size);
    }
    case Scene::Mixed:
    {
        const long long size = rng.below(10) == 0 ? rng.range(0, 800) : rng.range(0, 5);
        return box<T>(rng.range(-500, 500), rng.range(-500, 500), size, size);
    }
    case Scene::Pile:
    {
        const long long x = rng.range(0, 2);
        return box<T>(x, x, 1, 1);
    }
    case Scene::Outliers:
        if (rng.below(30) == 0)
            return extreme<T>(rng);
        return box<T>(rng.range(0, 100), rng.range(0, 100), 2, 2);
    }
    return {};
}

// Small move along x that never overflows; may become a no-op for huge floats.
template<typename T>
Box<T> nudge(Box<T> b, Rng& rng) noexcept
{
    const T step = std::is_integral_v<T> ? T(1) : static_cast<T>(0.5);
    const bool up = rng.below(2) == 0;
    if (up && b.maxX <= std::numeric_limits<T>::max() - step)
    {
        b.minX = static_cast<T>(b.minX + step);
        b.maxX = static_cast<T>(b.maxX + step);
    }
    else if (b.minX >= std::numeric_limits<T>::lowest() + step)
    {
        b.minX = static_cast<T>(b.minX - step);
        b.maxX = static_cast<T>(b.maxX - step);
    }
    return b;
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

template<typename T>
Box<T> unite(const Box<T>& a, const Box<T>& b) noexcept
{
    return {std::min(a.minX, b.minX), std::min(a.minY, b.minY), std::max(a.maxX, b.maxX), std::max(a.maxY, b.maxY)};
}

bool covers(Filter outer, Filter inner) noexcept
{
    return (inner.category & ~outer.category) == 0 && (inner.mask & ~outer.mask) == 0;
}

// Internal tree invariants. Own summaries may conservatively keep removed
// contributions until a tight refit (itemDirty); everything else is exact.
template<typename T>
void checkTree(const Tree<T>& tree)
{
    const auto& config = tree.mConfig;
    if (tree.mRoot == kNone)
    {
        CHECK(tree.size() <= config.smallSetThreshold);
        CHECK(tree.mActiveNodes == 0);
        return;
    }
    CHECK(tree.size() > config.smallSetThreshold);
    const auto& boxes = tree.mEntries.mBoxes;
    const auto& meta = tree.mEntries.mMeta;
    std::vector<int> seen(tree.size(), 0);
    std::size_t reachable = 0;
    auto visit = [&](auto& self, std::uint32_t n, std::uint32_t parent) -> void
    {
        ++reachable;
        const auto& node = tree.mNodes[n];
        CHECK(node.parent == parent);
        CHECK(node.depth <= config.maxDepth);
        if (parent != kNone)
            CHECK(node.depth == tree.mNodes[parent].depth + 1);
        std::vector<std::uint32_t> linked;
        std::uint32_t previous = kNone;
        bool any = false;
        Box<T> own{};
        Filter ownFilter{0, 0};
        for (std::uint32_t i = node.head; i != kNone; i = tree.mReferences[i].next)
        {
            CHECK(i < tree.size() && linked.size() < tree.size()); // Corrupt own list
            if (i >= tree.size() || linked.size() >= tree.size())
                return;
            linked.push_back(i);
            ++seen[i];
            CHECK(tree.mReferences[i].node == n);
            CHECK(tree.mReferences[i].previous == previous);
            previous = i;
            own = any ? unite(own, boxes[i]) : boxes[i];
            any = true;
            ownFilter.category |= meta[i].filter.category;
            ownFilter.mask |= meta[i].filter.mask;
            CHECK(tree.belongs(n, boxes[i]));
        }
        CHECK(linked.size() == node.ownCount);
        std::vector<std::uint32_t> traversed; // nextItem() honours the contiguous-snapshot shortcut
        for (std::uint32_t i = node.head; i != kNone && traversed.size() <= linked.size(); i = tree.nextItem(node, i))
            traversed.push_back(i);
        CHECK(traversed == linked);
        if (any)
        {
            CHECK(inside(node.itemBounds, own));
            CHECK(covers(node.itemSummary, ownFilter));
            if (!node.itemDirty)
            {
                CHECK(node.itemBounds == own);
                CHECK(node.itemSummary == ownFilter);
            }
        }
        std::uint32_t count = node.ownCount;
        Box<T> bounds = node.itemBounds;
        Filter summary = any ? node.itemSummary : Filter{0, 0};
        if (node.firstChild != kNone)
            for (std::uint32_t q = 0; q < 4; ++q)
            {
                const std::uint32_t child = node.firstChild + q;
                self(self, child, n);
                const auto& c = tree.mNodes[child];
                if (c.count == 0)
                    continue;
                count += c.count;
                bounds = any ? unite(bounds, c.bounds) : c.bounds;
                any = true;
                summary.category |= c.summary.category;
                summary.mask |= c.summary.mask;
            }
        CHECK(count == node.count);
        if (count != 0)
        {
            CHECK(bounds == node.bounds);
            CHECK(summary == node.summary);
        }
    };
    visit(visit, tree.mRoot, kNone);
    CHECK(reachable == tree.mActiveNodes);
    CHECK(reachable == tree.stats().nodes);
    for (int count : seen)
        CHECK(count == 1);
}

using Pair = std::pair<int, int>;

template<typename T>
std::vector<Pair> expectedPairs(const std::vector<Record<T>>& records, Boundary boundary,
    const std::set<int>* moved = nullptr)
{
    std::vector<Pair> result;
    for (std::size_t i = 0; i < records.size(); ++i)
        for (std::size_t j = i + 1; j < records.size(); ++j)
        {
            const auto& a = records[i];
            const auto& b = records[j];
            if (overlap(a.box, b.box, boundary) && accepts(a.filter, b.filter) &&
                (!moved || moved->contains(a.id) || moved->contains(b.id)))
                result.emplace_back(std::min(a.id, b.id), std::max(a.id, b.id));
        }
    std::sort(result.begin(), result.end());
    return result;
}

template<typename T>
void checkResults(const Tree<T>& tree, const std::vector<Record<T>>& records, QuadTree::Workspace<T>& workspace,
    Scene scene, Rng& rng)
{
    const Boundary boundary = tree.config().boundary;
    CHECK(tree.size() == records.size());
    std::vector<int> ids;
    CHECK(tree.forEachEntry([&](const auto& entry) noexcept
    {
        ids.push_back(entry.payload);
        return Continue;
    }).complete());
    std::sort(ids.begin(), ids.end());
    std::vector<int> expected_ids;
    for (const auto& record : records)
    {
        expected_ids.push_back(record.id);
        CHECK(tree.valid(record.handle));
        CHECK(tree.rect(record.handle) == record.box);
        CHECK(tree.payload(record.handle) == record.id);
        CHECK(tree.filter(record.handle) == record.filter);
        CHECK(tree.rank(record.handle) == record.rank);
    }
    std::sort(expected_ids.begin(), expected_ids.end());
    CHECK(ids == expected_ids);

    const auto expected = expectedPairs(records, boundary);
    std::vector<Pair> actual;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> sequence;
    auto collect = [&](const auto& a, const auto& b) noexcept
    {
        actual.emplace_back(std::min(a.payload, b.payload), std::max(a.payload, b.payload));
        return Continue;
    };
    auto collectOrdered = [&](const auto& a, const auto& b) noexcept
    {
        sequence.emplace_back(*a.rank, *b.rank);
        actual.emplace_back(std::min(a.payload, b.payload), std::max(a.payload, b.payload));
        return Continue;
    };
    const auto ordered_ok = [&]
    {
        for (std::size_t i = 0; i < sequence.size(); ++i)
            if (!(sequence[i].first < sequence[i].second) || (i != 0 && !(sequence[i - 1] < sequence[i])))
                return false;
        return true;
    };
    CHECK(tree.forEachPair(collect, {.workspace = &workspace}).complete());
    std::sort(actual.begin(), actual.end());
    CHECK(actual == expected); // Multiset comparison also detects duplicate reports.
    actual.clear();
    CHECK(tree.forEachPair(collectOrdered, {.ordered = true, .workspace = &workspace}).complete());
    CHECK(ordered_ok());
    std::sort(actual.begin(), actual.end());
    CHECK(actual == expected);

    // Moved sets: empty, single, sparse and dense.
    const std::size_t density = std::array<std::size_t, 4>{0, 1, 10, 2}[rng.below(4)];
    std::set<int> moved_ids;
    std::vector<typename Tree<T>::HandleType> moved;
    for (const auto& record : records)
        if ((density == 1 && moved.empty()) || (density > 1 && rng.below(density) == 0))
        {
            moved_ids.insert(record.id);
            moved.push_back(record.handle);
        }
    std::vector<Pair> expected_moved;
    for (const auto& pair : expected)
        if (moved_ids.contains(pair.first) || moved_ids.contains(pair.second))
            expected_moved.push_back(pair);
    for (bool ordered : {false, true})
    {
        actual.clear();
        sequence.clear();
        const auto result = ordered ?
            tree.forEachPairInvolving(moved, collectOrdered, {.ordered = true, .workspace = &workspace}) :
            tree.forEachPairInvolving(moved, collect, {.workspace = &workspace});
        CHECK(result.complete());
        CHECK(ordered_ok());
        std::sort(actual.begin(), actual.end());
        CHECK(actual == expected_moved);
    }
    if (moved.size() >= 2)
    {
        moved.push_back(moved.front());
        CHECK(tree.forEachPairInvolving(moved, collect, {.workspace = &workspace}).status == Status::DuplicateHandle);
    }

    // Region queries, unfiltered and filtered.
    const Box<T> query = generate<T>(rng, scene);
    const Filter query_filter{rng.next() & 7, rng.next() & 7};
    for (bool filtered : {false, true})
    {
        std::vector<int> hits, expected_hits;
        for (const auto& record : records)
            if (overlap(query, record.box, boundary) && (!filtered || accepts(query_filter, record.filter)))
                expected_hits.push_back(record.id);
        auto hit = [&](const auto& entry) noexcept { hits.push_back(entry.payload); return Continue; };
        CHECK((filtered ? tree.query(query, query_filter, hit) : tree.query(query, hit)).complete());
        std::sort(hits.begin(), hits.end());
        std::sort(expected_hits.begin(), expected_hits.end());
        CHECK(hits == expected_hits);
    }

    // Partner query with an exclusive lower rank bound.
    if (!records.empty())
    {
        const auto& a = records[rng.below(records.size())];
        const std::uint64_t bound = rng.below(2) == 0 ? 0 : a.rank;
        std::vector<int> hits, expected_hits;
        for (const auto& b : records)
            if (a.id != b.id && accepts(a.filter, b.filter) && overlap(a.box, b.box, boundary) && b.rank > bound)
                expected_hits.push_back(b.id);
        CHECK(tree.queryPartners(a.handle, [&](const auto& b) noexcept
            { hits.push_back(b.payload); return Continue; }, {.workspace = &workspace, .greaterRank = bound}).complete());
        std::sort(hits.begin(), hits.end());
        std::sort(expected_hits.begin(), expected_hits.end());
        CHECK(hits == expected_hits);
    }
}

struct Options
{
    std::uint64_t firstSeed = 1;
    std::uint64_t seeds = 40;
    unsigned steps = 1500;
    std::size_t maxEntries = 300;
    std::vector<std::string> types = {"int32", "int64", "float", "double"};
    std::vector<Scene> scenes = {Scene::Uniform, Scene::Clustered, Scene::Mixed, Scene::Pile, Scene::Outliers};
};

template<typename T>
QuadTree::Config<T> configFor(std::uint64_t seed)
{
    QuadTree::Config<T> config;
    config.smallSetThreshold = static_cast<std::uint32_t>(seed % 3) * 8;
    config.splitThreshold = 2 + static_cast<std::uint32_t>(seed % 4);
    config.mergeThreshold = static_cast<std::uint32_t>(seed % 2);
    config.maxDepth = seed % 5 == 0 ? 3 : 64;
    config.boundary = seed % 2 != 0 ? Boundary::Default : Boundary::TouchInclusive;
    if (seed % 4 == 0)
    {
        config.extent = QuadTree::Extent::Fixed;
        config.root = Box<T>{T(-2048), T(-2048), T(2048), T(2048)};
    }
    else if (seed % 4 == 1)
    {
        config.extent = QuadTree::Extent::Snapshot;
        config.snapshotGrowth = QuadTree::SnapshotGrowth::Adaptive;
    }
    return config;
}

// Every seventh seed shares filter bit 8 across its entries, so snapshot builds
// take the common-filter pair kernels. One in eight filter changes drops the
// bit, until the next build restores it on every entry.
constexpr bool sharedFilterSeed(std::uint64_t seed) noexcept { return seed % 7 == 3; }

Filter randomFilter(Rng& rng, std::uint64_t seed, bool mayDrop = false)
{
    const std::uint64_t category = rng.next() & 7, mask = rng.next() & 7;
    const std::uint64_t shared = sharedFilterSeed(seed) && !(mayDrop && category == 0) ? 8 : 0;
    return Filter{category | shared, mask | shared};
}

// Returns false on the first failing step.
template<typename T>
bool run(std::uint64_t seed, Scene scene, const Options& options)
{
    const auto config = configFor<T>(seed);
    if (scene == Scene::Outliers && config.extent == QuadTree::Extent::Fixed)
        return true;
    gContext.type = typeName<T>();
    gContext.scene = kSceneNames[static_cast<int>(scene)].data();
    gContext.seed = seed;
    gContext.step = 0;
    Rng rng{seed * 0x9e3779b97f4a7c15u + static_cast<std::uint64_t>(scene)};
    auto created = Tree<T>::create(config);
    CHECK(created.ok());
    if (!created.ok())
        return false;
    auto tree = std::move(created).value();
    QuadTree::Workspace<T> workspace;
    CHECK(workspace.reserve(options.maxEntries + 1) == Status::Ok);
    const bool fixed = config.extent == QuadTree::Extent::Fixed;
    std::vector<Record<T>> records;
    std::vector<typename Tree<T>::HandleType> dead;
    int next_id = 0;
    const auto retire = [&](typename Tree<T>::HandleType handle)
    {
        if (dead.size() == 64)
            dead.erase(dead.begin());
        dead.push_back(handle);
    };
    for (unsigned step = 0; step < options.steps; ++step)
    {
        gContext.step = step;
        const std::size_t op = rng.below(200);
        if (records.empty() || (op < 80 && records.size() < options.maxEntries))
        {
            const Box<T> b = generate<T>(rng, scene);
            const Filter filter = randomFilter(rng, seed);
            const auto rank = static_cast<std::uint64_t>(next_id) * 3 + 1;
            const auto inserted = tree.insert(b, next_id, filter, rank);
            CHECK(inserted.ok());
            if (inserted.ok())
                records.push_back({b, next_id++, filter, rank, inserted.handle});
        }
        else if (op < 140)
        {
            auto& record = records[rng.below(records.size())];
            const std::size_t kind = rng.below(8);
            const Box<T> b = kind == 0 ? record.box : kind < 3 ? generate<T>(rng, scene) : nudge(record.box, rng);
            const Status status = tree.update(record.handle, b);
            const Status expected = !fixed || inside(*config.root, b) ? Status::Ok : Status::OutOfBounds;
            CHECK(status == expected);
            if (status == Status::Ok)
                record.box = b;
        }
        else if (op < 150)
        {
            auto& record = records[rng.below(records.size())];
            record.filter = randomFilter(rng, seed, true);
            CHECK(tree.setFilter(record.handle, record.filter) == Status::Ok);
        }
        else if (op < 180)
        {
            const std::size_t index = rng.below(records.size());
            CHECK(tree.remove(records[index].handle) == Status::Ok);
            retire(records[index].handle);
            records[index] = records.back();
            records.pop_back();
        }
        else if (op < 190)
        {
            std::vector<QuadTree::BuildEntry<int, T>> input;
            for (auto& record : records)
            {
                if (sharedFilterSeed(seed))
                    record.filter = Filter{record.filter.category | 8, record.filter.mask | 8};
                input.push_back({record.box, record.id, record.filter, record.rank});
            }
            std::vector<typename Tree<T>::HandleType> output(input.size());
            const bool consume = rng.below(2) == 0;
            const Status status = consume ?
                tree.buildConsume(input, output, {.workspace = &workspace}) :
                tree.build(input, output, {.workspace = &workspace, .transactional = rng.below(2) == 0});
            CHECK(status == Status::Ok);
            for (std::size_t i = 0; i < records.size(); ++i)
            {
                retire(records[i].handle);
                records[i].handle = output[i];
            }
        }
        else if (op < 199)
            CHECK(tree.shrink() == Status::Ok);
        else
        {
            tree.clear();
            for (const auto& record : records)
                retire(record.handle);
            records.clear();
        }
        checkTree(tree);
        for (auto handle : dead)
            CHECK(!tree.valid(handle));
        if (step % 7 == 0 || step + 1 == options.steps)
            checkResults(tree, records, workspace, scene, rng);
        if (gContext.failures != 0)
        {
            std::fprintf(stderr, "reproduce: quadtree-index-fuzz --types=%s --scenes=%s --seed=%llu --steps=%u "
                "--max-entries=%zu\n", gContext.type, gContext.scene, static_cast<unsigned long long>(seed), step + 1,
                options.maxEntries);
            return false;
        }
    }
    return true;
}

template<typename T>
bool runType(const Options& options)
{
    for (Scene scene : options.scenes)
    {
        for (std::uint64_t seed = options.firstSeed; seed < options.firstSeed + options.seeds; ++seed)
            if (!run<T>(seed, scene, options))
                return false;
        std::printf("%-6s %-9s seeds %llu-%llu, %u steps: ok\n", typeName<T>(),
            kSceneNames[static_cast<int>(scene)].data(), static_cast<unsigned long long>(options.firstSeed),
            static_cast<unsigned long long>(options.firstSeed + options.seeds - 1), options.steps);
    }
    return true;
}

std::vector<std::string> split(std::string_view list)
{
    std::vector<std::string> result;
    while (!list.empty())
    {
        const std::size_t comma = list.find(',');
        result.emplace_back(list.substr(0, comma));
        list = comma == std::string_view::npos ? std::string_view{} : list.substr(comma + 1);
    }
    return result;
}

[[noreturn]] void usage()
{
    std::fputs("usage: quadtree-index-fuzz [--quick] [--seeds=N] [--seed=S] [--steps=N] [--max-entries=N]\n"
        "    [--types=int32,int64,float,double] [--scenes=uniform,clustered,mixed,pile,outliers]\n", stderr);
    std::exit(2);
}
} // namespace

int main(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg = argv[i];
        const auto value = [&](std::string_view prefix) -> std::optional<std::string_view>
        {
            if (arg.substr(0, prefix.size()) != prefix)
                return std::nullopt;
            return arg.substr(prefix.size());
        };
        if (arg == "--quick")
        {
            options.seeds = 4;
            options.steps = 400;
            options.maxEntries = 120;
        }
        else if (auto seeds = value("--seeds="))
            options.seeds = std::strtoull(std::string(*seeds).c_str(), nullptr, 10);
        else if (auto seed = value("--seed="))
        {
            options.firstSeed = std::strtoull(std::string(*seed).c_str(), nullptr, 10);
            options.seeds = 1;
        }
        else if (auto steps = value("--steps="))
            options.steps = static_cast<unsigned>(std::strtoul(std::string(*steps).c_str(), nullptr, 10));
        else if (auto entries = value("--max-entries="))
            options.maxEntries = std::strtoull(std::string(*entries).c_str(), nullptr, 10);
        else if (auto types = value("--types="))
            options.types = split(*types);
        else if (auto scenes = value("--scenes="))
        {
            options.scenes.clear();
            for (const auto& name : split(*scenes))
            {
                const auto found = std::find(std::begin(kSceneNames), std::end(kSceneNames), name);
                if (found == std::end(kSceneNames))
                    usage();
                options.scenes.push_back(static_cast<Scene>(found - std::begin(kSceneNames)));
            }
        }
        else
            usage();
    }
    bool ok = true;
    for (const auto& type : options.types)
    {
        if (type == "int32") ok = runType<std::int32_t>(options);
        else if (type == "int64") ok = runType<std::int64_t>(options);
        else if (type == "float") ok = runType<float>(options);
        else if (type == "double") ok = runType<double>(options);
        else usage();
        if (!ok)
            break;
    }
    return ok ? 0 : 1;
}
