// Assertion tests for handle identity and entry storage (QT-FR-06/07/08/10/35,
// QT-NFR-04/07). Optional argument: seed for the randomized test.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "entry-store-test.h"

#ifdef _MSC_VER
#pragma warning(disable: 4324) // Structure padded due to alignment specifier (Aligned)
#endif

using QuadTree::Box;
using QuadTree::Filter;
using QuadTree::Rect;
using QuadTree::Reservation;
using QuadTree::Status;
using QuadTree::detail::EntryStore;
using QuadTree::detail::HandleAccess;

namespace
{
int gFailures = 0;

#define CHECK(cond)                                                                   \
    do                                                                                \
    {                                                                                 \
        if (!(cond))                                                                  \
        {                                                                             \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++gFailures;                                                              \
        }                                                                             \
    } while (0)

// Counts allocations, checks alignment and ownership, and injects failures.
class TestResource final: public QuadTree::Resource
{
public:
    std::size_t allocations = 0;
    std::size_t failAt = 0; // Fail the n-th allocation (1-based), 0 = never
    bool failAll = false;
    std::size_t zeroByteRequests = 0;
    std::size_t misaligned = 0;
    std::size_t foreign = 0;
    std::size_t maxAlignment = 0;

    void* allocate(std::size_t bytes, std::size_t alignment) noexcept override
    {
        ++allocations;
        if (bytes == 0)
            ++zeroByteRequests;
        if (failAll || allocations == failAt)
            return nullptr;

        void* ptr = QuadTree::defaultResource()->allocate(bytes, alignment);
        if (ptr)
        {
            if (reinterpret_cast<std::uintptr_t>(ptr) % alignment != 0)
                ++misaligned;
            maxAlignment = std::max(maxAlignment, alignment);
            mOwned.insert(ptr);
        }
        return ptr;
    }

    void deallocate(void* ptr) noexcept override
    {
        if (!ptr)
            return;
        if (mOwned.erase(ptr) == 0)
            ++foreign;
        QuadTree::defaultResource()->deallocate(ptr);
    }

    std::size_t live() const { return mOwned.size(); }

private:
    std::unordered_set<void*> mOwned;
};

struct Tracked
{
    static inline int alive = 0;
    int value;

    explicit Tracked(int aValue) noexcept: value(aValue) { ++alive; }
    Tracked(const Tracked& other) noexcept: value(other.value) { ++alive; }
    Tracked(Tracked&& other) noexcept: value(other.value)
    {
        other.value = -1;
        ++alive;
    }
    Tracked& operator=(const Tracked&) = delete;
    ~Tracked() { --alive; }
};

struct alignas(64) Aligned
{
    int value;
};

// Test policies
struct LimitedGenerations
{
    static constexpr bool checkHandles = true;
    static constexpr std::uint32_t generationLimit = 2;
    static constexpr std::uint32_t slotLimit = 2;
};

struct SingleUseSlots
{
    static constexpr bool checkHandles = true;
    static constexpr std::uint32_t generationLimit = 0;
};

struct ChurnGenerations
{
    static constexpr bool checkHandles = true;
    static constexpr std::uint32_t generationLimit = 8; // New slots start at generation <= 4
};

struct RetiredCap
{
    static constexpr bool checkHandles = true;
    static constexpr std::uint32_t generationLimit = 0;
    static constexpr std::uint32_t retiredLimit = 3;
};

struct SlotCap
{
    static constexpr bool checkHandles = true;
    static constexpr std::uint32_t generationLimit = 0;
    static constexpr std::uint32_t slotLimit = 8;
};

struct SmallRankProbe
{
    static constexpr bool checkHandles = true;
    static constexpr std::uint32_t rankProbeLimit = 2; // Forces the splay-tree fallback early
};

struct ShortRankEpoch
{
    static constexpr bool checkHandles = true;
    static constexpr std::uint32_t rankEpochLimit = 4; // Bucket reset every 3 clears
};

struct LimitedIdentities
{
    static constexpr bool checkHandles = false;
    static inline int remaining = 0;

    static std::uint64_t acquireIdentity() noexcept
    {
        if (remaining == 0)
            return 0;
        --remaining;
        return QuadTree::detail::acquireModuleIdentity();
    }
};

using StoreB = EntryStore<int, int, QuadTree::Unchecked, TagB>;
using HandleB = StoreB::HandleType;
using StoreChecked = EntryStore<int, int, QuadTree::Checked, TagA>;

constexpr Box<int> kBox{0, 0, 1, 1};

// ---------------------------------------------------------------------------
// Compile-time contract checks
// ---------------------------------------------------------------------------

static_assert(QuadTree::Coordinate<int> && QuadTree::Coordinate<std::int32_t>);
static_assert(QuadTree::Coordinate<std::int64_t> && QuadTree::Coordinate<long long>);
static_assert(QuadTree::Coordinate<float> && QuadTree::Coordinate<double>);
static_assert(!QuadTree::Coordinate<bool> && !QuadTree::Coordinate<char> && !QuadTree::Coordinate<signed char>);
static_assert(!QuadTree::Coordinate<short> && !QuadTree::Coordinate<unsigned> && !QuadTree::Coordinate<std::uint64_t>);
static_assert(!QuadTree::Coordinate<wchar_t> && !QuadTree::Coordinate<char32_t> && !QuadTree::Coordinate<long double>);
static_assert(!QuadTree::Coordinate<const int>);

static_assert(std::is_trivially_copyable_v<HandleA>);
static_assert(sizeof(HandleA) == 16);
static_assert(!std::is_same_v<HandleA, HandleB>);
static_assert(!std::is_convertible_v<HandleB, HandleA>);
static_assert(std::is_nothrow_invocable_r_v<std::size_t, std::hash<HandleA>, const HandleA&>);

static_assert(!std::is_copy_constructible_v<StoreA> && !std::is_copy_assignable_v<StoreA>);
static_assert(std::is_nothrow_move_constructible_v<StoreA> && std::is_nothrow_move_assignable_v<StoreA>);

template<typename S, typename H>
concept AcceptsHandle = requires(const S& store, H handle) { store.valid(handle); };
static_assert(AcceptsHandle<StoreA, HandleA>);
static_assert(!AcceptsHandle<StoreA, HandleB>); // Cross-specialization handles are rejected

template<typename S, typename H>
concept AcceptsRemoval = requires(S& store, H handle) { store.remove(handle); };
static_assert(AcceptsRemoval<StoreA, HandleA>);

template<typename A, typename B>
constexpr bool distinctHandles = !std::is_same_v<typename A::HandleType, typename B::HandleType> &&
    !std::is_convertible_v<typename A::HandleType, typename B::HandleType> &&
    !std::is_convertible_v<typename B::HandleType, typename A::HandleType> &&
    !AcceptsHandle<A, typename B::HandleType> && !AcceptsHandle<B, typename A::HandleType> &&
    !AcceptsRemoval<A, typename B::HandleType> && !AcceptsRemoval<B, typename A::HandleType>;

// Reusing a tag must not erase coordinate, payload or diagnostics-policy identity.
static_assert(distinctHandles<StoreA, StoreB>);
static_assert(distinctHandles<StoreA, EntryStore<float, int, QuadTree::Unchecked, TagA>>);
static_assert(distinctHandles<StoreA, EntryStore<int, long, QuadTree::Unchecked, TagA>>);
static_assert(distinctHandles<StoreA, StoreChecked>);
using VoidTagStore = EntryStore<int, int, QuadTree::Unchecked, void>;
static_assert(distinctHandles<VoidTagStore, EntryStore<float, int, QuadTree::Unchecked, void>>);
static_assert(distinctHandles<VoidTagStore, EntryStore<int, long, QuadTree::Unchecked, void>>);
static_assert(distinctHandles<VoidTagStore, EntryStore<int, int, QuadTree::Checked, void>>);

struct GoodVisitor
{
    QuadTree::Visit operator()(const StoreA::View&) const noexcept { return QuadTree::Continue; }
};
struct ThrowingVisitor
{
    QuadTree::Visit operator()(const StoreA::View&) const { return QuadTree::Continue; }
};
struct BoolVisitor
{
    bool operator()(const StoreA::View&) const noexcept { return true; }
};

template<typename S, typename V>
concept CanVisit = requires(const S& store, V visitor) { store.forEachEntry(visitor); };
static_assert(CanVisit<StoreA, GoodVisitor>);
static_assert(!CanVisit<StoreA, ThrowingVisitor>);
static_assert(!CanVisit<StoreA, BoolVisitor>);

struct ThrowingCopy
{
    ThrowingCopy() = default;
    ThrowingCopy(const ThrowingCopy&) {}
    ThrowingCopy(ThrowingCopy&&) noexcept {}
};
struct ThrowingMove
{
    ThrowingMove(ThrowingMove&&) {}
};

template<typename S, typename P>
concept CanCopyInsert = requires(S& store, const P& payload) { store.insert(Box<int>{}, payload); };
static_assert(CanCopyInsert<StoreA, int>);
static_assert(!CanCopyInsert<EntryStore<int, ThrowingCopy, QuadTree::Unchecked, TagA>, ThrowingCopy>);

static_assert(QuadTree::StorablePayload<std::unique_ptr<int>> && QuadTree::StorablePayload<Aligned>);
static_assert(!QuadTree::StorablePayload<ThrowingMove> && !QuadTree::StorablePayload<int[4]>);
static_assert(!QuadTree::StorablePayload<const int> && !QuadTree::StorablePayload<int&>);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

struct Rng
{
    std::uint64_t state;

    std::uint64_t next()
    {
        state += 0x9e3779b97f4a7c15u;
        std::uint64_t z = state;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9u;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebu;
        return z ^ (z >> 31);
    }

    std::uint32_t below(std::uint32_t n) { return static_cast<std::uint32_t>(next() % n); }
    int range(int lo, int hi) { return lo + static_cast<int>(below(static_cast<std::uint32_t>(hi - lo + 1))); }
};

struct Record
{
    Box<int> box;
    int payload;
    Filter filter;
    std::optional<std::uint64_t> rank;
};

using Oracle = std::unordered_map<HandleA, Record>;

void verify(const StoreA& store, const Oracle& live, const std::vector<HandleA>& dead)
{
    CHECK(store.size() == live.size());
    CHECK(store.empty() == live.empty());
    for (const auto& [handle, record] : live)
    {
        CHECK(store.valid(handle));
        CHECK(store.rect(handle) == record.box);
        CHECK(store.payload(handle) == record.payload);
        CHECK(store.filter(handle) == record.filter);
        CHECK(store.rank(handle) == record.rank);
    }
    for (const auto& handle : dead)
        CHECK(!store.valid(handle));

    std::unordered_set<HandleA> seen;
    bool consistent = true;
    const Status status = store.forEachEntry([&](const StoreA::View& view) noexcept
    {
        const auto it = live.find(view.handle);
        if (it == live.end() || !seen.insert(view.handle).second || view.rect != it->second.box ||
            view.payload != it->second.payload || view.filter != it->second.filter || view.rank != it->second.rank)
            consistent = false;
        return QuadTree::Continue;
    });
    CHECK(status == Status::Ok);
    CHECK(consistent);
    CHECK(seen.size() == live.size());
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testBasics()
{
    TestResource resource;
    {
        StoreA store(&resource);
        CHECK(store.empty() && store.size() == 0);
        CHECK(store.resource() == &resource);
        CHECK(resource.allocations == 0); // Empty storage allocates nothing
        CHECK(!store.valid(HandleA{}));

        // Stored geometry is a snapshot of the submitted value.
        Box<int> client{1, 2, 3, 4};
        const auto a = store.insert(client, 10);
        CHECK(a.ok());
        client.maxX = 100;
        CHECK(store.rect(a.handle) == (Box<int>{1, 2, 3, 4}));
        CHECK(store.payload(a.handle) == 10);
        CHECK(store.filter(a.handle) == Filter{});
        CHECK(!store.rank(a.handle));

        // Same payload twice gives two independent entries.
        const auto b = store.insert(Rect<int>{5, 6, 7, 8}, 10, Filter{1, 2}, 0u);
        CHECK(b.ok());
        CHECK(a.handle != b.handle);
        CHECK(store.size() == 2);
        CHECK(store.rect(b.handle) == (Box<int>{5, 6, 12, 14}));
        CHECK(store.filter(b.handle) == (Filter{1, 2}));
        CHECK(store.rank(b.handle) == std::optional<std::uint64_t>(0));

        // Update keeps handle, payload, filter and rank; failure keeps old geometry.
        CHECK(store.update(b.handle, Box<int>{3, 3, 3, 3}) == Status::Ok);
        CHECK(store.rect(b.handle) == (Box<int>{3, 3, 3, 3}));
        CHECK(store.update(b.handle, Box<int>{2, 0, 1, 0}) == Status::InvalidGeometry);
        CHECK(store.update(b.handle, Rect<int>{0, 0, -1, 0}) == Status::InvalidGeometry);
        CHECK(store.rect(b.handle) == (Box<int>{3, 3, 3, 3}));
        CHECK(store.update(b.handle, Rect<int>{1, 1, 2, 2}) == Status::Ok);
        CHECK(store.rect(b.handle) == (Box<int>{1, 1, 3, 3}));
        CHECK(store.valid(b.handle) && store.payload(b.handle) == 10);
        CHECK(store.filter(b.handle) == (Filter{1, 2}) && store.rank(b.handle) == std::optional<std::uint64_t>(0));

        CHECK(store.setFilter(b.handle, Filter{0, 0}) == Status::Ok);
        CHECK(store.filter(b.handle) == (Filter{0, 0}));
        CHECK(store.rect(b.handle) == (Box<int>{1, 1, 3, 3}));

        store.payload(a.handle) = 11;
        CHECK(store.payload(a.handle) == 11);

        // Removal invalidates only that handle; slot reuse gets a new generation.
        CHECK(store.remove(a.handle) == Status::Ok);
        CHECK(!store.valid(a.handle) && store.valid(b.handle));
        CHECK(store.size() == 1);
        const auto c = store.insert(kBox, 12);
        CHECK(c.ok());
        CHECK(HandleAccess::slot(c.handle) == HandleAccess::slot(a.handle));
        CHECK(HandleAccess::generation(c.handle) != HandleAccess::generation(a.handle));
        CHECK(!store.valid(a.handle) && store.valid(c.handle));

        const std::hash<HandleA> hasher;
        HandleA copy = c.handle;
        CHECK(copy == c.handle && hasher(copy) == hasher(c.handle));
        CHECK(hasher(c.handle) != hasher(b.handle));
    }
    CHECK(resource.live() == 0);
    CHECK(resource.foreign == 0);
    CHECK(resource.zeroByteRequests == 0);
}

void testGeometry()
{
    {
        EntryStore<int, int, QuadTree::Unchecked, TagA> store;
        constexpr int max = std::numeric_limits<int>::max();
        constexpr int min = std::numeric_limits<int>::min();
        CHECK(store.insert(Rect<int>{max - 1, 1, 5, 2}, 0).status == Status::InvalidGeometry);
        CHECK(store.insert(Rect<int>{1, max - 1, 2, 5}, 0).status == Status::InvalidGeometry);
        CHECK(store.insert(Rect<int>{0, 0, -1, 1}, 0).status == Status::InvalidGeometry);
        CHECK(store.insert(Rect<int>{0, 0, 1, -1}, 0).status == Status::InvalidGeometry);
        CHECK(store.insert(Box<int>{1, 0, 0, 0}, 0).status == Status::InvalidGeometry);
        CHECK(store.insert(Box<int>{0, 1, 0, 0}, 0).status == Status::InvalidGeometry);
        CHECK(store.empty());

        const auto full = store.insert(Box<int>{min, min, max, max}, 1);
        CHECK(full.ok() && store.rect(full.handle) == (Box<int>{min, min, max, max}));
        const auto edge = store.insert(Rect<int>{max - 5, min, 5, 0}, 2);
        CHECK(edge.ok() && store.rect(edge.handle) == (Box<int>{max - 5, min, max, min}));
        const auto wide = store.insert(Rect<int>{min, -1, max, 1}, 3);
        CHECK(wide.ok() && store.rect(wide.handle) == (Box<int>{min, -1, -1, 0}));
        CHECK(store.size() == 3);
    }
    {
        EntryStore<std::int64_t, int, QuadTree::Unchecked, TagA> store;
        constexpr std::int64_t max = std::numeric_limits<std::int64_t>::max();
        constexpr std::int64_t min = std::numeric_limits<std::int64_t>::min();
        // Section 2 overflow reproducer: item {INT64_MAX-1, 1, 5, 2}.
        CHECK(store.insert(Rect<std::int64_t>{max - 1, 1, 5, 2}, 0).status == Status::InvalidGeometry);
        const auto full = store.insert(Box<std::int64_t>{min, min, max, max}, 1);
        CHECK(full.ok() && store.rect(full.handle) == (Box<std::int64_t>{min, min, max, max}));
        const auto far = store.insert(Rect<std::int64_t>{max - 1, 0, 1, 0}, 2);
        CHECK(far.ok() && store.rect(far.handle).maxX == max);
    }
    {
        EntryStore<float, int, QuadTree::Unchecked, TagA> store;
        constexpr float inf = std::numeric_limits<float>::infinity();
        constexpr float nan = std::numeric_limits<float>::quiet_NaN();
        constexpr float max = std::numeric_limits<float>::max();
        constexpr float denorm = std::numeric_limits<float>::denorm_min();
        CHECK(store.insert(Rect<float>{nan, 0, 1, 1}, 0).status == Status::InvalidGeometry);
        CHECK(store.insert(Rect<float>{0, 0, inf, 1}, 0).status == Status::InvalidGeometry);
        CHECK(store.insert(Rect<float>{max, 0, max, 1}, 0).status == Status::InvalidGeometry); // Endpoint overflows
        CHECK(store.insert(Rect<float>{1e30f, 0, 1, 1}, 0).status == Status::InvalidGeometry); // Collapsed extent
        CHECK(store.insert(Rect<float>{0, 1e30f, 1, 1}, 0).status == Status::InvalidGeometry);
        CHECK(store.insert(Rect<float>{0, 0, -1, 1}, 0).status == Status::InvalidGeometry);
        CHECK(store.insert(Box<float>{nan, 0, 1, 1}, 0).status == Status::InvalidGeometry);
        CHECK(store.insert(Box<float>{-inf, 0, 0, 0}, 0).status == Status::InvalidGeometry);
        CHECK(store.insert(Box<float>{0, 0, 0, inf}, 0).status == Status::InvalidGeometry);
        CHECK(store.insert(Box<float>{1, 0, 0, 0}, 0).status == Status::InvalidGeometry);
        CHECK(store.empty());

        const auto span = store.insert(Box<float>{-max, -max, max, max}, 1); // Span overflows; edges finite
        CHECK(span.ok() && store.rect(span.handle) == (Box<float>{-max, -max, max, max}));
        const auto tiny = store.insert(Rect<float>{0, 0, denorm, denorm}, 2);
        CHECK(tiny.ok() && store.rect(tiny.handle).maxX == denorm);
        const auto point = store.insert(Rect<float>{1e30f, 0, 0, 0}, 3);
        CHECK(point.ok() && store.rect(point.handle) == (Box<float>{1e30f, 0, 1e30f, 0}));

        const auto zero = store.insert(Box<float>{-0.0f, -0.0f, 0.0f, -0.0f}, 4);
        CHECK(zero.ok());
        const Box<float>& stored = store.rect(zero.handle);
        CHECK(!std::signbit(stored.minX) && !std::signbit(stored.minY) && !std::signbit(stored.maxY));
        const auto zero_rect = store.insert(Rect<float>{-0.0f, -0.0f, -0.0f, 0.0f}, 5);
        CHECK(zero_rect.ok());
        const Box<float>& stored_rect = store.rect(zero_rect.handle);
        CHECK(!std::signbit(stored_rect.minX) && !std::signbit(stored_rect.maxX) && !std::signbit(stored_rect.minY));

        // Section 2 float reproducer geometry is kept exactly.
        const float u = std::nextafter(1.0f, 2.0f) - 1.0f;
        const auto item = store.insert(Rect<float>{1 + 4 * u, 1, u, 1}, 6);
        CHECK(item.ok() && store.rect(item.handle).maxX == 1 + 5 * u);

        // Box input is stored without endpoint arithmetic.
        const auto exact = store.insert(Box<float>{0.1f, 0, 0.3f, 1}, 7);
        CHECK(exact.ok() && store.rect(exact.handle).maxX == 0.3f);
        CHECK(store.size() == 7);
    }
    {
        EntryStore<double, int, QuadTree::Unchecked, TagA> store;
        CHECK(store.insert(Rect<double>{1e300, 0, 1e-300, 0}, 0).status == Status::InvalidGeometry);
        CHECK(store.insert(Rect<double>{0, 0, std::numeric_limits<double>::denorm_min(), 0}, 0).ok());
        CHECK(store.size() == 1);
    }
}

void testRanks()
{
    {
        StoreA store;
        constexpr std::uint64_t max = std::numeric_limits<std::uint64_t>::max();
        const auto a = store.insert(kBox, 1, {}, 0u);
        const auto b = store.insert(kBox, 2, {}, max);
        CHECK(a.ok() && b.ok());
        const auto dup = store.insert(kBox, 3, {}, 0u);
        CHECK(dup.status == Status::DuplicateRank);
        CHECK(store.insert(kBox, 3, {}, max).status == Status::DuplicateRank);
        CHECK(store.size() == 2 && store.valid(a.handle) && store.valid(b.handle));
        CHECK(store.rank(b.handle) == std::optional<std::uint64_t>(max));
        CHECK(store.insert(kBox, 4).ok()); // No rank: never a duplicate
        CHECK(store.insert(kBox, 5).ok());

        CHECK(store.remove(a.handle) == Status::Ok);
        const auto reused = store.insert(kBox, 6, {}, 0u);
        CHECK(reused.ok() && store.rank(reused.handle) == std::optional<std::uint64_t>(0));
    }

    // Rank set stress against an ordered oracle; clustered and spread ranks.
    for (const std::uint64_t scale : {std::uint64_t(1), std::uint64_t(0x100000001u)})
    {
        StoreA store;
        std::map<std::uint64_t, HandleA> ranks;
        Rng rng{scale};
        for (int step = 0; step < 20000; ++step)
        {
            const std::uint64_t rank = rng.below(512) * scale;
            const auto it = ranks.find(rank);
            if (rng.below(3) != 0)
            {
                const auto result = store.insert(kBox, step, {}, rank);
                CHECK(result.status == (it == ranks.end() ? Status::Ok : Status::DuplicateRank));
                if (result.ok())
                    ranks.emplace(rank, result.handle);
            }
            else if (it != ranks.end())
            {
                CHECK(store.remove(it->second) == Status::Ok);
                ranks.erase(it);
            }
        }
        CHECK(store.size() == ranks.size());
        for (const auto& [rank, handle] : ranks)
            CHECK(store.valid(handle) && store.rank(handle) == std::optional<std::uint64_t>(rank));
    }
}

// Inverse of QuadTree::detail::mix64 (splitmix64 finalizer), to build ranks
// with chosen hashes.
constexpr std::uint64_t inverseOdd(std::uint64_t c)
{
    std::uint64_t x = c;
    for (int i = 0; i < 6; ++i)
        x *= 2 - c * x;
    return x;
}

constexpr std::uint64_t unmix64(std::uint64_t x)
{
    x ^= x >> 31 ^ x >> 62;
    x *= inverseOdd(0x94d049bb133111ebu);
    x ^= x >> 27 ^ x >> 54;
    x *= inverseOdd(0xbf58476d1ce4e5b9u);
    x ^= x >> 30 ^ x >> 60;
    return x;
}

static_assert(QuadTree::detail::mix64(unmix64(0x123456789abcdefu)) == 0x123456789abcdefu);

// Distinct valid ranks whose hashes share the low 32 bits: one home bucket at
// every table capacity up to 2^32.
constexpr std::uint64_t collidingRank(std::uint64_t k)
{
    return unmix64(k << 32);
}

std::uint64_t log2Ceil(std::size_t n)
{
    std::uint64_t bits = 0;
    while ((std::size_t(1) << bits) < n)
        ++bits;
    return bits;
}

void testRankCollisions()
{
    // Colliding ranks switch the rank set to the splay tree; each growth retries
    // hash mode and converts again (at most once per doubling). Total work
    // stays O(n log n); hash-only probing would need about n^2 / 2 probes.
    for (const std::size_t n : {std::size_t(2000), std::size_t(8000)})
    {
        StoreA store;
        std::vector<HandleA> handles;
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto result = store.insert(kBox, int(k), {}, collidingRank(k + 1));
            CHECK(result.ok());
            handles.push_back(result.handle);
        }
        const auto stats = store.rankStats();
        CHECK(stats.tree && stats.conversions >= 1 && stats.conversions <= log2Ceil(n) + 1);
        CHECK(stats.probes <= 8 * n * log2Ceil(n));
        CHECK(store.insert(kBox, -1, {}, collidingRank(1)).status == Status::DuplicateRank);
        CHECK(store.insert(kBox, -1, {}, collidingRank(n)).status == Status::DuplicateRank);
        CHECK(store.size() == n);

        // Remove every other entry; its rank becomes free, the others stay taken.
        for (std::size_t k = 0; k < n; k += 2)
            CHECK(store.remove(handles[k]) == Status::Ok);
        for (std::size_t k = 0; k < n; ++k)
        {
            const auto result = store.insert(kBox, -1, {}, collidingRank(k + 1));
            CHECK(result.status == (k % 2 == 0 ? Status::Ok : Status::DuplicateRank));
            if (k % 2 != 0)
                CHECK(store.rank(handles[k]) == std::optional<std::uint64_t>(collidingRank(k + 1)));
        }
        CHECK(store.size() == n);
    }

    // Ordinary ranks stay in hash mode with O(1) probes per operation.
    {
        StoreA store;
        constexpr std::size_t n = 8000;
        for (std::size_t k = 0; k < n; ++k)
            CHECK(store.insert(kBox, int(k), {}, std::uint64_t(k)).ok());
        const auto stats = store.rankStats();
        CHECK(!stats.tree && stats.conversions == 0);
        CHECK(stats.probes <= 8 * n);
    }

    // A rehash (shrink) rebuilds from live metadata and leaves the tree when
    // the colliding ranks are gone.
    {
        StoreA store;
        std::vector<HandleA> colliding;
        for (std::size_t k = 0; k < 300; ++k)
            colliding.push_back(store.insert(kBox, 0, {}, collidingRank(k + 1)).handle);
        for (std::size_t k = 0; k < 300; ++k)
            CHECK(store.insert(kBox, 1, {}, std::uint64_t(k)).ok());
        CHECK(store.rankStats().tree);
        for (const auto& handle : colliding)
            CHECK(store.remove(handle) == Status::Ok);
        CHECK(store.shrink() == Status::Ok);
        CHECK(!store.rankStats().tree);
        for (std::size_t k = 0; k < 300; ++k)
            CHECK(store.insert(kBox, 2, {}, std::uint64_t(k)).status == Status::DuplicateRank);
    }
}

void testRankClearCost()
{
    // clear() does not touch the retained rank cells.
    {
        StoreA store;
        CHECK(store.reserve(Reservation{.entries = 1u << 20, .rankedEntries = 1u << 20}) == Status::Ok);
        const std::uint64_t resets = store.rankStats().cellResets; // Paid by the allocation
        for (int i = 0; i < 1000; ++i)
        {
            CHECK(store.insert(kBox, i, {}, 7u).ok());
            store.clear();
        }
        CHECK(store.rankStats().cellResets == resets);
    }

    // In tree mode too; the cleared tree accepts the same ranks again.
    {
        StoreA store;
        for (std::size_t k = 0; k < 300; ++k)
            CHECK(store.insert(kBox, 0, {}, collidingRank(k + 1)).ok());
        CHECK(store.rankStats().tree);
        const std::uint64_t resets = store.rankStats().cellResets;
        for (int round = 0; round < 3; ++round)
        {
            store.clear();
            for (std::size_t k = 0; k < 300; ++k)
                CHECK(store.insert(kBox, 0, {}, collidingRank(k + 1)).ok());
            CHECK(store.insert(kBox, 0, {}, collidingRank(1)).status == Status::DuplicateRank);
        }
        CHECK(store.rankStats().cellResets == resets);
    }

    // Epoch wrap resets every bucket; stale buckets never reappear. With an
    // epoch limit of 4 the third clear wraps the epoch back to 1. Ranks 1000..
    // are written in epoch 1 and never overwritten, so without the reset they
    // would count as present again right after the wrap.
    {
        EntryStore<int, int, ShortRankEpoch, TagA> store;
        for (std::uint64_t k = 0; k < 100; ++k)
            CHECK(store.insert(kBox, 0, {}, 1000 + k).ok());
        const std::uint64_t resets = store.rankStats().cellResets;
        store.clear(); // Epoch 2
        for (std::uint64_t k = 0; k < 100; ++k)
            CHECK(store.insert(kBox, 0, {}, k).ok());
        store.clear(); // Epoch 3
        store.clear(); // Wrap: all buckets reset, epoch 1
        CHECK(store.rankStats().cellResets > resets);
        for (std::uint64_t k = 0; k < 100; ++k)
            CHECK(store.insert(kBox, 0, {}, 1000 + k).ok());
        CHECK(store.insert(kBox, 0, {}, std::uint64_t(1005)).status == Status::DuplicateRank);
    }
}

// Randomized rank operations against an ordered oracle with a two-bucket probe
// limit, so hash mode, conversion, tree mode and rehashes all occur.
template<typename Policy>
void rankOracle(std::uint64_t seed, bool adversarial, bool requireConversion)
{
    using Store = EntryStore<int, int, Policy, TagA>;
    Store store;
    std::map<std::uint64_t, typename Store::HandleType> ranks;
    std::vector<typename Store::HandleType> unranked;
    Rng rng{seed};
    std::uint64_t conversions = 0;
    for (int step = 0; step < 20000; ++step)
    {
        const std::uint32_t op = rng.below(100);
        std::uint64_t rank = rng.below(600);
        if (adversarial && rng.below(2) == 0)
            rank = collidingRank(rank + 1);
        if (op < 50)
        {
            const bool with_rank = rng.below(8) != 0;
            const auto result = with_rank ? store.insert(kBox, step, {}, rank) : store.insert(kBox, step);
            if (!with_rank)
            {
                CHECK(result.ok());
                unranked.push_back(result.handle);
            }
            else
            {
                CHECK(result.status == (ranks.count(rank) != 0 ? Status::DuplicateRank : Status::Ok));
                if (result.ok())
                    ranks.emplace(rank, result.handle);
            }
        }
        else if (op < 85)
        {
            const auto it = ranks.lower_bound(rank);
            if (it != ranks.end())
            {
                CHECK(store.remove(it->second) == Status::Ok);
                ranks.erase(it);
            }
            else if (!unranked.empty())
            {
                CHECK(store.remove(unranked.back()) == Status::Ok);
                unranked.pop_back();
            }
        }
        else if (op < 87)
        {
            store.clear();
            ranks.clear();
            unranked.clear();
        }
        else if (op < 92)
            CHECK(store.shrink() == Status::Ok);
        else if (op < 96)
        {
            const std::size_t entries = rng.below(static_cast<std::uint32_t>(2 * store.size() + 8));
            CHECK(store.reserve(Reservation{.entries = entries, .rankedEntries = entries}) == Status::Ok);
        }
        else
        {
            Store moved(std::move(store));
            store = std::move(moved);
        }

        CHECK(store.size() == ranks.size() + unranked.size());
        if (step % 251 == 0)
            for (const auto& [value, handle] : ranks)
                CHECK(store.valid(handle) && store.rank(handle) == std::optional<std::uint64_t>(value));
        conversions = std::max(conversions, store.rankStats().conversions);
    }
    for (const auto& [value, handle] : ranks)
    {
        CHECK(store.rank(handle) == std::optional<std::uint64_t>(value));
        CHECK(store.insert(kBox, 0, {}, value).status == Status::DuplicateRank);
    }
    CHECK(conversions > 0 || !requireConversion);
}

void testRankOracle(std::uint64_t seed)
{
    rankOracle<SmallRankProbe>(seed, false, true);
    rankOracle<SmallRankProbe>(seed + 1, true, true);
    rankOracle<QuadTree::Checked>(seed + 2, true, false); // Default limit: correctness only
}

void testForeignAndAddressReuse()
{
    // Same specialization, identical operation sequences: same slot and
    // generation, different owner.
    StoreA first;
    StoreA second;
    const auto a = first.insert(kBox, 1);
    const auto b = second.insert(kBox, 1);
    CHECK(HandleAccess::slot(a.handle) == HandleAccess::slot(b.handle));
    CHECK(HandleAccess::generation(a.handle) == HandleAccess::generation(b.handle));
    CHECK(a.handle != b.handle);
    CHECK(first.valid(a.handle) && !first.valid(b.handle));
    CHECK(second.valid(b.handle) && !second.valid(a.handle));

    // Destruction and reconstruction at the same address.
    alignas(StoreA) unsigned char buffer[sizeof(StoreA)];
    auto* store = ::new (static_cast<void*>(buffer)) StoreA();
    const auto old_handle = store->insert(kBox, 1).handle;
    store->~StoreA();
    store = ::new (static_cast<void*>(buffer)) StoreA();
    const auto new_handle = store->insert(kBox, 1).handle;
    CHECK(store->valid(new_handle) && !store->valid(old_handle));
    CHECK(new_handle != old_handle);
    store->~StoreA();
}

void testModuleIdentity()
{
    std::vector<std::uint64_t> owners;
    for (int round = 0; round < 4; ++round)
    {
        StoreA store;
        owners.push_back(HandleAccess::owner(store.insert(kBox, round).handle));
        const auto others = tu2Owners(3);
        owners.insert(owners.end(), others.begin(), others.end());
    }
    std::sort(owners.begin(), owners.end());
    CHECK(std::adjacent_find(owners.begin(), owners.end()) == owners.end());
    CHECK(std::find(owners.begin(), owners.end(), std::uint64_t(0)) == owners.end());

    StoreA store;
    const HandleA from_tu2 = tu2Insert(store, 5);
    CHECK(store.valid(from_tu2) && tu2Valid(store, from_tu2));
    StoreA other;
    const auto foreign = other.insert(kBox, 6).handle;
    CHECK(!tu2Valid(store, foreign));
}

void testClear()
{
    TestResource resource;
    StoreA store(&resource);
    std::vector<HandleA> handles;
    for (int i = 0; i < 100; ++i)
        handles.push_back(store.insert(Box<int>{i, i, i + 1, i + 1}, i, {}, std::uint64_t(i)).handle);

    const std::size_t bytes = store.retainedBytes();
    const std::size_t allocations = resource.allocations;
    store.clear();
    CHECK(store.empty());
    CHECK(store.retainedBytes() == bytes);
    for (const auto& handle : handles)
        CHECK(!store.valid(handle));

    // Same slots and generations again, new identity; ranks are free; no allocation.
    std::vector<HandleA> again;
    for (int i = 0; i < 100; ++i)
    {
        const auto result = store.insert(kBox, i, {}, std::uint64_t(i));
        CHECK(result.ok());
        again.push_back(result.handle);
    }
    CHECK(resource.allocations == allocations);
    CHECK(HandleAccess::slot(again[0]) == HandleAccess::slot(handles[0]));
    CHECK(HandleAccess::generation(again[0]) == HandleAccess::generation(handles[0]));
    for (std::size_t i = 0; i < handles.size(); ++i)
        CHECK(!store.valid(handles[i]) && store.valid(again[i]));
}

void testPayloadLifetime()
{
    CHECK(Tracked::alive == 0);
    {
        Tracked source(7);
        {
            EntryStore<int, Tracked, QuadTree::Unchecked, TagA> store;
            std::vector<EntryStore<int, Tracked, QuadTree::Unchecked, TagA>::HandleType> handles;
            for (int i = 0; i < 50; ++i)
                handles.push_back(store.insert(kBox, Tracked(i)).handle);
            CHECK(Tracked::alive == 51);
            for (int i = 0; i < 10; ++i)
                CHECK(store.remove(handles[std::size_t(i) * 3]) == Status::Ok);
            CHECK(Tracked::alive == 41);
            for (int i = 0; i < 50; ++i)
                if (i % 3 != 0 || i >= 30)
                    CHECK(store.payload(handles[std::size_t(i)]).value == i);

            const auto copy = store.insert(kBox, source);
            CHECK(copy.ok() && store.payload(copy.handle).value == 7 && source.value == 7);
            CHECK(Tracked::alive == 42);
            store.clear();
            CHECK(Tracked::alive == 1);
            for (int i = 0; i < 5; ++i)
                CHECK(store.insert(kBox, Tracked(i)).ok());
            CHECK(Tracked::alive == 6);
        }
        CHECK(Tracked::alive == 1);
    }
    CHECK(Tracked::alive == 0);

    // A payload argument that refers to stored storage survives growth.
    {
        EntryStore<int, Tracked, QuadTree::Unchecked, TagA> store;
        const auto first = store.insert(kBox, Tracked(42));
        while (store.size() < store.capacity())
            CHECK(store.insert(kBox, Tracked(0)).ok());
        const auto alias = store.insert(kBox, store.payload(first.handle));
        CHECK(alias.ok() && store.size() > 1);
        CHECK(store.payload(alias.handle).value == 42 && store.payload(first.handle).value == 42);
    }
    CHECK(Tracked::alive == 0);

    // Move-only payloads.
    {
        EntryStore<int, std::unique_ptr<int>, QuadTree::Unchecked, TagA> store;
        std::vector<EntryStore<int, std::unique_ptr<int>, QuadTree::Unchecked, TagA>::HandleType> handles;
        for (int i = 0; i < 100; ++i)
            handles.push_back(store.insert(kBox, std::make_unique<int>(i)).handle);
        for (std::size_t i = 0; i < handles.size(); i += 2)
            CHECK(store.remove(handles[i]) == Status::Ok);
        for (std::size_t i = 1; i < handles.size(); i += 2)
            CHECK(*store.payload(handles[i]) == int(i));
        CHECK(store.shrink() == Status::Ok);
        for (std::size_t i = 1; i < handles.size(); i += 2)
            CHECK(*store.payload(handles[i]) == int(i));
    }

    // Over-aligned payloads.
    {
        TestResource resource;
        EntryStore<double, Aligned, QuadTree::Unchecked, TagA> store(&resource);
        std::vector<EntryStore<double, Aligned, QuadTree::Unchecked, TagA>::HandleType> handles;
        for (int i = 0; i < 100; ++i)
            handles.push_back(store.insert(Box<double>{0, 0, 1, 1}, Aligned{i}).handle);
        for (std::size_t i = 0; i < handles.size(); ++i)
        {
            const Aligned& payload = store.payload(handles[i]);
            CHECK(reinterpret_cast<std::uintptr_t>(&payload) % 64 == 0 && payload.value == int(i));
        }
        CHECK(resource.misaligned == 0 && resource.maxAlignment >= 64);
    }
}

void testForEachEntry()
{
    StoreA store;
    CHECK(store.forEachEntry([](const StoreA::View&) noexcept { return QuadTree::Stop; }) == Status::Ok);

    std::vector<HandleA> handles;
    for (int i = 0; i < 10; ++i)
    {
        const std::optional<std::uint64_t> rank =
            i % 2 == 0 ? std::optional<std::uint64_t>(std::uint64_t(i)) : std::nullopt;
        handles.push_back(store.insert(Box<int>{i, 0, i, 0}, i, Filter{std::uint64_t(i), 1}, rank).handle);
    }
    CHECK(store.remove(handles[3]) == Status::Ok);
    CHECK(store.remove(handles[0]) == Status::Ok);

    int visited = 0;
    bool consistent = true;
    CHECK(store.forEachEntry([&](const StoreA::View& view) noexcept
    {
        ++visited;
        const int i = view.payload;
        if (!store.valid(view.handle) || view.handle != handles[std::size_t(i)] || view.rect != (Box<int>{i, 0, i, 0}) ||
            view.filter != (Filter{std::uint64_t(i), 1}) || view.rank != store.rank(view.handle))
            consistent = false;
        return QuadTree::Continue;
    }) == Status::Ok);
    CHECK(visited == 8 && consistent);

    visited = 0;
    CHECK(store.forEachEntry([&](const StoreA::View&) noexcept
    {
        return ++visited == 3 ? QuadTree::Stop : QuadTree::Continue;
    }) == Status::Stopped);
    CHECK(visited == 3);

    // Identical operation sequences visit in identical order.
    auto order = [](const StoreA& source)
    {
        std::vector<int> payloads;
        CHECK(source.forEachEntry([&](const StoreA::View& view) noexcept
        {
            payloads.push_back(view.payload);
            return QuadTree::Continue;
        }) == Status::Ok);
        return payloads;
    };
    StoreA replay;
    std::vector<HandleA> replay_handles;
    for (int i = 0; i < 10; ++i)
    {
        const std::optional<std::uint64_t> rank =
            i % 2 == 0 ? std::optional<std::uint64_t>(std::uint64_t(i)) : std::nullopt;
        replay_handles.push_back(replay.insert(Box<int>{i, 0, i, 0}, i, Filter{std::uint64_t(i), 1}, rank).handle);
    }
    CHECK(replay.remove(replay_handles[3]) == Status::Ok);
    CHECK(replay.remove(replay_handles[0]) == Status::Ok);
    CHECK(order(store) == order(replay));
}

void testMoves()
{
    TestResource resource_a;
    TestResource resource_b;
    {
        StoreA a(&resource_a);
        StoreA b(&resource_b);
        const auto ha = a.insert(kBox, 1, {}, 5u).handle;
        const auto hb = b.insert(kBox, 2).handle;
        const std::size_t allocations_a = resource_a.allocations;
        const std::size_t allocations_b = resource_b.allocations;

        // Move assignment: destination storage released through its old resource.
        b = std::move(a);
        CHECK(resource_b.live() == 0 && resource_b.foreign == 0);
        CHECK(resource_a.allocations == allocations_a && resource_b.allocations == allocations_b);
        CHECK(b.resource() == &resource_a);
        CHECK(b.valid(ha) && !b.valid(hb) && b.payload(ha) == 1 && b.rank(ha) == std::optional<std::uint64_t>(5));

        // Moved-from source: empty, queryable, reusable with its own resource.
        CHECK(a.empty() && !a.valid(ha) && a.resource() == &resource_a);
        CHECK(a.forEachEntry(GoodVisitor{}) == Status::Ok);
        const auto ha2 = a.insert(kBox, 3, {}, 5u);
        CHECK(ha2.ok() && a.valid(ha2.handle) && !a.valid(ha));
        CHECK(!b.valid(ha2.handle));

        // Self-move is a no-op.
        StoreA& alias = b;
        b = std::move(alias);
        CHECK(b.valid(ha) && b.size() == 1);

        // Move construction transfers state without allocation.
        const std::size_t before = resource_a.allocations;
        StoreA c(std::move(b));
        CHECK(resource_a.allocations == before);
        CHECK(c.valid(ha) && !b.valid(ha) && b.empty() && c.resource() == &resource_a);
        CHECK(c.insert(kBox, 4, {}, 5u).status == Status::DuplicateRank); // Rank set moved too
    }
    CHECK(resource_a.live() == 0 && resource_a.foreign == 0);
    CHECK(resource_b.live() == 0 && resource_b.foreign == 0);
}

void testReserveAndHardCapacity()
{
    {
        TestResource resource;
        StoreA store(&resource, true);
        CHECK(store.insert(kBox, 0).status == Status::CapacityExhausted);
        CHECK(store.empty() && resource.allocations == 0);

        CHECK(store.reserve(Reservation{.entries = 100, .rankedEntries = 50}) == Status::Ok);
        const std::size_t allocations = resource.allocations;
        std::vector<HandleA> handles;
        for (int i = 0; i < 100; ++i)
        {
            const std::optional<std::uint64_t> rank =
                i < 50 ? std::optional<std::uint64_t>(std::uint64_t(i)) : std::nullopt;
            const auto result = store.insert(kBox, i, {}, rank);
            CHECK(result.ok());
            handles.push_back(result.handle);
        }
        CHECK(store.insert(kBox, 100).status == Status::CapacityExhausted);
        CHECK(store.size() == 100);

        // Churn within capacity allocates nothing.
        Rng rng{7};
        for (int step = 0; step < 2000; ++step)
        {
            const std::size_t i = rng.below(100);
            const std::optional<std::uint64_t> rank = store.rank(handles[i]);
            CHECK(store.remove(handles[i]) == Status::Ok);
            const auto result = store.insert(Box<int>{step, 0, step, 0}, step, {}, rank);
            CHECK(result.ok());
            handles[i] = result.handle;
        }
        CHECK(resource.allocations == allocations);
        for (const auto& handle : handles)
            CHECK(store.valid(handle));

        CHECK(store.reserve(Reservation{.entries = std::numeric_limits<std::size_t>::max()}) ==
            Status::SizeLimitExceeded);
        CHECK(store.size() == 100);
    }
    {
        TestResource resource;
        StoreA store(&resource);
        CHECK(store.reserve(Reservation{.entries = 1000, .rankedEntries = 1000}) == Status::Ok);
        const std::size_t allocations = resource.allocations;
        for (int i = 0; i < 1000; ++i)
            CHECK(store.insert(kBox, i, {}, std::uint64_t(i) * 7919u).ok());
        CHECK(resource.allocations == allocations);
        CHECK(store.reserve(Reservation{.entries = 10}) == Status::Ok); // Never shrinks
        CHECK(store.capacity() >= 1000);
    }
}

void testShrink()
{
    TestResource resource;
    StoreA store(&resource);
    std::vector<HandleA> handles;
    for (int i = 0; i < 100; ++i)
        handles.push_back(store.insert(kBox, i, {}, std::uint64_t(i)).handle);
    for (std::size_t i = 50; i < 100; ++i)
        CHECK(store.remove(handles[i]) == Status::Ok);
    CHECK(store.remove(handles[10]) == Status::Ok);

    const std::size_t before = store.retainedBytes();
    CHECK(store.shrink() == Status::Ok);
    CHECK(store.retainedBytes() < before);
    CHECK(store.capacity() == 49);
    for (std::size_t i = 0; i < 50; ++i)
    {
        CHECK(store.valid(handles[i]) == (i != 10));
        if (i != 10)
            CHECK(store.payload(handles[i]) == int(i) && store.rank(handles[i]) == std::optional<std::uint64_t>(i));
    }

    // Recreated slots (truncated by shrink) never revive stale handles.
    std::vector<HandleA> fresh;
    for (int i = 0; i < 51; ++i)
    {
        const auto result = store.insert(kBox, 1000 + i, {}, std::uint64_t(i) + 50);
        CHECK(result.ok());
        fresh.push_back(result.handle);
    }
    bool slot_reused = false;
    for (std::size_t i = 50; i < 100; ++i)
    {
        CHECK(!store.valid(handles[i]));
        for (const auto& handle : fresh)
            slot_reused = slot_reused || HandleAccess::slot(handle) == HandleAccess::slot(handles[i]);
    }
    CHECK(slot_reused);
    CHECK(!store.valid(handles[10]));
    for (const auto& handle : fresh)
        CHECK(store.valid(handle));

    // Shrinking an empty store releases everything.
    for (std::size_t i = 0; i < 50; ++i)
        if (i != 10)
            CHECK(store.remove(handles[i]) == Status::Ok);
    for (const auto& handle : fresh)
        CHECK(store.remove(handle) == Status::Ok);
    CHECK(store.shrink() == Status::Ok);
    CHECK(store.retainedBytes() == 0 && resource.live() == 0);
    const auto after = store.insert(kBox, 1);
    CHECK(after.ok());
    for (const auto& handle : fresh)
        CHECK(!store.valid(handle));
}

void testAllocationFailure()
{
    for (std::size_t fail = 1; fail <= 16; ++fail)
    {
        TestResource resource;
        StoreA store(&resource);
        std::vector<HandleA> handles;
        resource.failAt = fail;
        int failed_at = -1;
        for (int i = 0; i < 64 && failed_at < 0; ++i)
        {
            const std::size_t size = store.size();
            const std::size_t bytes = store.retainedBytes();
            const std::size_t live = resource.live();
            const auto result = store.insert(Box<int>{i, i, i + 1, i + 1}, i, {}, std::uint64_t(i));
            if (result.status == Status::AllocationFailed)
            {
                failed_at = i;
                CHECK(store.size() == size && store.retainedBytes() == bytes && resource.live() == live);
            }
            else
            {
                CHECK(result.ok());
                handles.push_back(result.handle);
            }
        }
        CHECK(failed_at >= 0);
        for (std::size_t i = 0; i < handles.size(); ++i)
            CHECK(store.valid(handles[i]) && store.payload(handles[i]) == int(i));

        // The failed rank was not taken; the store recovers.
        resource.failAt = 0;
        const auto retry = store.insert(kBox, failed_at, {}, std::uint64_t(failed_at));
        CHECK(retry.ok());
        CHECK(store.size() == handles.size() + 1);
    }

    {
        TestResource resource;
        StoreA store(&resource);
        std::vector<HandleA> handles;
        for (int i = 0; i < 100; ++i)
            handles.push_back(store.insert(kBox, i, {}, std::uint64_t(i)).handle);
        for (std::size_t i = 10; i < 100; ++i)
            CHECK(store.remove(handles[i]) == Status::Ok);
        handles.resize(10);

        const std::size_t bytes = store.retainedBytes();
        resource.failAll = true;
        CHECK(store.shrink() == Status::AllocationFailed);
        CHECK(store.reserve(Reservation{.entries = 1000, .rankedEntries = 1000}) == Status::AllocationFailed);
        CHECK(store.retainedBytes() == bytes && store.size() == 10);
        for (std::size_t i = 0; i < handles.size(); ++i)
            CHECK(store.valid(handles[i]) && store.payload(handles[i]) == int(i));

        resource.failAll = false;
        CHECK(store.shrink() == Status::Ok);
        CHECK(store.retainedBytes() < bytes);
        for (std::size_t i = 0; i < handles.size(); ++i)
            CHECK(store.valid(handles[i]) && store.rank(handles[i]) == std::optional<std::uint64_t>(i));
        CHECK(resource.zeroByteRequests == 0);
    }
}

void testGenerationLimit()
{
    using Store = EntryStore<int, int, LimitedGenerations, TagA>;
    Store store;
    std::vector<Store::HandleType> old;
    for (std::uint32_t slot = 0; slot < 2; ++slot)
    {
        for (std::uint32_t generation = 0; generation <= 2; ++generation)
        {
            const auto result = store.insert(kBox, 1);
            CHECK(result.ok());
            CHECK(HandleAccess::slot(result.handle) == slot);
            CHECK(HandleAccess::generation(result.handle) == generation);
            old.push_back(result.handle);
            CHECK(store.remove(result.handle) == Status::Ok);
        }
    }
    for (const auto& handle : old)
        CHECK(!store.valid(handle));

    // Both slots retired at the slot limit.
    CHECK(store.insert(kBox, 1).status == Status::IdentityExhausted);
    CHECK(store.empty());

    // A new identity epoch resets slots.
    store.clear();
    const auto result = store.insert(kBox, 1);
    CHECK(result.ok() && HandleAccess::slot(result.handle) == 0);
    for (const auto& handle : old)
        CHECK(!store.valid(handle));

    // Shrink keeps retired slots above the highest live slot.
    using Single = EntryStore<int, int, SingleUseSlots, TagA>;
    Single single;
    const auto a = single.insert(kBox, 0).handle;
    const auto b = single.insert(kBox, 1).handle;
    const auto c = single.insert(kBox, 2).handle;
    CHECK(single.remove(c) == Status::Ok);
    CHECK(single.remove(a) == Status::Ok);
    CHECK(single.shrink() == Status::Ok);
    const auto d = single.insert(kBox, 3).handle;
    CHECK(HandleAccess::slot(d) == 3);
    CHECK(!single.valid(a) && single.valid(b) && !single.valid(c) && single.valid(d));
}

// Removes and reinserts the entry behind handle `count` times; returns the last handle.
template<typename Store>
typename Store::HandleType cycle(Store& store, typename Store::HandleType handle, int count,
    std::vector<typename Store::HandleType>& stale, std::uint32_t& maxSlot)
{
    for (int i = 0; i < count; ++i)
    {
        CHECK(store.remove(handle) == Status::Ok);
        stale.push_back(handle);
        const auto result = store.insert(kBox, i);
        CHECK(result.ok());
        handle = result.handle;
        maxSlot = std::max(maxSlot, HandleAccess::slot(handle));
    }
    return handle;
}

void testShrinkGenerations()
{
    using Store = EntryStore<int, int, ChurnGenerations, TagA>;

    // A tail slot above half the limit is kept: churn after shrink must not
    // create a new slot per removal.
    {
        Store store;
        std::vector<Store::HandleType> stale;
        std::uint32_t max_slot = 0;
        const auto live = store.insert(kBox, 0).handle;
        auto hot = cycle(store, store.insert(kBox, 1).handle, 7, stale, max_slot); // Slot 1 at generation 7
        CHECK(store.remove(hot) == Status::Ok);                                     // Free at generation 8
        stale.push_back(hot);
        CHECK(store.shrink() == Status::Ok);

        max_slot = 0;
        hot = store.insert(kBox, 2).handle;
        CHECK(HandleAccess::slot(hot) == 1 && HandleAccess::generation(hot) == 8);
        hot = cycle(store, hot, 100, stale, max_slot);
        CHECK(max_slot <= 1 + 100 / 5 + 1); // Each new slot serves at least 5 generations
        CHECK(store.valid(live) && store.valid(hot) && store.size() == 2);
        for (const auto& handle : stale)
            CHECK(!store.valid(handle));
    }

    // A truncated low-generation slot raises the start generation, capped at half the limit.
    {
        Store store;
        std::vector<Store::HandleType> stale;
        std::uint32_t max_slot = 0;
        const auto live = store.insert(kBox, 0).handle;
        auto tail = cycle(store, store.insert(kBox, 1).handle, 2, stale, max_slot); // Slot 1 at generation 2
        CHECK(store.remove(tail) == Status::Ok);                                     // Free at generation 3
        stale.push_back(tail);
        CHECK(store.shrink() == Status::Ok);

        tail = store.insert(kBox, 2).handle;
        CHECK(HandleAccess::slot(tail) == 1 && HandleAccess::generation(tail) == 3);
        max_slot = 0;
        tail = cycle(store, tail, 100, stale, max_slot);
        CHECK(max_slot <= 1 + 100 / 5 + 1);
        CHECK(store.valid(live) && store.valid(tail));
        for (const auto& handle : stale)
            CHECK(!store.valid(handle));
    }
}

void testRetiredBound()
{
    // Retired bookkeeping is bounded: new slots stop at retiredLimit.
    {
        TestResource resource;
        EntryStore<int, int, RetiredCap, TagA> store(&resource);
        const auto live = store.insert(kBox, 0).handle;
        for (int i = 0; i < 3; ++i)
        {
            const auto result = store.insert(kBox, i);
            CHECK(result.ok() && HandleAccess::slot(result.handle) == std::uint32_t(i + 1));
            CHECK(store.remove(result.handle) == Status::Ok); // Retires the slot
        }
        const std::size_t bytes = store.retainedBytes();
        const std::size_t allocations = resource.allocations;
        for (int i = 0; i < 10; ++i)
            CHECK(store.insert(kBox, i).status == Status::IdentityExhausted);
        CHECK(store.reserve(Reservation{.entries = 2}) == Status::IdentityExhausted);
        CHECK(store.reserve(Reservation{.entries = 1}) == Status::Ok);
        CHECK(store.retainedBytes() == bytes && resource.allocations == allocations);
        CHECK(store.size() == 1 && store.valid(live));

        store.clear(); // New identity epoch
        CHECK(store.insert(kBox, 1).ok());
    }

    // Reservation that retired slots make impossible is rejected before allocating.
    {
        TestResource resource;
        EntryStore<int, int, SlotCap, TagA> store(&resource);
        std::vector<EntryStore<int, int, SlotCap, TagA>::HandleType> handles;
        for (int i = 0; i < 8; ++i)
            handles.push_back(store.insert(kBox, i).handle);
        for (std::size_t i = 1; i < handles.size(); ++i)
            CHECK(store.remove(handles[i]) == Status::Ok); // 1 live, 7 retired

        const std::size_t allocations = resource.allocations;
        CHECK(store.reserve(Reservation{.entries = 2}) == Status::IdentityExhausted);
        CHECK(store.reserve(Reservation{.entries = 9}) == Status::SizeLimitExceeded);
        CHECK(resource.allocations == allocations);
        CHECK(store.reserve(Reservation{.entries = 1}) == Status::Ok);
        CHECK(store.insert(kBox, 8).status == Status::IdentityExhausted);
        CHECK(store.size() == 1 && store.valid(handles[0]));
    }
}

void testIdentityExhaustion()
{
    using Store = EntryStore<int, int, LimitedIdentities, TagA>;
    LimitedIdentities::remaining = 2;
    Store a;
    Store b;
    Store c;
    CHECK(a.insert(kBox, 1).ok());
    CHECK(a.insert(kBox, 2).ok()); // Same identity epoch
    CHECK(b.insert(kBox, 3).ok());
    CHECK(c.insert(kBox, 4).status == Status::IdentityExhausted);
    CHECK(c.empty());

    a.clear();
    CHECK(a.insert(kBox, 5).status == Status::IdentityExhausted);
    CHECK(a.empty());

    Store moved(std::move(b));
    CHECK(moved.size() == 1 && moved.insert(kBox, 6).ok()); // Identity moved with the state
    CHECK(b.insert(kBox, 7).status == Status::IdentityExhausted);
}

void testCheckedValid()
{
    // valid() never terminates in checked types.
    StoreChecked store;
    const auto handle = store.insert(kBox, 1).handle;
    CHECK(store.valid(handle) && !store.valid(StoreChecked::HandleType{}));
    CHECK(store.remove(handle) == Status::Ok);
    CHECK(!store.valid(handle));
}

void testRandomized(std::uint64_t seed)
{
    TestResource resource;
    StoreA store(&resource);
    StoreA spare(&resource);
    Oracle live;
    std::vector<HandleA> handles;
    std::vector<HandleA> dead;
    Rng rng{seed};

    auto randomBox = [&](bool& valid) -> Box<int>
    {
        const int x = rng.range(-1000, 1000);
        const int y = rng.range(-1000, 1000);
        const int w = rng.range(0, 50);
        const int h = rng.range(0, 50);
        valid = rng.below(20) != 0;
        return valid ? Box<int>{x, y, x + w, y + h} : Box<int>{x + w + 1, y, x, y + h};
    };
    auto randomFilter = [&]() { return Filter{rng.next(), rng.next()}; };
    auto bury = [&](HandleA handle)
    {
        if (dead.size() < 2000)
            dead.push_back(handle);
        else
            dead[rng.below(2000)] = handle;
    };

    for (int step = 0; step < 30000; ++step)
    {
        const std::uint32_t op = rng.below(1000);
        if (op < 450)
        {
            bool valid = true;
            const Box<int> box = randomBox(valid);
            std::optional<std::uint64_t> rank;
            if (rng.below(2) == 0)
                rank = rng.below(50) == 0 ? std::numeric_limits<std::uint64_t>::max() : rng.below(256);
            const Filter filter = randomFilter();
            bool duplicate = false;
            if (rank)
                for (const auto& [handle, record] : live)
                    duplicate = duplicate || record.rank == rank;

            StoreA::InsertResultType result{};
            if (rng.below(2) == 0)
                result = store.insert(box, step, filter, rank);
            else
                result = store.insert(Rect<int>{box.minX, box.minY, box.maxX - box.minX, box.maxY - box.minY}, step,
                    filter, rank);

            const Status expected = !valid ? Status::InvalidGeometry
                : duplicate               ? Status::DuplicateRank
                                          : Status::Ok;
            CHECK(result.status == expected);
            if (result.ok())
            {
                CHECK(live.find(result.handle) == live.end());
                live.emplace(result.handle, Record{box, step, filter, rank});
                handles.push_back(result.handle);
            }
        }
        else if (op < 750)
        {
            if (handles.empty())
                continue;
            const std::size_t i = rng.below(static_cast<std::uint32_t>(handles.size()));
            CHECK(store.remove(handles[i]) == Status::Ok);
            live.erase(handles[i]);
            bury(handles[i]);
            handles[i] = handles.back();
            handles.pop_back();
        }
        else if (op < 850)
        {
            if (handles.empty())
                continue;
            const HandleA handle = handles[rng.below(static_cast<std::uint32_t>(handles.size()))];
            bool valid = true;
            const Box<int> box = randomBox(valid);
            CHECK(store.update(handle, box) == (valid ? Status::Ok : Status::InvalidGeometry));
            if (valid)
                live[handle].box = box;
        }
        else if (op < 900)
        {
            if (handles.empty())
                continue;
            const HandleA handle = handles[rng.below(static_cast<std::uint32_t>(handles.size()))];
            const Filter filter = randomFilter();
            CHECK(store.setFilter(handle, filter) == Status::Ok);
            live[handle].filter = filter;
        }
        else if (op < 905)
        {
            store.clear();
            for (const auto& handle : handles)
                bury(handle);
            handles.clear();
            live.clear();
        }
        else if (op < 915)
        {
            CHECK(store.shrink() == Status::Ok);
        }
        else if (op < 925)
        {
            const std::size_t entries = rng.below(static_cast<std::uint32_t>(2 * handles.size() + 2));
            CHECK(store.reserve(Reservation{.entries = entries, .rankedEntries = entries / 2}) == Status::Ok);
        }
        else if (op < 935)
        {
            spare = std::move(store);
            CHECK(store.empty());
            if (!handles.empty())
                CHECK(!store.valid(handles.front()) && spare.valid(handles.front()));
            store = std::move(spare);
        }
        else if (op < 940)
        {
            StoreA temporary(std::move(store));
            store = std::move(temporary);
        }
        else
        {
            if (handles.empty())
                continue;
            const HandleA handle = handles[rng.below(static_cast<std::uint32_t>(handles.size()))];
            store.payload(handle) = -step;
            live[handle].payload = -step;
        }

        CHECK(store.size() == live.size());
        if (step % 97 == 0)
            verify(store, live, dead);
    }
    verify(store, live, dead);
    CHECK(resource.zeroByteRequests == 0 && resource.misaligned == 0 && resource.foreign == 0);
}
} // namespace

int main(int argc, char** argv)
{
    const std::uint64_t seed = argc > 1 ? std::strtoull(argv[1], nullptr, 0) : 20240917u;

    testModuleIdentity(); // First: compares identities drawn across translation units
    testBasics();
    testGeometry();
    testRanks();
    testRankCollisions();
    testRankClearCost();
    testRankOracle(seed);
    testForeignAndAddressReuse();
    testClear();
    testPayloadLifetime();
    testForEachEntry();
    testMoves();
    testReserveAndHardCapacity();
    testShrink();
    testAllocationFailure();
    testGenerationLimit();
    testShrinkGenerations();
    testRetiredBound();
    testIdentityExhaustion();
    testCheckedValid();
    testRandomized(seed);

    if (gFailures != 0)
    {
        std::fprintf(stderr, "%d check(s) failed (seed %llu)\n", gFailures, static_cast<unsigned long long>(seed));
        return 1;
    }
    std::printf("entry-store-test: all checks passed (seed %llu)\n", static_cast<unsigned long long>(seed));
    return 0;
}
