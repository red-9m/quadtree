#pragma once

#include <bit>
#include <numeric>
#include <ranges>
#include <span>
#if defined(__SSE2__)
#include <emmintrin.h>
#endif

#include "../quadtree-index.h"

namespace QuadTree
{

enum class Boundary : std::uint8_t { Default, TouchInclusive };
enum class Extent : std::uint8_t { Fixed, Adaptive, Snapshot };
enum class SnapshotGrowth : std::uint8_t { Unspecified, Adaptive, Reject };

template<Coordinate T>
struct Config
{
    Extent extent = Extent::Adaptive;
    std::optional<Box<T>> root = {};
    SnapshotGrowth snapshotGrowth = SnapshotGrowth::Unspecified;
    Boundary boundary = Boundary::Default;
    std::uint32_t splitThreshold = 8;
    std::uint32_t mergeThreshold = 4;
    std::uint32_t smallSetThreshold = 64;
    std::uint32_t maxDepth = 64;
    std::size_t maxEntries = detail::kMaxSlots;
    std::size_t maxNodes = detail::kMaxSlots;
    bool hardCapacity = false;
};

struct WorkBudget
{
    std::uint64_t candidateTests = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t nodeVisits = std::numeric_limits<std::uint64_t>::max();
};

struct Counters
{
    std::uint64_t nodeVisits = 0;
    std::uint64_t candidateTests = 0;
    std::uint64_t filterRejections = 0;
    std::uint64_t predicateCalls = 0;
    std::uint64_t reports = 0;
};

struct [[nodiscard]] QueryResult
{
    Status status = Status::Ok;
    Counters counters = {};

    bool ok() const noexcept { return status == Status::Ok; }
    bool complete() const noexcept { return status == Status::Ok; }
};

struct [[nodiscard]] CollectResult
{
    Status status = Status::Ok;
    std::size_t count = 0; // Elements written, even with instrumentation disabled
    Counters counters = {};

    bool ok() const noexcept { return status == Status::Ok; }
    bool complete() const noexcept { return status == Status::Ok; }
};

struct IndexStats
{
    std::size_t entries = 0;
    std::size_t nodes = 0; // Reachable pool slots, including empty siblings
    std::size_t references = 0;
    std::uint32_t maxDepth = 0;
    std::size_t liveBytes = 0;
    std::size_t retainedBytes = 0;
    std::size_t entryCapacity = 0;
    std::size_t nodeCapacity = 0;
    std::size_t referenceCapacity = 0;
    std::size_t descriptorCapacity = 0;
    std::size_t nodeBytes = 0;
    std::size_t referenceBytes = 0;
    std::size_t descriptorBytes = 0;
    std::size_t handleBytes = 16;
};

template<Coordinate T>
struct NodeView
{
    const Box<T>& cell;
    const Box<T>& bounds; // Valid only when entries != 0
    Filter summary;
    std::uint32_t depth;
    std::uint32_t entries;
    std::uint32_t ownEntries;
};

struct WorkspaceReservation
{
    std::size_t entries = 0;      // Ordered rows/order, selection flags and moved membership
    std::size_t movedEntries = 0; // Submitted moved handles, normalized to dense indices
    std::size_t buildEntries = 0; // Captured boxes, descriptors and two sorting buffers
};

namespace detail
{
// Trivially copyable resource-backed arrays. Allocation initializes spare slots;
// growing, cloning and moving never invoke a throwing allocator.
template<typename U, std::size_t Alignment = alignof(U)>
class Buffer
{
    static_assert(std::is_trivially_copyable_v<U> && std::is_nothrow_default_constructible_v<U>);

public:
    explicit Buffer(Resource* resource) noexcept: mResource(resource) {}
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept: mResource(other.mResource), mData(std::exchange(other.mData, nullptr)),
        mCapacity(std::exchange(other.mCapacity, 0)) {}
    Buffer& operator=(Buffer&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            mResource = other.mResource;
            mData = std::exchange(other.mData, nullptr);
            mCapacity = std::exchange(other.mCapacity, 0);
        }
        return *this;
    }
    ~Buffer() { reset(); }

    U& operator[](std::size_t i) noexcept { return mData[i]; }
    const U& operator[](std::size_t i) const noexcept { return mData[i]; }
    U* data() noexcept { return mData; }
    const U* data() const noexcept { return mData; }
    std::size_t capacity() const noexcept { return mCapacity; }
    std::size_t bytes() const noexcept { return mCapacity * sizeof(U); }

    Status allocate(std::size_t count) noexcept
    {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(U))
            return Status::SizeLimitExceeded;
        if (count == 0)
            return Status::Ok;
        U* data = static_cast<U*>(mResource->allocate(count * sizeof(U), Alignment));
        if (!data)
            return Status::AllocationFailed;
        if constexpr (std::same_as<U, std::byte>)
            data = ::new (data) U[count](); // One byte array can provide storage for typed scratch arrays.
        else
            for (std::size_t i = 0; i < count; ++i)
                std::construct_at(data + i);
        reset();
        mData = data;
        mCapacity = count;
        return Status::Ok;
    }

    // The caller commits pending only after every other fallible preparation.
    Status prepare(Buffer& pending, std::size_t count) const noexcept
    {
        if (count <= mCapacity)
            return Status::Ok;
        const Status status = pending.allocate(count);
        if (status == Status::Ok && mCapacity != 0)
            std::memcpy(pending.data(), mData, bytes());
        return status;
    }

    void commit(Buffer& pending) noexcept
    {
        if (pending.mData)
            swap(pending);
    }

    void swap(Buffer& other) noexcept
    {
        std::swap(mResource, other.mResource);
        std::swap(mData, other.mData);
        std::swap(mCapacity, other.mCapacity);
    }

    void reset() noexcept
    {
        if (mData)
            mResource->deallocate(mData);
        mData = nullptr;
        mCapacity = 0;
    }

private:
    Resource* mResource;
    U* mData = nullptr;
    std::size_t mCapacity = 0;
};

struct Descriptor
{
    Filter filter = {};
    std::optional<std::uint64_t> rank = {};
    const void* payload = nullptr;
};

// A portable 128-bit Morton prefix, then the depth of the stored prefix.
// No quantization, floating-point integer conversion, or packed entry limit.
struct SpatialKey
{
    std::uint64_t hi;
    std::uint64_t lo;
    std::uint32_t level;
    std::uint32_t entry;
};

template<typename U, typename Less>
void heapSort(U* data, std::size_t count, Less less) noexcept
{
    const auto sift = [&](std::size_t root, std::size_t end) noexcept
    {
        while (root < end / 2)
        {
            std::size_t child = root * 2 + 1;
            if (child + 1 < end && less(data[child], data[child + 1]))
                ++child;
            if (!less(data[root], data[child]))
                break;
            std::swap(data[root], data[child]);
            root = child;
        }
    };
    for (std::size_t i = count / 2; i != 0;)
        sift(--i, count);
    for (std::size_t end = count; end > 1;)
    {
        std::swap(data[0], data[--end]);
        sift(0, end);
    }
}

// Bounded stack, no allocation; equal keys need not be stable.
template<typename U, typename Less>
void introPartition(U* data, std::size_t count, Less less, unsigned depth) noexcept
{
    while (count > 16)
    {
        if (depth == 0)
        {
            heapSort(data, count, less);
            return;
        }
        --depth;
        const U pivot = data[count / 2];
        std::size_t left = 0, right = count;
        do
        {
            while (left < right && less(data[left], pivot)) ++left;
            while (left < right && less(pivot, data[right - 1])) --right;
            if (left == right) break;
            std::swap(data[left++], data[--right]);
        } while (left < right);
        if (left == 0 || left == count)
        {
            heapSort(data, count, less);
            return;
        }
        if (left < count - left)
        {
            introPartition(data, left, less, depth);
            data += left;
            count -= left;
        }
        else
        {
            introPartition(data + left, count - left, less, depth);
            count = left;
        }
    }
    for (std::size_t i = 1; i < count; ++i)
    {
        U value = data[i];
        std::size_t j = i;
        while (j != 0 && less(value, data[j - 1]))
        {
            data[j] = data[j - 1];
            --j;
        }
        data[j] = value;
    }
}

template<typename U, typename Less>
void rankSort(U* data, std::size_t count, Less less) noexcept
{
    unsigned depth = 0;
    for (std::size_t n = count; n > 1; n >>= 1)
        depth += 2;
    introPartition(data, count, less, depth);
}

inline bool uniqueBuildRanks(std::uint64_t* ranks, std::uint64_t* temp, std::uint32_t count) noexcept
{
    if (count < 256)
        rankSort(ranks, count, std::less<>{});
    else
    {
        std::uint32_t counts[8][256]{};
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto rank = ranks[i];
            for (unsigned byte = 0; byte < 8; ++byte)
                ++counts[byte][(rank >> (8 * byte)) & 255u];
        }
        for (unsigned byte = 0; byte < 8; ++byte)
        {
            const auto shift = 8 * byte;
            auto& bucket = counts[byte];
            if (bucket[(ranks[0] >> shift) & 255u] == count)
                continue;
            std::uint32_t offset = 0;
            for (auto& size : bucket)
            {
                const auto previous = offset;
                offset += size;
                size = previous;
            }
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const auto rank = ranks[i];
                temp[bucket[(rank >> shift) & 255u]++] = rank;
            }
            std::swap(ranks, temp);
        }
    }
    for (std::uint32_t i = 1; i < count; ++i)
        if (ranks[i - 1] == ranks[i])
            return false;
    return true;
}

// Stable LSD radix sort on (hi, lo, level); equal keys retain input order.
inline void radixSort(SpatialKey* keys, SpatialKey* temp, std::size_t count) noexcept
{
    if (count < 2)
        return;
    SpatialKey* const original = keys;
    std::uint64_t varying_hi = 0, varying_lo = 0;
    std::uint32_t varying_level = 0;
    for (std::size_t i = 1; i < count; ++i)
    {
        varying_hi |= keys[i].hi ^ keys[0].hi;
        varying_lo |= keys[i].lo ^ keys[0].lo;
        varying_level |= keys[i].level ^ keys[0].level;
    }
    for (unsigned pass = 0; pass < 17; ++pass)
    {
        const std::uint64_t varying = pass == 0 ? varying_level : pass <= 8 ?
            varying_lo >> ((pass - 1) * 8) : varying_hi >> ((pass - 9) * 8);
        if ((varying & 255u) == 0)
            continue;
        const auto digit = [pass](const SpatialKey& key) noexcept -> std::size_t
        {
            if (pass == 0)
                return key.level;
            if (pass <= 8)
                return static_cast<std::size_t>((key.lo >> ((pass - 1) * 8)) & 255u);
            return static_cast<std::size_t>((key.hi >> ((pass - 9) * 8)) & 255u);
        };
        std::size_t buckets[257] = {};
        for (std::size_t i = 0; i < count; ++i)
            ++buckets[digit(keys[i]) + 1];
        for (std::size_t i = 1; i < 257; ++i)
            buckets[i] += buckets[i - 1];
        for (std::size_t i = 0; i < count; ++i)
            temp[buckets[digit(keys[i])]++] = keys[i];
        std::swap(keys, temp);
    }
    if (keys != original)
        std::memcpy(original, keys, count * sizeof(SpatialKey));
}

constexpr bool compatible(Filter a, Filter b) noexcept
{
    return (a.category & b.mask) != 0 && (b.category & a.mask) != 0;
}

template<Coordinate T>
constexpr bool contains(const Box<T>& outer, const Box<T>& inner) noexcept
{
    return outer.minX <= inner.minX && outer.minY <= inner.minY && inner.maxX <= outer.maxX &&
        inner.maxY <= outer.maxY;
}

template<Coordinate T>
constexpr bool closedOverlap(const Box<T>& a, const Box<T>& b) noexcept
{
    return a.minX <= b.maxX && b.minX <= a.maxX && a.minY <= b.maxY && b.minY <= a.maxY;
}

template<Coordinate T>
constexpr bool overlaps(const Box<T>& a, const Box<T>& b, Boundary boundary) noexcept
{
    if (boundary == Boundary::TouchInclusive || a.minX == a.maxX || a.minY == a.maxY ||
        b.minX == b.maxX || b.minY == b.maxY)
        return closedOverlap(a, b);
    return a.minX < b.maxX && b.minX < a.maxX && a.minY < b.maxY && b.minY < a.maxY;
}

template<Coordinate T>
constexpr Box<T> unite(const Box<T>& a, const Box<T>& b) noexcept
{
    return {std::min(a.minX, b.minX), std::min(a.minY, b.minY), std::max(a.maxX, b.maxX),
        std::max(a.maxY, b.maxY)};
}

template<Coordinate T>
constexpr bool normalize(Box<T> input, Box<T>& output) noexcept
{
    if (!canonicalize(input))
        return false;
    output = input;
    return true;
}

template<Coordinate T>
constexpr bool normalize(const Rect<T>& input, Box<T>& output) noexcept { return toBox(input, output); }

template<typename G, typename T>
concept Geometry = !std::is_volatile_v<std::remove_reference_t<G>> &&
    (std::same_as<std::remove_cvref_t<G>, Box<T>> || std::same_as<std::remove_cvref_t<G>, Rect<T>>);

// The float span is a size heuristic only. Halving endpoints avoids overflow;
// rounding/subnormal loss can affect placement, never conservative bounds.
template<Coordinate T>
constexpr auto span(T lo, T hi) noexcept
{
    if constexpr (IntegerCoordinate<T>)
    {
        using U = std::make_unsigned_t<T>;
        return U(hi) - U(lo);
    }
    else
        return hi / T(2) - lo / T(2);
}

template<Coordinate T>
struct Cell
{
    Box<T> box{};
    bool closedX = true;
    bool closedY = true;
};

template<Coordinate T>
struct Split
{
    T x, y;
    bool splitX, splitY;

    explicit Split(const Cell<T>& cell) noexcept:
        x(std::midpoint(cell.box.minX, cell.box.maxX)), y(std::midpoint(cell.box.minY, cell.box.maxY)),
        splitX(cell.box.minX < x && x < cell.box.maxX), splitY(cell.box.minY < y && y < cell.box.maxY) {}

    bool progress() const noexcept { return splitX || splitY; }

    bool fits(const Cell<T>& cell, const Box<T>& box) const noexcept
    {
        const auto w = splitX ? std::min(span(cell.box.minX, x), span(x, cell.box.maxX)) :
            span(cell.box.minX, cell.box.maxX);
        const auto h = splitY ? std::min(span(cell.box.minY, y), span(y, cell.box.maxY)) :
            span(cell.box.minY, cell.box.maxY);
        return span(box.minX, box.maxX) <= w && span(box.minY, box.maxY) <= h;
    }

    std::uint32_t quadrant(const Box<T>& box) const noexcept
    {
        return (splitX && std::midpoint(box.minX, box.maxX) >= x ? 1u : 0u) |
            (splitY && std::midpoint(box.minY, box.maxY) >= y ? 2u : 0u);
    }

    Cell<T> child(Cell<T> cell, std::uint32_t q) const noexcept
    {
        if (splitX)
        {
            if ((q & 1u) != 0)
                cell.box.minX = x;
            else
            {
                cell.box.maxX = x;
                cell.closedX = false;
            }
        }
        if (splitY)
        {
            if ((q & 2u) != 0)
                cell.box.minY = y;
            else
            {
                cell.box.maxY = y;
                cell.closedY = false;
            }
        }
        return cell;
    }
};

template<Coordinate T>
bool centerInside(const Cell<T>& cell, const Box<T>& box) noexcept
{
    const T x = std::midpoint(box.minX, box.maxX);
    const T y = std::midpoint(box.minY, box.maxY);
    return cell.box.minX <= x && (x < cell.box.maxX || (cell.closedX && x == cell.box.maxX)) &&
        cell.box.minY <= y && (y < cell.box.maxY || (cell.closedY && y == cell.box.maxY));
}

// Grow by a half span on both sides, saturating at the coordinate limits.
// Unsigned differences and midpoint avoid signed overflow and wide extensions.
template<Coordinate T>
Box<T> padded(Box<T> box) noexcept
{
    const auto axis = [](T& lo, T& hi) noexcept
    {
        if constexpr (IntegerCoordinate<T>)
        {
            using U = std::make_unsigned_t<T>;
            const U amount = (U(hi) - U(lo)) / 2;
            const T low = std::numeric_limits<T>::lowest();
            const T high = std::numeric_limits<T>::max();
            const U left = U(lo) - U(low);
            const U right = U(high) - U(hi);
            // amount <= signed max, so each cast and the guarded addition is safe.
            lo = amount > left ? low : static_cast<T>(lo - static_cast<T>(amount));
            hi = amount > right ? high : static_cast<T>(hi + static_cast<T>(amount));
        }
        else
        {
            const T amount = span(lo, hi);
            const T low = std::numeric_limits<T>::lowest();
            const T high = std::numeric_limits<T>::max();
            lo = std::max(low, static_cast<T>(lo - amount));
            hi = std::min(high, static_cast<T>(hi + amount));
        }
    };
    axis(box.minX, box.maxX);
    axis(box.minY, box.maxY);
    return box;
}

inline void increment(std::uint64_t& value) noexcept
{
    if (value != std::numeric_limits<std::uint64_t>::max())
        ++value;
}

[[noreturn]] inline void activeWorkspace() noexcept
{
    std::fputs("QuadTree: active workspace reused or moved\n", stderr);
    std::abort();
}

template<typename R, typename P, typename T>
concept BuildRange = std::ranges::forward_range<R> &&
    std::is_lvalue_reference_v<std::ranges::range_reference_t<R>> &&
    requires(std::ranges::range_reference_t<R> entry)
    {
        requires Geometry<decltype(entry.rect), T>;
        requires std::same_as<std::remove_cvref_t<decltype(entry.payload)>, P>;
        requires (!std::is_volatile_v<std::remove_reference_t<decltype(entry.payload)>>);
        requires std::same_as<std::remove_cvref_t<decltype(entry.filter)>, Filter>;
        requires std::same_as<std::remove_cvref_t<decltype(entry.rank)>, std::optional<std::uint64_t>>;
    };

template<typename R, typename P, typename T>
concept MutableBuildRange = BuildRange<R, P, T> &&
    !std::is_const_v<std::remove_reference_t<decltype((std::declval<std::ranges::range_reference_t<R>>().payload))>>;

template<typename R, typename P>
concept PayloadRange = std::ranges::forward_range<R> &&
    !std::is_volatile_v<std::remove_reference_t<std::ranges::range_reference_t<R>>> &&
    std::is_lvalue_reference_v<std::ranges::range_reference_t<R>> &&
    std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<R>>, P>;

struct AcceptAll
{
    template<typename... Args>
    bool operator()(const Args&...) const noexcept { return true; }
};
} // namespace detail

template<Coordinate T>
class Workspace
{
    template<StorablePayload, Coordinate, DiagnosticsPolicy, bool>
    friend class Index;

public:
    explicit Workspace(Resource* resource = nullptr) noexcept:
        mResource(resource ? resource : defaultResource()), mOrder(mResource), mRow(mResource), mMoved(mResource),
        mMembership(mResource), mSelections(mResource), mBuildBoxes(mResource), mDescriptors(mResource),
        mKeys(mResource), mTemp(mResource) {}
    Workspace(const Workspace&) = delete;
    Workspace& operator=(const Workspace&) = delete;
    Workspace(Workspace&& other) noexcept: Workspace(other.mResource) { *this = std::move(other); }
    Workspace& operator=(Workspace&& other) noexcept
    {
        if (this != &other)
        {
            if (mActive || other.mActive)
                detail::activeWorkspace();
            mResource = other.mResource;
            mOrder = std::move(other.mOrder);
            mRow = std::move(other.mRow);
            mMoved = std::move(other.mMoved);
            mMembership = std::move(other.mMembership);
            mSelections = std::move(other.mSelections);
            mBuildBoxes = std::move(other.mBuildBoxes);
            mDescriptors = std::move(other.mDescriptors);
            mKeys = std::move(other.mKeys);
            mTemp = std::move(other.mTemp);
            mEpoch = std::exchange(other.mEpoch, 0);
        }
        return *this;
    }

    Resource* resource() const noexcept { return mResource; }
    std::size_t retainedBytes() const noexcept
    {
        return mOrder.bytes() + mRow.bytes() + mMoved.bytes() + mMembership.bytes() + mSelections.bytes() +
            mBuildBoxes.bytes() + mDescriptors.bytes() + mKeys.bytes() + mTemp.bytes();
    }

    Status reserve(std::size_t entries) noexcept { return reserve({entries, entries, entries}); }

    Status reserve(const WorkspaceReservation& reservation) noexcept
    {
        if (mActive)
            detail::activeWorkspace();
        if (reservation.entries > detail::kMaxSlots || reservation.movedEntries > detail::kMaxSlots ||
            reservation.buildEntries > detail::kMaxSlots)
            return Status::SizeLimitExceeded;
        Workspace pending(mResource);
        Status status = mOrder.prepare(pending.mOrder, reservation.entries);
        if (status == Status::Ok) status = mRow.prepare(pending.mRow, reservation.entries);
        if (status == Status::Ok) status = mMoved.prepare(pending.mMoved, reservation.movedEntries);
        if (status == Status::Ok) status = mMembership.prepare(pending.mMembership, reservation.entries);
        if (status == Status::Ok) status = mSelections.prepare(pending.mSelections, reservation.entries);
        if (status == Status::Ok) status = mBuildBoxes.prepare(pending.mBuildBoxes, reservation.buildEntries);
        if (status == Status::Ok) status = mDescriptors.prepare(pending.mDescriptors, reservation.buildEntries);
        if (status == Status::Ok) status = mKeys.prepare(pending.mKeys, reservation.buildEntries);
        // mKeys.prepare already verified the byte multiplication.
        if (status == Status::Ok && tempKeyCapacity() < reservation.buildEntries)
            status = pending.mTemp.allocate(reservation.buildEntries * sizeof(detail::SpatialKey));
        if (status != Status::Ok)
            return status;
        mOrder.commit(pending.mOrder);
        mRow.commit(pending.mRow);
        mMoved.commit(pending.mMoved);
        mMembership.commit(pending.mMembership);
        mSelections.commit(pending.mSelections);
        mBuildBoxes.commit(pending.mBuildBoxes);
        mDescriptors.commit(pending.mDescriptors);
        mKeys.commit(pending.mKeys);
        mTemp.commit(pending.mTemp);
        return Status::Ok;
    }

    // O(membership capacity); schedule outside the step before epoch wrap.
    void resetMovedMembership() noexcept
    {
        if (mActive)
            detail::activeWorkspace();
        resetEpoch();
    }

private:
    Resource* mResource;
    detail::Buffer<std::uint32_t> mOrder, mRow, mMoved;
    detail::Buffer<std::uint32_t> mMembership;
    detail::Buffer<std::uint8_t> mSelections;
    // Spatial planning reads geometry repeatedly, not payloads, filters or ranks.
    detail::Buffer<Box<T>> mBuildBoxes;
    detail::Buffer<detail::Descriptor> mDescriptors;
    detail::Buffer<detail::SpatialKey> mKeys;
    detail::Buffer<std::byte, alignof(detail::SpatialKey)> mTemp;

    std::size_t tempKeyCapacity() const noexcept { return mTemp.capacity() / sizeof(detail::SpatialKey); }

    // Each phase starts its own array lifetime within the same allocation.
    // Scratch is overwritten before it is read; the caller checked capacity.
    detail::SpatialKey* tempKeys(std::size_t count) noexcept
    {
        return ::new (mTemp.data()) detail::SpatialKey[count];
    }
    std::uint64_t* tempRanks(std::size_t count) noexcept
    {
        static_assert(sizeof(detail::SpatialKey) >= 2 * sizeof(std::uint64_t));
        return ::new (mTemp.data()) std::uint64_t[2 * count];
    }
    std::pair<std::uint32_t*, std::uint8_t*> tempPermutation(std::size_t count) noexcept
    {
        static_assert(sizeof(detail::SpatialKey) >= 2 * sizeof(std::uint32_t) + sizeof(std::uint8_t));
        auto* data = mTemp.data();
        auto* ids = ::new (data) std::uint32_t[2 * count];
        auto* tags = ::new (data + 2 * count * sizeof(std::uint32_t)) std::uint8_t[count];
        return {ids, tags};
    }
    std::uint32_t mEpoch = 0;
    bool mActive = false;

    struct Use
    {
        Workspace& workspace;
        explicit Use(Workspace& value) noexcept: workspace(value)
        {
            if (workspace.mActive)
                detail::activeWorkspace();
            workspace.mActive = true;
        }
        ~Use() { workspace.mActive = false; }
    };

    void resetEpoch() noexcept
    {
        for (std::size_t i = 0; i < mMembership.capacity(); ++i)
            mMembership[i] = 0;
        mEpoch = 0;
    }

    void nextEpoch() noexcept
    {
        if (mEpoch == std::numeric_limits<std::uint32_t>::max())
            resetEpoch();
        ++mEpoch;
    }
};

template<Coordinate T>
struct QueryOptions
{
    bool ordered = false;
    Workspace<T>* workspace = nullptr; // Non-null = prepared: never grown by the operation
    WorkBudget budget = {};
    std::optional<std::uint64_t> greaterRank = {}; // queryPartners only, exclusive
    std::optional<std::uint64_t> lessRank = {};    // queryPartners only, exclusive
};

template<Coordinate T>
struct BuildOptions
{
    Workspace<T>* workspace = nullptr; // Non-null = prepared
    bool transactional = false; // Both modes currently prepare every failure before commit
};

template<StorablePayload Payload, Coordinate T, typename G = Box<T>>
    requires detail::Geometry<G, T>
struct BuildEntry
{
    G rect;
    Payload payload;
    Filter filter = {};
    std::optional<std::uint64_t> rank = {};
};

template<typename I>
struct [[nodiscard]] CreateResult
{
    Status status;
    std::optional<I> index;

    bool ok() const noexcept { return status == Status::Ok; }
    I& value() & noexcept { return *index; }
    const I& value() const & noexcept { return *index; }
    I&& value() && noexcept { return std::move(*index); }
};

template<StorablePayload Payload, Coordinate T, DiagnosticsPolicy Policy, bool Instrumented>
class Index
{
    template<StorablePayload, Coordinate, DiagnosticsPolicy, bool>
    friend class Index;

    using Store = detail::EntryStore<T, Payload, Policy, Index>;
    using Traits = detail::PolicyTraits<Policy>;
    using Cell = detail::Cell<T>;
    using Key = detail::SpatialKey;
    static constexpr std::uint32_t none = detail::kNone;

    struct Reference
    {
        std::uint32_t node = none;
        std::uint32_t previous = none;
        std::uint32_t next = none;
    };

    struct Node
    {
        Cell cell;
        Box<T> bounds{};
        Box<T> itemBounds{};
        Filter summary{0, 0};
        Filter itemSummary{0, 0};
        std::uint32_t parent = none;
        std::uint32_t firstChild = none;
        std::uint32_t head = none;
        std::uint32_t count = 0;
        std::uint32_t ownCount = 0;
        std::uint32_t splitRetry = 0;  // Successful own changes before retrying an unproductive split
        std::uint32_t itemChanges = 0; // Changes since the last tight own-summary refit
        std::uint8_t depth = 0; // Config validation caps depth at 64
        bool linear = true;    // Sorted snapshots can traverse own entries without link loads
        bool itemDirty = false; // Own bounds/filter summaries may conservatively include old entries
    };

public:
    using HandleType = typename Store::HandleType;
    using HandlePair = std::pair<HandleType, HandleType>;
    using View = typename Store::View;
    using InsertResultType = typename Store::InsertResultType;
    using PayloadType = Payload;
    using CoordinateType = T;

    static CreateResult<Index> create(Config<T> config = {}, Resource* resource = nullptr) noexcept
    {
        const bool known_extent = config.extent == Extent::Fixed || config.extent == Extent::Adaptive ||
            config.extent == Extent::Snapshot;
        const bool known_growth = config.snapshotGrowth == SnapshotGrowth::Unspecified ||
            config.snapshotGrowth == SnapshotGrowth::Adaptive || config.snapshotGrowth == SnapshotGrowth::Reject;
        if (!known_extent || !known_growth ||
            (config.boundary != Boundary::Default && config.boundary != Boundary::TouchInclusive) ||
            config.splitThreshold == 0 || config.mergeThreshold >= config.splitThreshold || config.maxDepth > 64 ||
            config.maxEntries > Traits::slotLimit || config.maxNodes == 0 || config.maxNodes > detail::kMaxSlots ||
            (config.extent == Extent::Fixed && !config.root) ||
            (config.extent == Extent::Snapshot && (config.root || config.snapshotGrowth == SnapshotGrowth::Unspecified)))
            return {Status::InvalidConfiguration, std::nullopt};
        if (config.root)
        {
            if (!detail::canonicalize(*config.root) || (config.extent == Extent::Fixed &&
                (!(config.root->minX < config.root->maxX) || !(config.root->minY < config.root->maxY))))
                return {Status::InvalidGeometry, std::nullopt};
        }
        return {Status::Ok, Index(config, resource)};
    }

    Index(const Index&) = delete;
    Index& operator=(const Index&) = delete;
    Index(Index&& other) noexcept:
        mConfig(other.mConfig), mEntries(std::move(other.mEntries)), mNodes(std::move(other.mNodes)),
        mReferences(std::move(other.mReferences)), mKeys(std::move(other.mKeys)), mTemp(std::move(other.mTemp)),
        mExtent(std::exchange(other.mExtent, other.mConfig.root)), mRoot(std::exchange(other.mRoot, none)),
        mNodeUsed(std::exchange(other.mNodeUsed, 0)), mActiveNodes(std::exchange(other.mActiveNodes, 0)),
        mFreeBlocks(std::exchange(other.mFreeBlocks, none)) {}

    Index& operator=(Index&& other) noexcept
    {
        if (this != &other)
        {
            mConfig = other.mConfig;
            mEntries = std::move(other.mEntries);
            mNodes = std::move(other.mNodes);
            mReferences = std::move(other.mReferences);
            mKeys = std::move(other.mKeys);
            mTemp = std::move(other.mTemp);
            mExtent = std::exchange(other.mExtent, other.mConfig.root);
            mRoot = std::exchange(other.mRoot, none);
            mNodeUsed = std::exchange(other.mNodeUsed, 0);
            mActiveNodes = std::exchange(other.mActiveNodes, 0);
            mFreeBlocks = std::exchange(other.mFreeBlocks, none);
        }
        return *this;
    }

    Resource* resource() const noexcept { return mEntries.resource(); }
    std::size_t size() const noexcept { return mEntries.size(); }
    bool empty() const noexcept { return mEntries.empty(); }
    bool valid(HandleType handle) const noexcept { return mEntries.valid(handle); }
    const Config<T>& config() const noexcept { return mConfig; }
    std::optional<Box<T>> extent() const noexcept { return mExtent; }
    const Box<T>& rect(HandleType handle) const noexcept { return mEntries.rect(handle); }
    const Payload& payload(HandleType handle) const noexcept { return mEntries.payload(handle); }
    Payload& payload(HandleType handle) noexcept { return mEntries.payload(handle); }
    Filter filter(HandleType handle) const noexcept { return mEntries.filter(handle); }
    std::optional<std::uint64_t> rank(HandleType handle) const noexcept { return mEntries.rank(handle); }

    template<typename G, typename P>
        requires detail::Geometry<G, T> && std::same_as<std::remove_cvref_t<P>, Payload> &&
            std::is_nothrow_constructible_v<Payload, P&&>
    InsertResultType insert(const G& geometry, P&& value, Filter entryFilter = {},
        std::optional<std::uint64_t> entryRank = {}) noexcept
    {
        Box<T> box{};
        if (!detail::normalize(geometry, box))
            return {Status::InvalidGeometry, {}};
        if (size() >= mConfig.maxEntries)
            return {Status::SizeLimitExceeded, {}};
        if (entryRank && mEntries.containsRank(*entryRank))
            return {Status::DuplicateRank, {}};
        std::optional<Box<T>> next_extent;
        const Status extent_status = mutationExtent(box, next_extent);
        if (extent_status != Status::Ok)
            return {extent_status, {}};

        Mutation plan;
        const auto dense = static_cast<std::uint32_t>(size());
        const Status planned = prepareMutation(dense, box, next_extent, true, plan);
        if (planned != Status::Ok)
            return {planned, {}};
        detail::Buffer<Node> nodes(resource());
        detail::Buffer<Reference> references(resource());
        const Status prepared = prepareStorage(plan.requiredNodes, size() + 1, nodes, references);
        if (prepared != Status::Ok)
            return {prepared, {}};
        const auto result = mEntries.insert(box, std::forward<P>(value), entryFilter, entryRank);
        if (!result.ok())
            return result;
        mNodes.commit(nodes);
        mReferences.commit(references);
        mExtent = next_extent;
        commitMutation(dense, plan, none);
        return result;
    }

    // Borrows the accessor for this call; evaluates it exactly once.
    template<typename P, typename Accessor>
        requires std::same_as<std::remove_cvref_t<P>, Payload> && std::is_nothrow_constructible_v<Payload, P&&> &&
            std::is_nothrow_invocable_v<Accessor&, const Payload&> &&
            detail::Geometry<std::invoke_result_t<Accessor&, const Payload&>, T>
    InsertResultType insertWith(P&& value, Accessor&& accessor, Filter entryFilter = {},
        std::optional<std::uint64_t> entryRank = {}) noexcept
    {
        const auto geometry = std::invoke(accessor, std::as_const(value));
        return insert(geometry, std::forward<P>(value), entryFilter, entryRank);
    }

    template<typename G> requires detail::Geometry<G, T>
    Status update(HandleType handle, const G& geometry) noexcept
    {
        mEntries.requireValid(handle, "update");
        Box<T> box{};
        if (!detail::normalize(geometry, box))
            return Status::InvalidGeometry;
        const std::uint32_t dense = mEntries.denseOf(handle);
        if (mEntries.mBoxes[dense] == box)
            return Status::Ok;
        std::optional<Box<T>> next_extent;
        const Status extent_status = mutationExtent(box, next_extent);
        if (extent_status != Status::Ok)
            return extent_status;
        if (mRoot != none && next_extent == mExtent && belongs(mReferences[dense].node, box))
        {
            const Node& node = mNodes[mReferences[dense].node];
            // Retry overfull leaves periodically: motion can make a formerly
            // coincident leaf productive without changing the owning cell.
            if (node.firstChild != none || node.ownCount <= mConfig.splitThreshold ||
                node.depth >= mConfig.maxDepth || node.splitRetry != 0)
            {
                mEntries.mBoxes[dense] = box;
                changedItems(mReferences[dense].node, true);
                includeItem(mReferences[dense].node, dense);
                refitPath(mReferences[dense].node);
                return Status::Ok;
            }
        }
        Mutation plan;
        const Status planned = prepareMutation(dense, box, next_extent, false, plan);
        if (planned != Status::Ok)
            return planned;
        detail::Buffer<Node> nodes(resource());
        detail::Buffer<Reference> references(resource());
        const Status prepared = prepareStorage(plan.requiredNodes, size(), nodes, references);
        if (prepared != Status::Ok)
            return prepared;
        mNodes.commit(nodes);
        mReferences.commit(references);
        const std::uint32_t old_node = mRoot == none ? none : mReferences[dense].node;
        mEntries.mBoxes[dense] = box;
        mExtent = next_extent;
        commitMutation(dense, plan, old_node);
        return Status::Ok;
    }

    Status remove(HandleType handle) noexcept
    {
        mEntries.requireValid(handle, "remove");
        const std::uint32_t dense = mEntries.denseOf(handle);
        const auto last = static_cast<std::uint32_t>(size() - 1);
        std::uint32_t old_node = none;
        if (mRoot != none)
        {
            old_node = mReferences[dense].node;
            unlink(dense);
            if (dense != last)
                remapReference(last, dense);
        }
        const Status status = mEntries.remove(handle);
        if (size() <= mConfig.smallSetThreshold)
            resetTopology();
        else
        {
            refitPath(old_node);
            maybeMerge(old_node);
        }
        return status;
    }

    Status setFilter(HandleType handle, Filter entryFilter) noexcept
    {
        mEntries.requireValid(handle, "setFilter");
        const std::uint32_t dense = mEntries.denseOf(handle);
        if (mEntries.mMeta[dense].filter == entryFilter)
            return Status::Ok;
        mEntries.mMeta[dense].filter = entryFilter;
        if (mRoot != none)
        {
            changedItems(mReferences[dense].node, true);
            includeItem(mReferences[dense].node, dense);
            refitPath(mReferences[dense].node);
        }
        return Status::Ok;
    }

    void clear() noexcept
    {
        mEntries.clear();
        resetTopology();
        mExtent = mConfig.root;
    }

    Status reserve(const Reservation& reservation) noexcept
    {
        const std::size_t references = std::max(reservation.entries, reservation.references);
        if (reservation.entries > mConfig.maxEntries || reservation.nodes > mConfig.maxNodes ||
            references > Traits::slotLimit || reservation.descriptors > Traits::slotLimit)
            return Status::SizeLimitExceeded;
        detail::Buffer<Node> nodes(resource());
        detail::Buffer<Reference> refs(resource());
        detail::Buffer<Key> keys(resource()), temp(resource());
        Status status = mNodes.prepare(nodes, reservation.nodes);
        if (status == Status::Ok) status = mReferences.prepare(refs, references);
        if (status == Status::Ok) status = mKeys.prepare(keys, reservation.descriptors);
        if (status == Status::Ok) status = mTemp.prepare(temp, reservation.descriptors);
        if (status == Status::Ok) status = mEntries.reserve(reservation);
        if (status != Status::Ok)
            return status;
        mNodes.commit(nodes);
        mReferences.commit(refs);
        mKeys.commit(keys);
        mTemp.commit(temp);
        return Status::Ok;
    }

    Status shrink() noexcept
    {
        if (empty())
        {
            const Status status = mEntries.shrink();
            if (status != Status::Ok)
                return status;
            mNodes.reset();
            mReferences.reset();
            mKeys.reset();
            mTemp.reset();
            resetTopology();
            mExtent = mConfig.root;
            return Status::Ok;
        }
        Workspace<T> scratch(resource());
        Status status = scratch.reserve({0, 0, size()});
        if (status != Status::Ok)
            return status;
        auto new_extent = mExtent;
        if (mConfig.extent == Extent::Adaptive || (mConfig.extent == Extent::Snapshot &&
            mConfig.snapshotGrowth == SnapshotGrowth::Adaptive))
            new_extent = dataExtent();
        const Cell cell{*new_extent};
        std::size_t node_count = 0;
        if (size() > mConfig.smallSetThreshold)
        {
            for (std::uint32_t i = 0; i < size(); ++i)
                scratch.mKeys[i] = makeKey(cell, 0, i, mEntries.mBoxes[i]);
            detail::radixSort(scratch.mKeys.data(), scratch.tempKeys(size()), size());
            node_count = countNodes(scratch.mKeys.data(), 0, size(), 0);
            if (node_count > mConfig.maxNodes)
                return Status::CapacityExhausted;
        }
        detail::Buffer<Node> nodes(resource());
        detail::Buffer<Reference> refs(resource());
        status = nodes.allocate(node_count);
        if (status == Status::Ok) status = refs.allocate(node_count == 0 ? 0 : size());
        if (status == Status::Ok) status = mEntries.shrink();
        if (status != Status::Ok)
            return status;
        mNodes.swap(nodes);
        mReferences.swap(refs);
        resetTopology();
        mExtent = new_extent;
        if (node_count != 0)
            rebuild(scratch.mKeys.data(), size(), cell);
        mKeys.reset();
        mTemp.reset();
        return Status::Ok;
    }

    IndexStats stats() const noexcept
    {
        IndexStats result;
        result.entries = size();
        result.nodes = mActiveNodes;
        result.references = mRoot == none ? 0 : size();
        result.entryCapacity = mEntries.capacity();
        result.nodeCapacity = mNodes.capacity();
        result.referenceCapacity = mReferences.capacity();
        result.descriptorCapacity = mKeys.capacity();
        result.nodeBytes = sizeof(Node);
        result.referenceBytes = sizeof(Reference);
        result.descriptorBytes = 2 * sizeof(Key);
        result.liveBytes = size() * (sizeof(Box<T>) + sizeof(Payload) + sizeof(typename Store::Meta)) +
            std::size_t(mEntries.mSlotsUsed) * sizeof(typename Store::Slot) +
            mEntries.mRankCount * sizeof(typename Store::RankCell) + mActiveNodes * sizeof(Node) +
            result.references * sizeof(Reference);
        result.retainedBytes = mEntries.retainedBytes() + mNodes.bytes() + mReferences.bytes() +
            mKeys.bytes() + mTemp.bytes();
        if (mRoot != none)
            activeDepth(mRoot, result.maxDepth);
        return result;
    }

    template<typename R> requires detail::BuildRange<R, Payload, T> && std::is_nothrow_copy_constructible_v<Payload>
    Status build(R&& range, BuildOptions<T> options = {}) noexcept
    {
        return buildRange<false, false>(range, detail::AcceptAll{}, std::nullopt, options);
    }

    template<typename R> requires detail::BuildRange<R, Payload, T> && std::is_nothrow_copy_constructible_v<Payload>
    Status build(R&& range, std::span<HandleType> handles, BuildOptions<T> options = {}) noexcept
    {
        return buildRange<false, false>(range, detail::AcceptAll{}, handles, options);
    }

    template<typename R> requires detail::MutableBuildRange<R, Payload, T>
    Status buildConsume(R&& range, BuildOptions<T> options = {}) noexcept
    {
        return buildRange<true, false>(range, detail::AcceptAll{}, std::nullopt, options);
    }

    template<typename R> requires detail::MutableBuildRange<R, Payload, T>
    Status buildConsume(R&& range, std::span<HandleType> handles, BuildOptions<T> options = {}) noexcept
    {
        return buildRange<true, false>(range, detail::AcceptAll{}, handles, options);
    }

    // Payload ranges with a borrowed geometry accessor; default filters, no ranks.
    template<typename R, typename Accessor> requires detail::PayloadRange<R, Payload> &&
        std::is_nothrow_copy_constructible_v<Payload> && std::is_nothrow_invocable_v<Accessor&, const Payload&> &&
        detail::Geometry<std::invoke_result_t<Accessor&, const Payload&>, T>
    Status buildWith(R&& range, Accessor&& accessor, BuildOptions<T> options = {}) noexcept
    {
        return buildRange<false, true>(range, accessor, std::nullopt, options);
    }

    template<typename R, typename Accessor> requires detail::PayloadRange<R, Payload> &&
        (!std::is_const_v<std::remove_reference_t<std::ranges::range_reference_t<R>>>) &&
        std::is_nothrow_invocable_v<Accessor&, const Payload&> &&
        detail::Geometry<std::invoke_result_t<Accessor&, const Payload&>, T>
    Status buildConsumeWith(R&& range, Accessor&& accessor, BuildOptions<T> options = {}) noexcept
    {
        return buildRange<true, true>(range, accessor, std::nullopt, options);
    }
    template<typename Visitor> requires std::is_nothrow_invocable_r_v<Visit, Visitor&, const NodeView<T>&>
    QueryResult forEachNode(Visitor&& visitor) const noexcept
    {
        Context context({});
        if (mRoot != none)
            visitNodes(mRoot, context, visitor);
        return context.result;
    }

    template<typename Visitor> requires std::is_nothrow_invocable_r_v<Visit, Visitor&, const View&>
    QueryResult forEachEntry(Visitor&& visitor) const noexcept
    {
        Context context({});
        for (std::uint32_t i = 0; i < size() && context.active(); ++i)
            reportEntry(i, context, visitor);
        return context.result;
    }

    template<typename G, typename Visitor, typename Predicate = detail::AcceptAll>
        requires detail::Geometry<G, T> && std::is_nothrow_invocable_r_v<Visit, Visitor&, const View&> &&
            std::is_nothrow_invocable_r_v<bool, Predicate&, const View&>
    QueryResult query(const G& geometry, Visitor&& visitor, QueryOptions<T> options = {}, Predicate&& predicate = {}) const noexcept
    {
        return regionQuery(geometry, std::nullopt, visitor, options, predicate);
    }

    template<typename G, typename Visitor, typename Predicate = detail::AcceptAll>
        requires detail::Geometry<G, T> && std::is_nothrow_invocable_r_v<Visit, Visitor&, const View&> &&
            std::is_nothrow_invocable_r_v<bool, Predicate&, const View&>
    QueryResult query(const G& geometry, Filter queryFilter, Visitor&& visitor, QueryOptions<T> options = {},
        Predicate&& predicate = {}) const noexcept
    {
        return regionQuery(geometry, queryFilter, visitor, options, predicate);
    }

    template<typename Visitor, typename Predicate = detail::AcceptAll>
        requires std::is_nothrow_invocable_r_v<Visit, Visitor&, const View&, const View&> &&
            std::is_nothrow_invocable_r_v<bool, Predicate&, const View&, const View&>
    QueryResult forEachPair(Visitor&& visitor, QueryOptions<T> options = {}, Predicate&& predicate = {}) const noexcept
    {
        if (size() < 2)
            return {options.ordered && !allRanked() ? Status::MissingRank : Status::Ok, {}};
        if (options.ordered && !allRanked())
            return {Status::MissingRank, {}};
        Context context(options.budget);
        if (options.ordered)
        {
            Workspace<T> local(resource());
            Workspace<T>* scratch = options.workspace ? options.workspace : &local;
            const Status prepared = queryWorkspace(*scratch, options.workspace == nullptr, size(), 0);
            if (prepared != Status::Ok)
                return {prepared, {}};
            typename Workspace<T>::Use use(*scratch);
            orderedSelf(*scratch, false, context, visitor, predicate);
        }
        else if (mRoot != none)
            selfPairs(mRoot, context, visitor, predicate);
        else if (size() <= 4 && options.budget.candidateTests >= size() * (size() - 1) / 2)
            tinyPairs(context, visitor, predicate);
        else
            for (std::uint32_t a = 0; a < size() && context.active(); ++a)
                for (std::uint32_t b = a + 1; b < size() && context.active(); ++b)
                    testPair<FloatCoordinate<T>>(a, b, false, context, visitor, predicate);
        return context.result;
    }

    template<typename Visitor, typename Predicate = detail::AcceptAll>
        requires std::is_nothrow_invocable_r_v<Visit, Visitor&, const View&, const View&> &&
            std::is_nothrow_invocable_r_v<bool, Predicate&, const View&, const View&>
    QueryResult forEachPairInvolving(std::span<const HandleType> moved, Visitor&& visitor,
        QueryOptions<T> options = {}, Predicate&& predicate = {}) const noexcept
    {
        // In particular, no O(n) rank or membership preparation for an empty set.
        if (moved.empty())
            return {};
        for (HandleType handle : moved)
            mEntries.requireValid(handle, "forEachPairInvolving");
        if (moved.size() > size())
            return {Status::DuplicateHandle, {}};
        Workspace<T> local(resource());
        Workspace<T>* scratch = options.workspace ? options.workspace : &local;
        const Status prepared = queryWorkspace(*scratch, options.workspace == nullptr, size(), moved.size(), options.ordered);
        if (prepared != Status::Ok)
            return {prepared, {}};
        typename Workspace<T>::Use use(*scratch);
        scratch->nextEpoch();
        for (std::size_t i = 0; i < moved.size(); ++i)
        {
            const std::uint32_t dense = mEntries.denseOf(moved[i]);
            if (scratch->mMembership[dense] == scratch->mEpoch)
                return {Status::DuplicateHandle, {}};
            scratch->mMembership[dense] = scratch->mEpoch;
            if (options.ordered)
                scratch->mSelections[dense] = 1; // Dirty; source-only marks use 2
            scratch->mMoved[i] = dense;
        }
        Context context(options.budget);
        if (options.ordered)
        {
            if (!allRanked())
                return {Status::MissingRank, {}};
            orderedMoved(*scratch, moved.size(), context, visitor, predicate);
        }
        else
            for (std::size_t i = 0; i < moved.size() && context.active(); ++i)
            {
                const std::uint32_t a = scratch->mMoved[i];
                auto candidate = [&](std::uint32_t b) noexcept
                {
                    if (a == b || (scratch->mMembership[b] == scratch->mEpoch && entryKey(b) < entryKey(a)))
                        return;
                    testPair(a, b, false, context, visitor, predicate);
                };
                scan(mEntries.mBoxes[a], mEntries.mMeta[a].filter, context, candidate);
            }
        return context.result;
    }

    template<typename Visitor, typename Predicate = detail::AcceptAll>
        requires std::is_nothrow_invocable_r_v<Visit, Visitor&, const View&> &&
            std::is_nothrow_invocable_r_v<bool, Predicate&, const View&, const View&>
    QueryResult queryPartners(HandleType handle, Visitor&& visitor, QueryOptions<T> options = {},
        Predicate&& predicate = {}) const noexcept
    {
        mEntries.requireValid(handle, "queryPartners");
        const std::uint32_t a = mEntries.denseOf(handle);
        if ((options.ordered || options.greaterRank || options.lessRank) && !allRanked())
            return {Status::MissingRank, {}};
        Workspace<T> local(resource());
        Workspace<T>* scratch = options.workspace ? options.workspace : &local;
        if (options.ordered)
        {
            const Status prepared = queryWorkspace(*scratch, options.workspace == nullptr, size(), 0);
            if (prepared != Status::Ok)
                return {prepared, {}};
        }
        // No workspace is touched by the unordered partner path.
        Context context(options.budget);
        std::size_t count = 0;
        const auto discover = [&]() noexcept
        {
            auto candidate = [&](std::uint32_t b) noexcept
            {
                if (a == b)
                    return;
                const auto r = mEntries.rankAt(b);
                if ((options.greaterRank && *r <= *options.greaterRank) ||
                    (options.lessRank && *r >= *options.lessRank))
                    return;
                if (!pairQualifies(a, b, options.ordered, context, predicate))
                    return;
                if (options.ordered)
                    scratch->mRow[count++] = b;
                else
                    reportEntry(b, context, visitor);
            };
            scan(mEntries.mBoxes[a], mEntries.mMeta[a].filter, context, candidate);
        };
        if (options.ordered)
        {
            typename Workspace<T>::Use use(*scratch);
            discover();
            if (context.active())
            {
                sortRanks(scratch->mRow.data(), count);
                for (std::size_t i = 0; i < count && context.active(); ++i)
                    reportEntry(scratch->mRow[i], context, visitor);
            }
        }
        else
            discover();
        return context.result;
    }

    template<StorablePayload OtherPayload, Coordinate OtherT, DiagnosticsPolicy OtherPolicy, bool OtherInstrumented,
        typename Visitor, typename Predicate = detail::AcceptAll>
        requires std::is_nothrow_invocable_r_v<Visit, Visitor&, const View&,
            const typename Index<OtherPayload, OtherT, OtherPolicy, OtherInstrumented>::View&> &&
            std::is_nothrow_invocable_r_v<bool, Predicate&, const View&,
                const typename Index<OtherPayload, OtherT, OtherPolicy, OtherInstrumented>::View&>
    QueryResult forEachPair(const Index<OtherPayload, OtherT, OtherPolicy, OtherInstrumented>& other, Visitor&& visitor,
        QueryOptions<T> options = {}, Predicate&& predicate = {}) const noexcept
    {
        if constexpr (!std::same_as<T, OtherT>)
            return {Status::IncompatiblePolicy, {}};
        else
        {
            if (static_cast<const void*>(this) == static_cast<const void*>(&other))
                return {Status::InvalidArgument, {}};
            if (mConfig.boundary != other.mConfig.boundary)
                return {Status::IncompatiblePolicy, {}};
            if (options.ordered && (!allRanked() || !other.allRanked()))
                return {Status::MissingRank, {}};
            Workspace<T> local(resource());
            Workspace<T>* scratch = options.workspace ? options.workspace : &local;
            if (options.ordered)
            {
                const Status prepared = queryWorkspace(*scratch, options.workspace == nullptr,
                    std::max(size(), other.size()), 0);
                if (prepared != Status::Ok)
                    return {prepared, {}};
                typename Workspace<T>::Use use(*scratch);
                return crossEnumeration(other, *scratch, false, true, options.budget, visitor, predicate);
            }
            return crossEnumeration(other, *scratch, false, false, options.budget, visitor, predicate);
        }
    }

    template<typename Left, typename Right, typename Visitor, typename Predicate = detail::AcceptAll>
        requires std::is_nothrow_invocable_r_v<bool, Left&, const View&> &&
            std::is_nothrow_invocable_r_v<bool, Right&, const View&> &&
            std::is_nothrow_invocable_r_v<Visit, Visitor&, const View&, const View&> &&
            std::is_nothrow_invocable_r_v<bool, Predicate&, const View&, const View&>
    QueryResult forEachPairSelected(Left&& left, Right&& right, Visitor&& visitor, QueryOptions<T> options = {},
        Predicate&& predicate = {}) const noexcept
    {
        Workspace<T> local(resource());
        Workspace<T>* scratch = options.workspace ? options.workspace : &local;
        const Status prepared = queryWorkspace(*scratch, options.workspace == nullptr, size(), 0, options.ordered);
        if (prepared != Status::Ok)
            return {prepared, {}};
        if (scratch->mSelections.capacity() < size())
            return {Status::InsufficientWorkspace, {}};
        typename Workspace<T>::Use use(*scratch);
        bool overlapping = false, missing_rank = false;
        for (std::uint32_t i = 0; i < size(); ++i)
        {
            const View entry = view(i);
            const bool in_left = std::invoke(left, entry), in_right = std::invoke(right, entry);
            const auto flags = static_cast<std::uint8_t>((in_left ? 1 : 0) | (in_right ? 2 : 0));
            scratch->mSelections[i] = flags;
            overlapping = overlapping || flags == 3;
            missing_rank = missing_rank || (flags != 0 && !entry.rank);
        }
        if (overlapping)
            return {Status::InvalidArgument, {}};
        if (options.ordered && missing_rank)
            return {Status::MissingRank, {}};
        return crossEnumeration(*this, *scratch, true, options.ordered, options.budget, visitor, predicate);
    }

    // Appends handles; output operations must be nonthrowing, like visitors.
    template<typename G, typename Output> requires detail::Geometry<G, T> &&
        requires(Output output, HandleType handle) { *output++ = handle; }
    QueryResult queryTo(const G& geometry, Output output, QueryOptions<T> options = {}) const noexcept
    {
        return query(geometry, [&](const View& entry) noexcept
            { *output++ = entry.handle; return Continue; }, options);
    }

    template<typename Output> requires requires(Output output, HandlePair pair) { *output++ = pair; }
    QueryResult pairsTo(Output output, QueryOptions<T> options = {}) const noexcept
    {
        return forEachPair([&](const View& a, const View& b) noexcept
            { *output++ = HandlePair{a.handle, b.handle}; return Continue; }, options);
    }

    // Bounded output; use the written prefix only on complete().
    template<typename G> requires detail::Geometry<G, T>
    CollectResult queryTo(const G& geometry, std::span<HandleType> out, QueryOptions<T> options = {}) const noexcept
    {
        std::size_t count = 0;
        const auto result = query(geometry, [&](const View& entry) noexcept
        {
            if (count == out.size()) return Stop;
            out[count++] = entry.handle;
            return Continue;
        }, options);
        return {result.status == Status::Stopped ? Status::InsufficientOutput : result.status, count, result.counters};
    }

    CollectResult pairsTo(std::span<HandlePair> out, QueryOptions<T> options = {}) const noexcept
    {
        std::size_t count = 0;
        const auto result = forEachPair([&](const View& a, const View& b) noexcept
        {
            if (count == out.size()) return Stop;
            out[count++] = HandlePair{a.handle, b.handle};
            return Continue;
        }, options);
        return {result.status == Status::Stopped ? Status::InsufficientOutput : result.status, count, result.counters};
    }

private:
    Config<T> mConfig;
    Store mEntries;
    detail::Buffer<Node> mNodes;
    detail::Buffer<Reference> mReferences;
    detail::Buffer<Key> mKeys, mTemp;
    std::optional<Box<T>> mExtent;
    std::uint32_t mRoot = none;
    std::uint32_t mNodeUsed = 0;
    std::size_t mActiveNodes = 0;
    std::uint32_t mFreeBlocks = none;

    explicit Index(const Config<T>& config, Resource* resource) noexcept:
        mConfig(config), mEntries(resource, config.hardCapacity), mNodes(mEntries.resource()),
        mReferences(mEntries.resource()), mKeys(mEntries.resource()), mTemp(mEntries.resource()), mExtent(config.root) {}

    View view(std::uint32_t dense) const noexcept
    {
        const auto& meta = mEntries.mMeta[dense];
        const auto handle = detail::HandleAccess::make<Store>(mEntries.mOwner, meta.slot,
            mEntries.mSlots[meta.slot].generation);
        View result{handle, mEntries.mBoxes[dense], mEntries.mPayloads[dense], meta.filter, {}};
        if (meta.hasRank)
            result.rank.emplace(meta.rank);
        return result;
    }

    std::uint32_t entryKey(std::uint32_t dense) const noexcept { return mEntries.mMeta[dense].slot; }

    Status mutationExtent(const Box<T>& box, std::optional<Box<T>>& result) const noexcept
    {
        result = mExtent;
        const bool reject = mConfig.extent == Extent::Fixed || (mConfig.extent == Extent::Snapshot &&
            mConfig.snapshotGrowth == SnapshotGrowth::Reject);
        if (reject)
            return result && detail::contains(*result, box) ? Status::Ok : Status::OutOfBounds;
        if (!result)
            result = box;
        else if (!detail::contains(*result, box))
            result = detail::padded(detail::unite(*result, box));
        return Status::Ok;
    }

    Box<T> dataExtent() const noexcept
    {
        Box<T> box = mEntries.mBoxes[0];
        for (std::uint32_t i = 1; i < size(); ++i)
            box = detail::unite(box, mEntries.mBoxes[i]);
        return box;
    }

    Key makeKey(Cell cell, std::uint32_t baseDepth, std::uint32_t dense, const Box<T>& box) const noexcept
    {
        Key key{0, 0, 0, dense};
        while (baseDepth + key.level < mConfig.maxDepth)
        {
            const detail::Split<T> split(cell);
            if (!split.progress() || !split.fits(cell, box))
                break;
            const std::uint32_t q = split.quadrant(box);
            if (key.level < 32)
                key.hi |= std::uint64_t(q) << (62 - 2 * key.level);
            else
                key.lo |= std::uint64_t(q) << (126 - 2 * key.level);
            ++key.level;
            cell = split.child(cell, q);
        }
        return key;
    }

    Key makeBuildKey(const Cell& cell, std::uint32_t input, const Box<T>& box) const noexcept
    {
        if constexpr (!IntegerCoordinate<T>)
            return makeKey(cell, 0, input, box);
        else
        {
            // All build entries are contained in the root. Store the center
            // relative to the cell's low edge, and halve the unsigned span.
            using U = std::make_unsigned_t<T>;
            const U width = detail::span(box.minX, box.maxX);
            const U height = detail::span(box.minY, box.maxY);
            U x = detail::span(cell.box.minX, box.minX) + width / 2;
            U y = detail::span(cell.box.minY, box.minY) + height / 2;
            U w = detail::span(cell.box.minX, cell.box.maxX);
            U h = detail::span(cell.box.minY, cell.box.maxY);
            Key key{0, 0, 0, input};
            while (key.level < mConfig.maxDepth)
            {
                const U mx = w / 2, my = h / 2;
                if ((mx | my) == 0 || width > (mx != 0 ? mx : w) || height > (my != 0 ? my : h))
                    break;
                std::uint32_t q = 0;
                if (mx != 0)
                {
                    if (x >= mx) { q = 1; x -= mx; w -= mx; }
                    else w = mx;
                }
                if (my != 0)
                {
                    if (y >= my) { q |= 2; y -= my; h -= my; }
                    else h = my;
                }
                if constexpr (sizeof(T) <= 4)
                    key.hi = (key.hi << 2) | q;
                else if (key.level < 32)
                    key.hi = (key.hi << 2) | q;
                else
                    key.lo = (key.lo << 2) | q;
                ++key.level;
            }
            if (key.level != 0 && key.level < 32)
                key.hi <<= 64 - 2 * key.level;
            if (key.level > 32 && key.level < 64)
                key.lo <<= 128 - 2 * key.level;
            return key;
        }
    }

    static std::uint32_t digit(const Key& key, std::uint32_t level) noexcept
    {
        return static_cast<std::uint32_t>(level < 32 ? (key.hi >> (62 - 2 * level)) & 3u :
            (key.lo >> (126 - 2 * level)) & 3u);
    }

    struct Cut
    {
        bool split = false;
        std::size_t own = 0;
        std::size_t bound[5] = {};
    };

    Cut cut(const Key* keys, std::size_t begin, std::size_t end, std::uint32_t level) const noexcept
    {
        Cut result;
        result.own = end;
        if (end - begin <= mConfig.splitThreshold || level == 64)
            return result;
        std::size_t own = begin;
        while (own < end && keys[own].level == level)
            ++own;
        if (end - own <= mConfig.splitThreshold ||
            (keys[own].hi == keys[end - 1].hi && keys[own].lo == keys[end - 1].lo))
            return result; // Coincident/nonproductive prefixes do not build unary chains.
        result.split = true;
        result.own = own;
        result.bound[0] = own;
        result.bound[4] = end;
        for (std::uint32_t q = 1; q < 4; ++q)
        {
            std::size_t lo = result.bound[q - 1], hi = end;
            while (lo < hi)
            {
                const std::size_t mid = lo + (hi - lo) / 2;
                if (digit(keys[mid], level) < q)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            result.bound[q] = lo;
        }
        return result;
    }

    std::size_t countNodes(const Key* keys, std::size_t begin, std::size_t end, std::uint32_t level) const noexcept
    {
        const Cut ranges = cut(keys, begin, end, level);
        std::size_t count = 1;
        if (ranges.split)
            for (std::uint32_t q = 0; q < 4; ++q)
            {
                const std::size_t child = countNodes(keys, ranges.bound[q], ranges.bound[q + 1], level + 1);
                if (child > mConfig.maxNodes || count > mConfig.maxNodes - child)
                    return mConfig.maxNodes + 1;
                count += child;
            }
        return count;
    }

    // Largest child extent along one axis, as Split::fits computes it. For integers the
    // smaller half of an exact midpoint split is the unsigned span / 2.
    static auto fitSpan(bool split, decltype(detail::span(T{}, T{})) whole, T lo, T mid, T hi) noexcept
    {
        if constexpr (IntegerCoordinate<T>)
            return split ? whole / 2 : whole;
        else
            return split ? std::min(detail::span(lo, mid), detail::span(mid, hi)) : whole;
    }

    // Top-down bulk plan. Stable permutations and tags use compact temporary streams.
    // Only internal nodes write records to the key array. Placement matches makeKey:
    // integers use exact unsigned halving, floats evaluate Split::fits/quadrant per entry.
    // NearMid: every coordinate lies within +-max/2, so std::midpoint is (lo + hi) / 2 (its
    // overflow-free branch in libstdc++ and libc++). Integers ignore it.
    template<bool NearMid = false>
    bool planBulk(const Cell& cell, std::uint32_t depth, std::uint32_t begin, std::uint32_t end,
        const Box<T>* boxes, std::uint32_t* src, std::uint32_t* dst, std::uint32_t* order,
        Key* keys, std::uint8_t* tags,
        std::size_t planCapacity, std::size_t& plans, bool& overflow) const noexcept
    {
        const auto finish = [&]() noexcept
        {
            if (src != order)
                std::copy(src + begin, src + end, order + begin);
            return false;
        };
        if (end - begin <= mConfig.splitThreshold || depth == mConfig.maxDepth)
            return finish();
        const detail::Split<T> split(cell);
        if (!split.progress())
            return finish();
        const auto w = detail::span(cell.box.minX, cell.box.maxX);
        const auto h = detail::span(cell.box.minY, cell.box.maxY);
        const auto fits_width = fitSpan(split.splitX, w, cell.box.minX, split.x, cell.box.maxX);
        const auto fits_height = fitSpan(split.splitY, h, cell.box.minY, split.y, cell.box.maxY);
        const auto length = end - begin;
        const auto middle = length < 32 ? begin : begin + length / 2;
        std::uint32_t count[5]{}, lowerCount[5]{};
        const auto classify = [&](std::uint32_t from, std::uint32_t to, std::uint32_t (&bins)[5]) noexcept
        {
            for (auto i = from; i < to; ++i)
            {
#if defined(__GNUC__) || defined(__clang__)
                constexpr std::uint32_t ahead = 16;
                if (end - i > ahead)
                    __builtin_prefetch(&boxes[src[i + ahead]], 0, 3);
#endif
                const auto& box = boxes[src[i]];
                const auto width = detail::span(box.minX, box.maxX);
                const auto height = detail::span(box.minY, box.maxY);
                std::uint32_t b = 0;
                if (width <= fits_width && height <= fits_height)
                {
                    if constexpr (IntegerCoordinate<T>)
                    {
                        const auto x = box.minX + static_cast<T>(width / 2);
                        const auto y = box.minY + static_cast<T>(height / 2);
                        b = 1 + ((split.splitX && x >= split.x ? 1u : 0u) |
                            (split.splitY && y >= split.y ? 2u : 0u));
                    }
                    else if constexpr (NearMid)
                    {
                        const T x = (box.minX + box.maxX) / 2, y = (box.minY + box.maxY) / 2;
                        b = 1 + ((split.splitX && x >= split.x ? 1u : 0u) | (split.splitY && y >= split.y ? 2u : 0u));
                    }
                    else
                        b = 1 + split.quadrant(box);
                }
                tags[i] = static_cast<std::uint8_t>(b);
                ++bins[b];
            }
        };
        if (middle != begin)
            classify(begin, middle, lowerCount);
        classify(middle, end, count);
        for (unsigned b = 0; b < 5; ++b)
            count[b] += lowerCount[b];
        const auto movable = end - begin - count[0];
        if (movable <= mConfig.splitThreshold)
            return finish();
        if (count[1] == movable || count[2] == movable || count[3] == movable || count[4] == movable)
        {
            // A single occupied quadrant can still split lower down. Match
            // cut()'s padded-prefix equality test without generating keys
            // for groups that already diverge at this level.
            Key first{};
            bool seen = false, distinct = false;
            for (auto i = begin; i < end; ++i)
            {
                if (tags[i] == 0)
                    continue;
                const auto key = makeKey(cell, depth, 0, boxes[src[i]]);
                if (!seen) { first = key; seen = true; }
                else if (first.hi != key.hi || first.lo != key.lo) { distinct = true; break; }
            }
            if (!distinct)
                return finish();
        }
        if (plans == planCapacity)
        {
            overflow = true;
            return false;
        }
        const auto plan = plans++;
        const std::uint32_t edges[5]{begin + count[0], begin + count[0] + count[1],
            begin + count[0] + count[1] + count[2], end - count[4], end};
        std::uint32_t next[5]{begin, edges[0], edges[1], edges[2], edges[3]};
        if (middle == begin)
        {
            for (auto i = begin; i < end; ++i)
                dst[next[tags[i]]++] = src[i];
        }
        else
        {
            // Each tag's lower-half slice precedes its upper-half slice.
            // The two independent cursors may advance in interleaved order.
            std::uint32_t upper[5]{begin + lowerCount[0], edges[0] + lowerCount[1],
                edges[1] + lowerCount[2], edges[2] + lowerCount[3], edges[3] + lowerCount[4]};
            for (auto i = begin, j = middle; i < middle; ++i, ++j)
            {
                const auto a = tags[i], b = tags[j];
                const auto x = src[i], y = src[j];
                const auto lower = next[a]++, higher = upper[b]++;
                dst[lower] = x;
                dst[higher] = y;
            }
            if ((length & 1u) != 0)
                dst[upper[tags[end - 1]]] = src[end - 1];
        }
        if (dst != order)
            std::copy(dst + begin, dst + edges[0], order + begin);
        std::uint32_t children = 0;
        for (std::uint32_t q = 0; q < 4; ++q)
        {
            if (planBulk<NearMid>(split.child(cell, q), depth + 1, edges[q], edges[q + 1], boxes,
                dst, src, order, keys, tags, planCapacity, plans, overflow))
                children |= 1u << q;
            if (overflow)
                return false;
        }
        keys[plan].hi = (std::uint64_t(edges[0]) << 32) | edges[1];
        keys[plan].lo = (std::uint64_t(edges[2]) << 32) | edges[3];
        keys[plan].level = children;
        return true;
    }

    void resetTopology() noexcept
    {
        mRoot = none;
        mNodeUsed = 0;
        mActiveNodes = 0;
        mFreeBlocks = none;
    }

    std::uint32_t takeBlock() noexcept
    {
        std::uint32_t first;
        if (mFreeBlocks != none)
        {
            first = mFreeBlocks;
            mFreeBlocks = mNodes[first].firstChild;
        }
        else
        {
            first = mNodeUsed;
            mNodeUsed += 4;
        }
        mActiveNodes += 4;
        return first;
    }

    void freeBlock(std::uint32_t first) noexcept
    {
        mNodes[first].firstChild = mFreeBlocks;
        mFreeBlocks = first;
        mActiveNodes -= 4;
    }

    void changedItems(std::uint32_t n, bool stale) noexcept
    {
        Node& node = mNodes[n];
        if (node.splitRetry != 0)
            --node.splitRetry;
        if (node.ownCount == 0)
        {
            node.itemBounds = {};
            node.itemSummary = {0, 0};
            node.itemDirty = false;
            node.itemChanges = 0;
            node.splitRetry = 0;
        }
        else
        {
            node.itemDirty = node.itemDirty || stale;
            if (node.itemDirty && node.itemChanges != none)
                ++node.itemChanges;
        }
    }

    void includeItem(std::uint32_t n, std::uint32_t dense) noexcept
    {
        Node& node = mNodes[n];
        const Box<T>& box = mEntries.mBoxes[dense];
        const Filter f = mEntries.mMeta[dense].filter;
        if (node.ownCount == 1)
        {
            node.itemBounds = box;
            node.itemSummary = f;
            node.itemDirty = false;
            node.itemChanges = 0;
        }
        else
        {
            node.itemBounds = detail::unite(node.itemBounds, box);
            node.itemSummary.category |= f.category;
            node.itemSummary.mask |= f.mask;
        }
    }

    void link(std::uint32_t node, std::uint32_t dense) noexcept
    {
        Reference& ref = mReferences[dense];
        mNodes[node].linear = mNodes[node].ownCount == 0 ||
            (mNodes[node].linear && dense + 1 == mNodes[node].head);
        ref = {node, none, mNodes[node].head};
        if (ref.next != none)
            mReferences[ref.next].previous = dense;
        mNodes[node].head = dense;
        ++mNodes[node].ownCount;
        changedItems(node, false);
        includeItem(node, dense);
    }

    void unlink(std::uint32_t dense) noexcept
    {
        const Reference ref = mReferences[dense];
        mNodes[ref.node].linear = false;
        if (ref.previous == none)
            mNodes[ref.node].head = ref.next;
        else
            mReferences[ref.previous].next = ref.next;
        if (ref.next != none)
            mReferences[ref.next].previous = ref.previous;
        --mNodes[ref.node].ownCount;
        changedItems(ref.node, true);
    }

    void remapReference(std::uint32_t oldDense, std::uint32_t newDense) noexcept
    {
        const Reference ref = mReferences[oldDense];
        mNodes[ref.node].linear = false;
        if (ref.previous == none)
            mNodes[ref.node].head = newDense;
        else
            mReferences[ref.previous].next = newDense;
        if (ref.next != none)
            mReferences[ref.next].previous = newDense;
        mReferences[newDense] = ref;
    }

    std::uint32_t nextItem(const Node& node, std::uint32_t dense) const noexcept
    {
        if (!node.linear)
            return mReferences[dense].next;
        return dense + 1 == node.head + node.ownCount ? none : dense + 1;
    }

    void refit(std::uint32_t n, bool items = true) noexcept
    {
        Node& node = mNodes[n];
        node.count = node.ownCount;
        if (items && node.itemDirty && (node.ownCount <= 8 ||
            node.itemChanges >= std::max<std::uint32_t>(1, node.ownCount / 2)))
        {
            if constexpr (requires { Policy::onItemRefit(std::size_t{}); })
            {
                static_assert(noexcept(Policy::onItemRefit(std::size_t{})) &&
                    std::same_as<decltype(Policy::onItemRefit(std::size_t{})), void>,
                    "QuadTree: Policy::onItemRefit() must be noexcept and return void");
                Policy::onItemRefit(node.ownCount);
            }
            bool any = false;
            node.itemSummary = {0, 0};
            for (std::uint32_t i = node.head; i != none; i = nextItem(node, i))
            {
                node.itemBounds = any ? detail::unite(node.itemBounds, mEntries.mBoxes[i]) : mEntries.mBoxes[i];
                const Filter f = mEntries.mMeta[i].filter;
                node.itemSummary.category |= f.category;
                node.itemSummary.mask |= f.mask;
                any = true;
            }
            node.itemDirty = false;
            node.itemChanges = 0;
        }
        bool any = node.ownCount != 0;
        node.bounds = node.itemBounds;
        node.summary = node.itemSummary;
        if (node.firstChild != none)
            for (std::uint32_t q = 0; q < 4; ++q)
            {
                const Node& child = mNodes[node.firstChild + q];
                if (child.count == 0)
                    continue;
                node.count += child.count;
                node.bounds = any ? detail::unite(node.bounds, child.bounds) : child.bounds;
                node.summary.category |= child.summary.category;
                node.summary.mask |= child.summary.mask;
                any = true;
            }
    }

    void refitPath(std::uint32_t n) noexcept
    {
        if (n == none)
            return;
        refit(n);
        for (n = mNodes[n].parent; n != none; n = mNodes[n].parent)
            refit(n, false);
    }

    void buildDenseItems(std::uint32_t n, std::uint32_t begin, std::uint32_t end) noexcept
    {
        if (begin == end)
            return;
        auto& node = mNodes[n];
        auto bounds = mEntries.mBoxes[begin];
        Filter summary{0, 0};
        for (auto i = begin; i < end; ++i)
        {
            mReferences[i] = {n, i == begin ? none : i - 1, i + 1 == end ? none : i + 1};
            bounds = detail::unite(bounds, mEntries.mBoxes[i]);
            const auto filter = mEntries.mMeta[i].filter;
            summary.category |= filter.category;
            summary.mask |= filter.mask;
        }
        node.head = begin;
        node.ownCount = end - begin;
        node.itemBounds = bounds;
        node.itemSummary = summary;
    }

    void materializeBulk(std::uint32_t n, const Key*& plan, bool splitNode, std::uint32_t begin,
        std::uint32_t end) noexcept
    {
        if (splitNode)
        {
            const auto& info = *plan++;
            const std::uint32_t edges[5]{static_cast<std::uint32_t>(info.hi >> 32),
                static_cast<std::uint32_t>(info.hi), static_cast<std::uint32_t>(info.lo >> 32),
                static_cast<std::uint32_t>(info.lo), end};
            buildDenseItems(n, begin, edges[0]);
            const auto base = takeBlock();
            mNodes[n].firstChild = base;
            const detail::Split<T> split(mNodes[n].cell);
            for (std::uint32_t q = 0; q < 4; ++q)
            {
                // Reinitialize this trivially destructible pool slot directly.
                ::new (static_cast<void*>(mNodes.data() + base + q)) Node{
                    .cell = split.child(mNodes[n].cell, q), .parent = n,
                    .depth = static_cast<std::uint8_t>(mNodes[n].depth + 1)};
                materializeBulk(base + q, plan, (info.level & (1u << q)) != 0, edges[q], edges[q + 1]);
            }
        }
        else
        {
            buildDenseItems(n, begin, end);
            if (mNodes[n].ownCount > mConfig.splitThreshold)
                mNodes[n].splitRetry = std::max<std::uint32_t>(1, mNodes[n].ownCount / 2);
        }
        refit(n);
    }

    template<bool DenseBuild = false>
    void materialize(std::uint32_t n, const Key* keys, std::size_t begin, std::size_t end,
        std::uint32_t level) noexcept
    {
        const Cut ranges = cut(keys, begin, end, level);
        if constexpr (DenseBuild)
            buildDenseItems(n, static_cast<std::uint32_t>(begin), static_cast<std::uint32_t>(ranges.own));
        else
            for (std::size_t i = ranges.own; i != begin;)
                link(n, keys[--i].entry);
        if (ranges.split)
        {
            const std::uint32_t first = takeBlock();
            mNodes[n].firstChild = first;
            const detail::Split<T> split(mNodes[n].cell);
            for (std::uint32_t q = 0; q < 4; ++q)
            {
                Node node;
                node.cell = split.child(mNodes[n].cell, q);
                node.parent = n;
                node.depth = static_cast<std::uint8_t>(mNodes[n].depth + 1);
                mNodes[first + q] = node;
                materialize<DenseBuild>(first + q, keys, ranges.bound[q], ranges.bound[q + 1], level + 1);
            }
        }
        else if (mNodes[n].ownCount > mConfig.splitThreshold)
            mNodes[n].splitRetry = std::max<std::uint32_t>(1, mNodes[n].ownCount / 2);
        refit(n);
    }

    template<bool DenseBuild = false>
    void rebuild(const Key* keys, std::size_t count, const Cell& cell) noexcept
    {
        resetTopology();
        mRoot = 0;
        mNodeUsed = 1;
        mActiveNodes = 1;
        mNodes[0] = Node{};
        mNodes[0].cell = cell;
        materialize<DenseBuild>(0, keys, 0, count, 0);
    }

    std::uint32_t destination(const Box<T>& box) const noexcept
    {
        std::uint32_t n = mRoot;
        while (mNodes[n].firstChild != none)
        {
            const detail::Split<T> split(mNodes[n].cell);
            if (!split.fits(mNodes[n].cell, box))
                break;
            n = mNodes[n].firstChild + split.quadrant(box);
        }
        return n;
    }

    bool belongs(std::uint32_t n, const Box<T>& box) const noexcept
    {
        const Node& node = mNodes[n];
        if (!detail::centerInside(node.cell, box))
            return false;
        if (node.parent != none && !detail::Split<T>(mNodes[node.parent].cell).fits(mNodes[node.parent].cell, box))
            return false;
        return node.firstChild == none || !detail::Split<T>(node.cell).fits(node.cell, box);
    }

    struct Mutation
    {
        bool rebuildAll = false;
        std::uint32_t target = none;
        std::size_t records = 0;
        std::size_t plannedNodes = 0;
        std::size_t requiredNodes = 0;
        Cell cell;
    };

    static std::size_t grown(std::size_t capacity, std::size_t needed, std::size_t limit) noexcept
    {
        return std::max(needed, std::min(limit, capacity <= limit / 2 ? std::max<std::size_t>(8, capacity * 2) : limit));
    }

    Status scratchFor(std::size_t count) noexcept
    {
        if (mKeys.capacity() >= count && mTemp.capacity() >= count)
            return Status::Ok;
        if (mConfig.hardCapacity)
            return Status::CapacityExhausted;
        detail::Buffer<Key> keys(resource()), temp(resource());
        const std::size_t capacity = grown(mKeys.capacity(), count, mConfig.maxEntries);
        Status status = mKeys.prepare(keys, capacity);
        if (status == Status::Ok) status = mTemp.prepare(temp, capacity);
        if (status == Status::Ok)
        {
            mKeys.commit(keys);
            mTemp.commit(temp);
        }
        return status;
    }

    Status prepareMutation(std::uint32_t dense, const Box<T>& box, const std::optional<Box<T>>& nextExtent,
        bool inserting, Mutation& plan) noexcept
    {
        const std::size_t next_size = size() + (inserting ? 1u : 0u);
        if (next_size <= mConfig.smallSetThreshold)
            return Status::Ok;
        plan.rebuildAll = mRoot == none || nextExtent != mExtent;
        std::uint32_t base_depth = 0;
        if (plan.rebuildAll)
        {
            plan.cell = Cell{*nextExtent};
            plan.records = next_size;
        }
        else
        {
            plan.target = destination(box);
            const Node& node = mNodes[plan.target];
            plan.requiredNodes = mActiveNodes;
            if (node.firstChild != none)
                return Status::Ok;
            plan.records = std::size_t(node.ownCount) + 1;
            if (!inserting && mReferences[dense].node == plan.target)
                --plan.records;
            if (plan.records <= mConfig.splitThreshold || node.depth >= mConfig.maxDepth || node.splitRetry != 0)
            {
                plan.records = 0;
                return Status::Ok;
            }
            plan.cell = node.cell;
            base_depth = node.depth;
        }
        const Status status = scratchFor(plan.records);
        if (status != Status::Ok)
            return status;
        if constexpr (requires { Policy::onSplitPreparation(std::size_t{}); })
        {
            static_assert(noexcept(Policy::onSplitPreparation(std::size_t{})) &&
                std::same_as<decltype(Policy::onSplitPreparation(std::size_t{})), void>,
                "QuadTree: Policy::onSplitPreparation() must be noexcept and return void");
            Policy::onSplitPreparation(plan.records);
        }
        std::size_t fill = 0;
        if (plan.rebuildAll)
        {
            for (std::uint32_t i = 0; i < next_size; ++i)
                mKeys[fill++] = makeKey(plan.cell, base_depth, i, i == dense ? box : mEntries.mBoxes[i]);
        }
        else
        {
            for (std::uint32_t i = mNodes[plan.target].head; i != none; i = mReferences[i].next)
                if (i != dense)
                    mKeys[fill++] = makeKey(plan.cell, base_depth, i, mEntries.mBoxes[i]);
            mKeys[fill++] = makeKey(plan.cell, base_depth, dense, box);
        }
        detail::radixSort(mKeys.data(), mTemp.data(), fill);
        plan.plannedNodes = countNodes(mKeys.data(), 0, fill, 0);
        const std::size_t old_nodes = plan.rebuildAll ? 0 : mActiveNodes - 1;
        if (plan.plannedNodes > mConfig.maxNodes || old_nodes > mConfig.maxNodes - plan.plannedNodes)
            return Status::CapacityExhausted;
        plan.requiredNodes = old_nodes + plan.plannedNodes;
        return Status::Ok;
    }

    Status prepareStorage(std::size_t nodeCount, std::size_t entryCount, detail::Buffer<Node>& nodes,
        detail::Buffer<Reference>& references) const noexcept
    {
        if (nodeCount == 0)
            return Status::Ok;
        if (mConfig.hardCapacity && (nodeCount > mNodes.capacity() || entryCount > mReferences.capacity()))
            return Status::CapacityExhausted;
        Status status = mNodes.prepare(nodes, nodeCount > mNodes.capacity() ?
            grown(mNodes.capacity(), nodeCount, mConfig.maxNodes) : 0);
        if (status == Status::Ok)
            status = mReferences.prepare(references, entryCount > mReferences.capacity() ?
                grown(mReferences.capacity(), entryCount, mConfig.maxEntries) : 0);
        return status;
    }

    void commitMutation(std::uint32_t dense, const Mutation& plan, std::uint32_t oldNode) noexcept
    {
        if (plan.requiredNodes == 0)
            return;
        if (plan.rebuildAll)
        {
            rebuild(mKeys.data(), plan.records, plan.cell);
            return;
        }
        if (oldNode != none && (oldNode != plan.target || plan.plannedNodes > 1))
            unlink(dense);
        if (plan.plannedNodes > 1)
        {
            const Node old = mNodes[plan.target];
            mNodes[plan.target] = Node{};
            mNodes[plan.target].cell = old.cell;
            mNodes[plan.target].parent = old.parent;
            mNodes[plan.target].depth = old.depth;
            materialize(plan.target, mKeys.data(), 0, plan.records, 0);
        }
        else if (oldNode == plan.target)
        {
            changedItems(plan.target, true);
            includeItem(plan.target, dense);
        }
        else
            link(plan.target, dense);
        if (plan.plannedNodes == 1)
            mNodes[plan.target].splitRetry = std::max<std::uint32_t>(1, mNodes[plan.target].ownCount / 2);
        if (oldNode != none && oldNode != plan.target)
            refitPath(oldNode);
        refitPath(plan.target);
        if (oldNode != none && oldNode != plan.target)
            maybeMerge(oldNode);
    }

    void gather(std::uint32_t child, std::uint32_t target) noexcept
    {
        for (std::uint32_t i = mNodes[child].head; i != none;)
        {
            const std::uint32_t next = mReferences[i].next;
            link(target, i);
            i = next;
        }
        const std::uint32_t first = mNodes[child].firstChild;
        if (first != none)
        {
            for (std::uint32_t q = 0; q < 4; ++q)
                gather(first + q, target);
            freeBlock(first);
        }
    }

    void maybeMerge(std::uint32_t n) noexcept
    {
        std::uint32_t target = none;
        for (std::uint32_t i = n; i != none; i = mNodes[i].parent)
            if (mNodes[i].firstChild != none && (mNodes[i].count <= mConfig.mergeThreshold ||
                mNodes[i].count == mNodes[i].ownCount))
                target = i;
        if (target != none)
        {
            const std::uint32_t first = mNodes[target].firstChild;
            mNodes[target].firstChild = none;
            for (std::uint32_t q = 0; q < 4; ++q)
                gather(first + q, target);
            freeBlock(first);
            refitPath(target);
        }
    }

    void activeDepth(std::uint32_t n, std::uint32_t& depth) const noexcept
    {
        if (mNodes[n].count == 0)
            return;
        depth = std::max<std::uint32_t>(depth, mNodes[n].depth);
        if (mNodes[n].firstChild != none)
            for (std::uint32_t q = 0; q < 4; ++q)
                activeDepth(mNodes[n].firstChild + q, depth);
    }

    template<bool Consume, bool Projected, typename R, typename Accessor>
    Status buildRange(R& range, Accessor&& accessor, std::optional<std::span<HandleType>> handles,
        BuildOptions<T> options) noexcept
    {
        if (std::ranges::empty(range))
        {
            // A previously cleared index has no identity or topology to retire.
            if (mEntries.mOwner != 0)
            {
                mEntries.clear();
                resetTopology();
            }
            if (mConfig.extent != Extent::Fixed)
                mExtent.reset();
            return Status::Ok;
        }
        std::size_t count = 0;
        if constexpr (std::ranges::sized_range<R>)
        {
            const auto length = std::ranges::size(range);
            if (length > mConfig.maxEntries)
                return Status::SizeLimitExceeded;
            count = static_cast<std::size_t>(length);
        }
        else
            for (auto it = std::ranges::begin(range); it != std::ranges::end(range); ++it)
            {
                if (count == mConfig.maxEntries)
                    return Status::SizeLimitExceeded;
                ++count;
            }
        if (handles && handles->size() < count)
            return Status::InsufficientOutput;
        if (options.workspace)
            return buildPrepared<Consume, Projected>(range, accessor, handles, count, *options.workspace);
        return buildWithLocalWorkspace<Consume, Projected>(range, accessor, handles, count);
    }

    template<bool Consume, bool Projected, typename R, typename Accessor>
    Status buildWithLocalWorkspace(R& range, Accessor& accessor, std::optional<std::span<HandleType>> handles,
        std::size_t count) noexcept
    {
        Workspace<T> local(resource());
        const Status reserved = local.reserve({0, 0, count});
        if (reserved != Status::Ok)
            return reserved;
        return buildPrepared<Consume, Projected>(range, accessor, handles, count, local);
    }

    template<bool Consume, bool Projected, typename R, typename Accessor>
    Status buildPrepared(R& range, Accessor& accessor, std::optional<std::span<HandleType>> handles,
        std::size_t count, Workspace<T>& workspace) noexcept
    {
        auto* scratch = &workspace;
        if (scratch->mBuildBoxes.capacity() < count || scratch->mDescriptors.capacity() < count ||
            scratch->mKeys.capacity() < count || scratch->tempKeyCapacity() < count)
            return Status::InsufficientWorkspace;
        typename Workspace<T>::Use use(*scratch);
        if constexpr (!Projected && !Consume &&
            std::ranges::contiguous_range<R> &&
            std::same_as<std::ranges::range_value_t<R>, BuildEntry<Payload, T>> &&
            std::is_trivially_copy_constructible_v<Payload> && std::is_trivially_destructible_v<Payload> &&
            !requires { Policy::acquireIdentity(); })
        {
            if (count <= mConfig.smallSetThreshold && count <= mEntries.mCapacity &&
                count <= mEntries.mSlotCapacity && mConfig.extent != Extent::Fixed)
            {
                if (auto result = buildFlatInteger(std::ranges::data(range), static_cast<std::uint32_t>(count), handles))
                    return *result;
            }
        }
        if (count == 1 && mConfig.smallSetThreshold != 0)
            return buildSingle<Consume, Projected>(*std::ranges::begin(range), accessor, handles);
        auto* boxes = scratch->mBuildBoxes.data();
        auto* descriptors = scratch->mDescriptors.data();
        auto* ranks = scratch->tempRanks(count);
        std::size_t i = 0;
        std::uint32_t ranked = 0;
        std::optional<Box<T>> next_extent = mConfig.extent == Extent::Fixed ? mConfig.root : std::nullopt;
        for (auto& entry : range)
        {
            auto& box = boxes[i];
            auto& descriptor = descriptors[i];
            if (!captureBuildEntry<Projected>(entry, accessor, box, descriptor))
                return Status::InvalidGeometry;
            if (mConfig.extent == Extent::Fixed)
            {
                if (!detail::contains(*next_extent, box))
                    return Status::OutOfBounds;
            }
            else
                next_extent = next_extent ? detail::unite(*next_extent, box) : box;
            if (descriptor.rank)
                ranks[ranked++] = *descriptor.rank;
            ++i;
        }
        // Independent preparation: no payload moves, topology mutation, or output writes.
        if (ranked > 1 && !detail::uniqueBuildRanks(ranks, ranks + ranked, ranked))
            return Status::DuplicateRank;
        std::size_t node_count = 0;
        const std::uint32_t* bulk_order = nullptr;
        if (count > mConfig.smallSetThreshold)
        {
            const Cell cell{*next_extent};
            {
                auto [order, tags] = scratch->tempPermutation(count);
                for (i = 0; i < count; ++i)
                    order[i] = static_cast<std::uint32_t>(i);
                const auto nodeLimit = (mConfig.maxNodes - 1) / 4;
                std::size_t plans = 0;
                bool overflow = false;
                const auto plan = [&](auto nearMid) noexcept
                {
                    planBulk<decltype(nearMid)::value>(cell, 0, 0, static_cast<std::uint32_t>(count), boxes, order,
                        order + count, order, scratch->mKeys.data(), tags, std::min(count, nodeLimit), plans, overflow);
                };
                if constexpr (FloatCoordinate<T>)
                {
                    constexpr T limit = std::numeric_limits<T>::max() / 2;
                    const auto& e = *next_extent;
                    if (-limit <= e.minX && e.maxX <= limit && -limit <= e.minY && e.maxY <= limit)
                        plan(std::true_type{});
                    else
                        plan(std::false_type{});
                }
                else
                    plan(std::false_type{});
                if (overflow && plans == nodeLimit)
                    return Status::CapacityExhausted;
                if (!overflow)
                {
                    bulk_order = order;
                    node_count = 1 + 4 * plans;
                }
            }
            if (!bulk_order)
            {
                for (i = 0; i < count; ++i)
                    scratch->mKeys[i] = makeBuildKey(cell, static_cast<std::uint32_t>(i), boxes[i]);
                detail::radixSort(scratch->mKeys.data(), scratch->tempKeys(count), count);
                node_count = countNodes(scratch->mKeys.data(), 0, count, 0);
                if (node_count > mConfig.maxNodes)
                    return Status::CapacityExhausted;
            }
        }
        const auto spatialEntry = [&](std::size_t dense) noexcept
        {
            return bulk_order ? bulk_order[dense] : scratch->mKeys[dense].entry;
        };
        const Status storage = prepareReplacement(count, ranked, node_count);
        if (storage != Status::Ok)
            return storage;
        // Commit is infallible: validated input, preallocated topology/rank/slot
        // storage, unique ranks, a reserved identity and nonthrowing payloads.
        const std::uint64_t owner = mEntries.mOwner;
        // Tree-less indexes already have reset topology; avoid rewriting it on each flat build.
        if (mRoot != none)
            resetTopology();
        mExtent = next_extent;
        if constexpr (Consume ? std::is_trivially_move_constructible_v<Payload> :
            std::is_trivially_copy_constructible_v<Payload>)
        {
            if (node_count == 0 && ranked == 0)
            {
                // No rank insertion or client construction observes a partially built store.
                // Publish the dense/slot counts once, after constructing every entry.
                const auto size = static_cast<std::uint32_t>(count);
                for (std::uint32_t dense = 0; dense < size; ++dense)
                    constructBuildEntry<Consume>(dense, boxes[dense], descriptors[dense], false);
                mEntries.mSize = size;
                mEntries.mSlotsUsed = size;
                if (handles)
                    for (std::uint32_t dense = 0; dense < size; ++dense)
                        (*handles)[dense] = detail::HandleAccess::make<Store>(owner, dense, 0);
                return Status::Ok;
            }
        }
        for (i = 0; i < count; ++i)
        {
#if defined(__GNUC__) || defined(__clang__)
            if constexpr (!Consume && !Projected && std::is_trivially_copy_constructible_v<Payload>)
            {
                if (node_count != 0 && ranked == 0)
                {
                    // Start the indexed descriptor fetch before the dependent payload fetch.
                    constexpr std::size_t descriptorAhead = 32, payloadAhead = 16;
                    if (count - i > descriptorAhead)
                    {
                        const auto future = spatialEntry(i + descriptorAhead);
                        __builtin_prefetch(&boxes[future], 0, 3);
                        __builtin_prefetch(&descriptors[future], 0, 3);
                        __builtin_prefetch(&descriptors[future].payload, 0, 3);
                    }
                    if (count - i > payloadAhead)
                        __builtin_prefetch(descriptors[spatialEntry(i + payloadAhead)].payload, 0, 3);
                }
            }
#endif
            // Spatially sorted dense entries keep snapshot geometry contiguous.
            const std::uint32_t input = node_count == 0 ? static_cast<std::uint32_t>(i) : spatialEntry(i);
            const auto& descriptor = descriptors[input];
            const auto dense = static_cast<std::uint32_t>(i);
            constructBuildEntry<Consume>(dense, boxes[input], descriptor, descriptor.rank.has_value());
            ++mEntries.mSize;
            ++mEntries.mSlotsUsed;
            if (descriptor.rank)
            {
                mEntries.insertRank(*descriptor.rank);
                ++mEntries.mRankCount;
            }
            if (handles)
                (*handles)[input] = detail::HandleAccess::make<Store>(owner, dense, 0);
        }
        if (bulk_order)
        {
            mRoot = 0;
            mNodeUsed = 1;
            mActiveNodes = 1;
            mNodes[0] = Node{};
            mNodes[0].cell = Cell{*next_extent};
            const Key* plan = scratch->mKeys.data();
            materializeBulk(0, plan, node_count != 1, 0, static_cast<std::uint32_t>(count));
        }
        else if (node_count != 0)
            rebuild<true>(scratch->mKeys.data(), count, Cell{*next_extent});
        return Status::Ok;
    }

    // Adding +0 maps -0 to +0 and leaves every other finite value unchanged (normalizeZero).
    static Box<T> canonicalZero(Box<T> box) noexcept requires FloatCoordinate<T>
    {
        return {box.minX + T(0), box.minY + T(0), box.maxX + T(0), box.maxY + T(0)};
    }

    // Same acceptance and result as detail::canonicalize; floats use one branch-free test per box.
    static bool canonicalBox(Box<T>& box) noexcept
    {
        if constexpr (FloatCoordinate<T>)
        {
            constexpr T lo = std::numeric_limits<T>::lowest(), hi = std::numeric_limits<T>::max();
            const bool ok = (box.minX >= lo) & (box.minY >= lo) & (box.maxX <= hi) & (box.maxY <= hi) &
                (box.minX <= box.maxX) & (box.minY <= box.maxY);
            box = canonicalZero(box);
            return ok;
        }
        else
            return detail::canonicalize(box);
    }

    // Warm, unranked contiguous boxes need no descriptor copy: integer edges are
    // already canonical, floats store their canonical copy (signed zeros normalized).
    // The commit neither allocates nor invokes client code.
    // A ranked entry delegates to the general path before any state changes.
    std::optional<Status> buildFlatInteger(const BuildEntry<Payload, T>* input, std::uint32_t count,
        std::optional<std::span<HandleType>> handles) noexcept
    {
        auto extent = input[0].rect;
        if (input[0].rank)
            return std::nullopt;
        if (!canonicalBox(extent))
            return Status::InvalidGeometry;
        for (std::uint32_t i = 1; i < count; ++i)
        {
            auto box = input[i].rect;
            if (input[i].rank)
                return std::nullopt;
            if (!canonicalBox(box))
                return Status::InvalidGeometry;
            extent = detail::unite(extent, box);
        }
        const auto owner = detail::acquireModuleIdentityInline();
        if (owner == 0)
            return Status::IdentityExhausted;
        // Payload destruction is trivial. Reset the slot/rank bookkeeping,
        // then publish the new identity and counts without clearing them first.
        mEntries.resetSlots();
        if (mRoot != none)
            resetTopology();
        mExtent = extent;
        if (count == 1)
        {
            std::construct_at(mEntries.mBoxes, extent);
            std::construct_at(mEntries.mPayloads, input[0].payload);
            std::construct_at(mEntries.mMeta, typename Store::Meta{input[0].filter, 0, 0, false});
            std::construct_at(mEntries.mSlots, typename Store::Slot{0, 0});
        }
        else
        {
            for (std::size_t i = 0; i < count; ++i)
            {
                if constexpr (FloatCoordinate<T>)
                    std::construct_at(mEntries.mBoxes + i, canonicalZero(input[i].rect)); // Validated above.
                else
                    std::construct_at(mEntries.mBoxes + i, input[i].rect);
            }
            for (std::size_t i = 0; i < count; ++i)
                std::construct_at(mEntries.mPayloads + i, input[i].payload);
            for (std::uint32_t i = 0; i < count; ++i)
                std::construct_at(mEntries.mMeta + i, typename Store::Meta{input[i].filter, 0, i, false});
            for (std::uint32_t i = 0; i < count; ++i)
                std::construct_at(mEntries.mSlots + i, typename Store::Slot{0, i});
        }
        mEntries.mOwner = owner;
        mEntries.mSize = count;
        mEntries.mSlotsUsed = count;
        if (handles)
            for (std::uint32_t i = 0; i < count; ++i)
                (*handles)[i] = detail::HandleAccess::make<Store>(owner, i, 0);
        return Status::Ok;
    }

    template<bool Projected, typename Entry, typename Accessor>
    static bool captureBuildEntry(Entry& entry, Accessor& accessor, Box<T>& box,
        detail::Descriptor& descriptor) noexcept
    {
        if constexpr (Projected)
        {
            const auto geometry = std::invoke(accessor, std::as_const(entry));
            if (!detail::normalize(geometry, box))
                return false;
            descriptor.payload = std::addressof(entry);
            descriptor.filter = {};
            descriptor.rank.reset();
        }
        else
        {
            if constexpr (FloatCoordinate<T> && std::same_as<std::remove_cvref_t<decltype(entry.rect)>, Box<T>>)
            {
                box = entry.rect;
                if (!canonicalBox(box))
                    return false;
            }
            else if (!detail::normalize(entry.rect, box))
                return false;
            descriptor.payload = std::addressof(entry.payload);
            descriptor.filter = entry.filter;
            descriptor.rank = entry.rank;
        }
        return true;
    }

    template<bool Consume, bool Projected, typename Entry, typename Accessor>
    Status buildSingle(Entry& entry, Accessor& accessor, std::optional<std::span<HandleType>> handles) noexcept
    {
        Box<T> box;
        detail::Descriptor descriptor;
        if (!captureBuildEntry<Projected>(entry, accessor, box, descriptor))
            return Status::InvalidGeometry;
        const bool fixed = mConfig.extent == Extent::Fixed;
        if (fixed && !detail::contains(*mConfig.root, box))
            return Status::OutOfBounds;
        const bool ranked = descriptor.rank.has_value();
        const Status storage = prepareReplacement(1, ranked, 0);
        if (storage != Status::Ok)
            return storage;
        if (mRoot != none)
            resetTopology();
        mExtent = fixed ? *mConfig.root : box;
        constructBuildEntry<Consume>(0, box, descriptor, ranked);
        mEntries.mSize = 1;
        mEntries.mSlotsUsed = 1;
        if (ranked)
        {
            mEntries.insertRank(*descriptor.rank);
            ++mEntries.mRankCount;
        }
        if (handles)
            (*handles)[0] = detail::HandleAccess::make<Store>(mEntries.mOwner, 0, 0);
        return Status::Ok;
    }

    template<bool Consume>
    void constructBuildEntry(std::uint32_t dense, const Box<T>& box, const detail::Descriptor& descriptor,
        bool ranked) noexcept
    {
        const auto* value = static_cast<const Payload*>(descriptor.payload);
        if constexpr (Consume)
            std::construct_at(mEntries.mPayloads + dense, std::move(*const_cast<Payload*>(value)));
        else
            std::construct_at(mEntries.mPayloads + dense, *value);
        std::construct_at(mEntries.mBoxes + dense, box);
        std::construct_at(mEntries.mMeta + dense, typename Store::Meta{descriptor.filter,
            ranked ? *descriptor.rank : 0, dense, ranked});
        std::construct_at(mEntries.mSlots + dense, typename Store::Slot{0, dense});
    }

    // Prepared replacements need neither a pending storage transaction nor its cleanup.
    Status prepareReplacement(std::size_t count, std::size_t ranked, std::size_t nodeCount) noexcept
    {
        const auto rankCapacity = Store::rankCapacityFor(ranked);
        if (count <= mEntries.mCapacity && count <= mEntries.mSlotCapacity && rankCapacity <= mEntries.mRankCapacity &&
            (nodeCount == 0 || (nodeCount <= mNodes.capacity() && count <= mReferences.capacity())))
        {
            const auto owner = Traits::acquireIdentity();
            if (owner == 0)
                return Status::IdentityExhausted;
            mEntries.clear();
            mEntries.mOwner = owner;
            return Status::Ok;
        }
        if (mConfig.hardCapacity)
            return Status::CapacityExhausted;
        return growReplacement(count, rankCapacity, nodeCount);
    }

    Status growReplacement(std::size_t count, std::uint64_t rankCapacity, std::size_t nodeCount) noexcept
    {
        detail::Buffer<Node> nodes(resource());
        detail::Buffer<Reference> refs(resource());
        Status status = prepareStorage(nodeCount, count, nodes, refs);
        typename Store::Pending entries;
        if (status == Status::Ok)
            status = mEntries.allocate(entries, count > mEntries.mCapacity ? static_cast<std::uint32_t>(count) : 0,
                count > mEntries.mSlotCapacity ? count : 0,
                rankCapacity > mEntries.mRankCapacity ? rankCapacity : 0);
        if (status != Status::Ok)
            return status;
        const auto owner = Traits::acquireIdentity();
        if (owner == 0)
        {
            mEntries.release(entries);
            return Status::IdentityExhausted;
        }
        mEntries.clear();
        mEntries.commit(entries, 0);
        mEntries.mOwner = owner;
        mNodes.commit(nodes);
        mReferences.commit(refs);
        return Status::Ok;
    }

    struct Context
    {
        QueryResult result;
        WorkBudget remaining;

        explicit Context(WorkBudget budget) noexcept: remaining(budget) {}
        bool active() const noexcept { return result.status == Status::Ok; }

        bool charge(std::uint64_t& budget, std::uint64_t& counter) noexcept
        {
            if (budget == 0)
            {
                result.status = Status::BudgetExhausted;
                return false;
            }
            if (budget != std::numeric_limits<std::uint64_t>::max())
                --budget;
            if constexpr (Instrumented)
                detail::increment(counter);
            return true;
        }
        bool node() noexcept { return charge(remaining.nodeVisits, result.counters.nodeVisits); }
        bool nodes(std::uint64_t count) noexcept
        {
            auto& budget = remaining.nodeVisits;
            const auto max = std::numeric_limits<std::uint64_t>::max();
            bool complete = true;
            if (budget != max)
            {
                complete = count <= budget;
                if (!complete)
                {
                    count = budget;
                    result.status = Status::BudgetExhausted;
                }
                budget -= count;
            }
            if constexpr (Instrumented)
            {
                auto& visits = result.counters.nodeVisits;
                visits += std::min(count, max - visits);
            }
            return complete;
        }
        bool candidate() noexcept { return charge(remaining.candidateTests, result.counters.candidateTests); }
        void filtered() noexcept
        {
            if constexpr (Instrumented) detail::increment(result.counters.filterRejections);
        }
        void predicate() noexcept
        {
            if constexpr (Instrumented) detail::increment(result.counters.predicateCalls);
        }
        void report(Visit control) noexcept
        {
            if constexpr (Instrumented) detail::increment(result.counters.reports);
            if (control == Stop)
                result.status = Status::Stopped;
        }
    };

    bool allRanked() const noexcept { return mEntries.mRankCount == size(); }

    static Status queryWorkspace(Workspace<T>& scratch, bool grow, std::size_t entries, std::size_t moved,
        bool ordered = true) noexcept
    {
        if (grow)
        {
            const Status reserved = scratch.reserve({entries, moved, 0});
            if (reserved != Status::Ok)
                return reserved;
        }
        if ((ordered && (scratch.mOrder.capacity() < entries || scratch.mRow.capacity() < entries)) ||
            (moved != 0 && (scratch.mMembership.capacity() < entries || scratch.mMoved.capacity() < moved ||
                (ordered && scratch.mSelections.capacity() < entries))))
            return Status::InsufficientWorkspace;
        return Status::Ok;
    }

    void sortRanks(std::uint32_t* order, std::size_t count) const noexcept
    {
        detail::rankSort(order, count, [this](std::uint32_t a, std::uint32_t b) noexcept
            { return mEntries.mMeta[a].rank < mEntries.mMeta[b].rank; });
    }

    template<typename C, typename Callback>
    void walk(std::uint32_t n, const Box<T>& box, std::optional<Filter> queryFilter, C& context,
        Callback& callback) const noexcept
    {
        const Node& node = mNodes[n];
        if (!context.active() || node.count == 0 || !context.node())
            return;
        if (!detail::closedOverlap(box, node.bounds))
            return;
        if (queryFilter && !detail::compatible(*queryFilter, node.summary))
        {
            context.filtered();
            return;
        }
        for (std::uint32_t i = node.head; i != none && context.active(); i = nextItem(node, i))
            callback(i);
        if (node.firstChild != none)
            for (std::uint32_t q = 0; q < 4 && context.active(); ++q)
                walk(node.firstChild + q, box, queryFilter, context, callback);
    }

    static constexpr unsigned countChildren(unsigned mask) noexcept
    {
        // Only four bits are live. Avoid a general popcount helper on targets
        // where the compiler cannot emit a hardware instruction.
        return (0x4332322132212110ULL >> (mask * 4)) & 15u;
    }

    // The caller has screened geometry and charged this nonempty node.
    template<typename Callback>
    void walkOrdered(const Node& node, const Box<T>& box, Filter filter, Context& context,
        Callback& callback) const noexcept
    {
        if (!detail::compatible(filter, node.summary))
        {
            context.filtered();
            return;
        }
        for (std::uint32_t i = node.head; i != none && context.active(); i = nextItem(node, i))
            callback(i);
        if (node.firstChild == none || !context.active())
            return;
        const Node* children = mNodes.data() + node.firstChild;
        unsigned occupied = 0, overlapping = 0;
        for (unsigned q = 0; q != 4; ++q)
        {
            const auto& child = children[q];
            if (child.count != 0)
            {
                occupied |= 1u << q;
                if (detail::closedOverlap(box, child.bounds))
                    overlapping |= 1u << q;
            }
        }
        while (overlapping != 0)
        {
            const auto q = static_cast<unsigned>(std::countr_zero(overlapping));
            const auto through = (2u << q) - 1u;
            // Charge skipped disjoint children and the next admitted child
            // in DFS order, never precharging siblings beyond that child.
            if (!context.nodes(countChildren(occupied & through)))
                return;
            walkOrdered(children[q], box, filter, context, callback);
            if (!context.active())
                return;
            occupied &= ~through;
            overlapping &= overlapping - 1;
        }
        if (occupied != 0)
            context.nodes(countChildren(occupied));
    }

    template<typename C, typename Callback>
    void scan(const Box<T>& box, std::optional<Filter> queryFilter, C& context, Callback& callback) const noexcept
    {
        if (mRoot != none)
            walk(mRoot, box, queryFilter, context, callback);
        else
            for (std::uint32_t i = 0; i < size() && context.active(); ++i)
                callback(i);
    }

    template<typename Visitor>
    void visitNodes(std::uint32_t n, Context& context, Visitor& visitor) const noexcept
    {
        const Node& node = mNodes[n];
        const NodeView<T> info{node.cell.box, node.bounds, node.summary, node.depth, node.count, node.ownCount};
        context.report(std::invoke(visitor, info));
        if (node.firstChild != none)
            for (std::uint32_t q = 0; q < 4 && context.active(); ++q)
                visitNodes(node.firstChild + q, context, visitor);
    }

    template<typename Visitor>
    void reportEntry(std::uint32_t dense, Context& context, Visitor& visitor) const noexcept
    {
        const View entry = view(dense);
        context.report(std::invoke(visitor, entry));
    }

    void orient(std::uint32_t& a, std::uint32_t& b, bool ordered) const noexcept
    {
        if (ordered ? mEntries.mMeta[b].rank < mEntries.mMeta[a].rank : entryKey(b) < entryKey(a))
            std::swap(a, b);
    }

    static bool packedCrossItemOverlap(const Box<T>& a, const Box<T>& b, Boundary boundary) noexcept
    {
#if defined(__SSE2__)
        if constexpr (std::same_as<T, std::int32_t>)
        {
            static_assert(sizeof(Box<T>) == sizeof(__m128i) && offsetof(Box<T>, minX) == 0 &&
                offsetof(Box<T>, minY) == 4 && offsetof(Box<T>, maxX) == 8 && offsetof(Box<T>, maxY) == 12);
            __m128i left, right;
            std::memcpy(&left, &a, sizeof(left));
            std::memcpy(&right, &b, sizeof(right));
            const auto lo = _mm_unpacklo_epi64(left, right);
            const auto hi = _mm_unpackhi_epi64(right, left);
            if (_mm_movemask_epi8(_mm_cmpgt_epi32(lo, hi)) != 0)
                return false;
            // No opposing endpoints are equal on a strict hit. Only contact needs the mode.
            return _mm_movemask_epi8(_mm_cmpeq_epi32(lo, hi)) == 0 || boundary == Boundary::TouchInclusive ||
                a.minX == a.maxX || a.minY == a.maxY || b.minX == b.maxX || b.minY == b.maxY;
        }
        else if constexpr (std::same_as<T, float>)
        {
            // Same lanes as int32. Stored floats are finite and signed zeros are canonical.
            static_assert(sizeof(Box<T>) == sizeof(__m128) && offsetof(Box<T>, minX) == 0 &&
                offsetof(Box<T>, minY) == 4 && offsetof(Box<T>, maxX) == 8 && offsetof(Box<T>, maxY) == 12);
            const auto left = _mm_loadu_ps(&a.minX), right = _mm_loadu_ps(&b.minX);
            const auto lo = _mm_movelh_ps(left, right); // a.min, b.min
            const auto hi = _mm_movehl_ps(left, right); // b.max, a.max
            if (_mm_movemask_ps(_mm_cmpgt_ps(lo, hi)) != 0)
                return false;
            return _mm_movemask_ps(_mm_cmpeq_ps(lo, hi)) == 0 || boundary == Boundary::TouchInclusive ||
                a.minX == a.maxX || a.minY == a.maxY || b.minX == b.maxX || b.minY == b.maxY;
        }
        else if constexpr (std::same_as<T, double>)
        {
            static_assert(sizeof(Box<T>) == 2 * sizeof(__m128d) && offsetof(Box<T>, minX) == 0 &&
                offsetof(Box<T>, minY) == 8 && offsetof(Box<T>, maxX) == 16 && offsetof(Box<T>, maxY) == 24);
            const auto aMin = _mm_loadu_pd(&a.minX), aMax = _mm_loadu_pd(&a.maxX);
            const auto bMin = _mm_loadu_pd(&b.minX), bMax = _mm_loadu_pd(&b.maxX);
            if (_mm_movemask_pd(_mm_or_pd(_mm_cmpgt_pd(aMin, bMax), _mm_cmpgt_pd(bMin, aMax))) != 0)
                return false;
            return _mm_movemask_pd(_mm_or_pd(_mm_cmpeq_pd(aMin, bMax), _mm_cmpeq_pd(bMin, aMax))) == 0 ||
                boundary == Boundary::TouchInclusive ||
                a.minX == a.maxX || a.minY == a.maxY || b.minX == b.maxX || b.minY == b.maxY;
        }
#endif
        return detail::overlaps(a, b, boundary);
    }

    template<bool PackedCross = false, typename Predicate>
    bool pairQualifies(std::uint32_t a, std::uint32_t b, bool ordered, Context& context, Predicate& predicate) const noexcept
    {
        if (!detail::compatible(mEntries.mMeta[a].filter, mEntries.mMeta[b].filter))
        {
            context.filtered();
            return false;
        }
        if (!context.candidate() || !(PackedCross && std::same_as<Predicate, detail::AcceptAll> ?
            packedCrossItemOverlap(mEntries.mBoxes[a], mEntries.mBoxes[b], mConfig.boundary) :
            detail::overlaps(mEntries.mBoxes[a], mEntries.mBoxes[b], mConfig.boundary)))
            return false;
        if constexpr (!std::same_as<Predicate, detail::AcceptAll>)
        {
            orient(a, b, ordered);
            context.predicate();
            const View left = view(a), right = view(b);
            return std::invoke(predicate, left, right);
        }
        return true;
    }

    template<typename Visitor>
    void reportPair(std::uint32_t a, std::uint32_t b, bool ordered, Context& context, Visitor& visitor) const noexcept
    {
        orient(a, b, ordered);
        const View left = view(a), right = view(b);
        context.report(std::invoke(visitor, left, right));
    }

    template<bool PackedCross = false, typename Visitor, typename Predicate>
    void testPair(std::uint32_t a, std::uint32_t b, bool ordered, Context& context, Visitor& visitor,
        Predicate& predicate) const noexcept
    {
        if (pairQualifies<PackedCross>(a, b, ordered, context, predicate))
            reportPair(a, b, ordered, context, visitor);
    }

    template<typename Visitor, typename Predicate>
    void tinyPairs(Context& context, Visitor& visitor, Predicate& predicate) const noexcept
    {
        const auto count = mEntries.mSize;
        const auto* boxes = mEntries.mBoxes;
        const auto* meta = mEntries.mMeta;
        for (std::uint32_t a = 0; a + 1 < count; ++a)
            for (std::uint32_t b = a + 1; b < count; ++b)
            {
                if (!detail::compatible(meta[a].filter, meta[b].filter))
                {
                    context.filtered();
                    continue;
                }
                // The caller checked that every possible pair fits the budget.
                if constexpr (Instrumented)
                    ++context.result.counters.candidateTests;
                const auto& x = boxes[a];
                const auto& y = boxes[b];
                // Strict overlap always qualifies; only contact needs the boundary and degeneracy checks.
                if (!(x.minX < y.maxX && y.minX < x.maxX && x.minY < y.maxY && y.minY < x.maxY) &&
                    !(detail::closedOverlap(x, y) && (mConfig.boundary == Boundary::TouchInclusive ||
                        x.minX == x.maxX || x.minY == x.maxY || y.minX == y.maxX || y.minY == y.maxY)))
                    continue;
                auto first = a, second = b;
                orient(first, second, false);
                const View left = view(first), right = view(second);
                if constexpr (!std::same_as<Predicate, detail::AcceptAll>)
                {
                    context.predicate();
                    if (!std::invoke(predicate, left, right))
                        continue;
                }
                context.report(std::invoke(visitor, left, right));
                if (!context.active())
                    return;
            }
    }

    template<typename G, typename Visitor, typename Predicate>
    QueryResult regionQuery(const G& geometry, std::optional<Filter> queryFilter, Visitor& visitor,
        QueryOptions<T> options, Predicate& predicate) const noexcept
    {
        Box<T> box{};
        if (!detail::normalize(geometry, box))
            return {Status::InvalidGeometry, {}};
        if (options.ordered && !allRanked())
            return {Status::MissingRank, {}};
        Workspace<T> local(resource());
        Workspace<T>* scratch = options.workspace ? options.workspace : &local;
        if (options.ordered)
        {
            const Status prepared = queryWorkspace(*scratch, options.workspace == nullptr, size(), 0);
            if (prepared != Status::Ok)
                return {prepared, {}};
        }
        Context context(options.budget);
        std::size_t count = 0;
        const auto discover = [&]() noexcept
        {
            auto candidate = [&](std::uint32_t i) noexcept
            {
                if (queryFilter && !detail::compatible(*queryFilter, mEntries.mMeta[i].filter))
                {
                    context.filtered();
                    return;
                }
                if (!context.candidate() || !detail::overlaps(box, mEntries.mBoxes[i], mConfig.boundary))
                    return;
                const View entry = view(i);
                if constexpr (!std::same_as<Predicate, detail::AcceptAll>)
                {
                    context.predicate();
                    if (!std::invoke(predicate, entry))
                        return;
                }
                if (options.ordered)
                    scratch->mRow[count++] = i;
                else
                    context.report(std::invoke(visitor, entry));
            };
            scan(box, queryFilter, context, candidate);
        };
        if (options.ordered)
        {
            typename Workspace<T>::Use use(*scratch);
            discover();
            if (context.active())
            {
                sortRanks(scratch->mRow.data(), count);
                for (std::size_t i = 0; i < count && context.active(); ++i)
                    reportEntry(scratch->mRow[i], context, visitor);
            }
        }
        else
            discover();
        return context.result;
    }

    void radixRankOrder(Workspace<T>& scratch, std::uint32_t* order, std::uint32_t count) const noexcept
    {
        auto* keys = scratch.mKeys.data();
        auto* temp = scratch.tempKeys(count);
        std::uint32_t counts[8][256]{};
        // Cache each rank once; all subsequent passes touch only scratch.
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto entry = order[i];
            const auto rank = mEntries.mMeta[entry].rank;
            keys[i].hi = rank;
            keys[i].entry = entry;
            for (unsigned byte = 0; byte < 8; ++byte)
                ++counts[byte][(rank >> (8 * byte)) & 255u];
        }
        for (unsigned byte = 0; byte < 8; ++byte)
        {
            const auto shift = 8 * byte;
            auto& bucket = counts[byte];
            if (bucket[(keys[0].hi >> shift) & 255u] == count)
                continue;
            std::uint32_t offset = 0;
            for (auto& size : bucket)
            {
                const auto previous = offset;
                offset += size;
                size = previous;
            }
            for (std::uint32_t i = 0; i < count; ++i)
            {
                const auto rank = keys[i].hi;
                const auto dst = bucket[(rank >> shift) & 255u]++;
                temp[dst].hi = rank;
                temp[dst].entry = keys[i].entry;
            }
            std::swap(keys, temp);
        }
        for (std::uint32_t i = 0; i < count; ++i)
            order[i] = keys[i].entry;
    }

    template<typename Visitor, typename Predicate>
    void orderedSelf(Workspace<T>& scratch, bool movedOnly, Context& context, Visitor& visitor,
        Predicate& predicate) const noexcept
    {
        const auto sourceCount = size();
        for (std::uint32_t i = 0; i < sourceCount; ++i)
            scratch.mOrder[i] = i;
        // Reuse only already reserved build scratch; smaller queries keep index sort.
        if (!movedOnly && sourceCount >= 4096 && sourceCount <= scratch.mKeys.capacity() &&
            sourceCount <= scratch.tempKeyCapacity())
            radixRankOrder(scratch, scratch.mOrder.data(), static_cast<std::uint32_t>(sourceCount));
        else
            sortRanks(scratch.mOrder.data(), sourceCount);
        for (std::size_t i = 0; i + 1 < size() && context.active(); ++i)
        {
            const std::uint32_t a = scratch.mOrder[i];
            std::size_t count = 0;
            auto candidate = [&](std::uint32_t b) noexcept
            {
                if (mEntries.mMeta[b].rank <= mEntries.mMeta[a].rank ||
                    (movedOnly && scratch.mMembership[a] != scratch.mEpoch && scratch.mMembership[b] != scratch.mEpoch))
                    return;
                if (pairQualifies<true>(a, b, true, context, predicate))
                    scratch.mRow[count++] = b;
            };
            if (!movedOnly && mRoot != none)
            {
                const auto& root = mNodes[mRoot];
                if (root.count != 0 && context.node() && detail::closedOverlap(mEntries.mBoxes[a], root.bounds))
                    walkOrdered(root, mEntries.mBoxes[a], mEntries.mMeta[a].filter, context, candidate);
            }
            else
                scan(mEntries.mBoxes[a], mEntries.mMeta[a].filter, context, candidate);
            if (!context.active())
                return; // An unfinished row is discarded, never emitted as a non-prefix.
            if (!movedOnly && count >= 128 && count <= scratch.mKeys.capacity() && count <= scratch.tempKeyCapacity())
                radixRankOrder(scratch, scratch.mRow.data(), static_cast<std::uint32_t>(count));
            else
                sortRanks(scratch.mRow.data(), count);
            for (std::size_t j = 0; j < count && context.active(); ++j)
                reportPair(a, scratch.mRow[j], true, context, visitor);
        }
    }

    template<typename Visitor, typename Predicate>
    void orderedMoved(Workspace<T>& scratch, std::size_t movedCount, Context& context, Visitor& visitor,
        Predicate& predicate) const noexcept
    {
        if (movedCount >= (size() + 1) / 2)
        {
            orderedSelf(scratch, true, context, visitor, predicate);
            return;
        }
        // Every output row starts at a dirty entry or a clean lower-ranked
        // partner of one. Epoch stamps deduplicate source-only marks without
        // clearing/scanning the whole index or changing dirty membership.
        std::size_t sources = movedCount;
        detail::AcceptAll geometryOnly; // Custom predicates filter rows, never drive source pruning.
        for (std::size_t i = 0; i < movedCount && context.active(); ++i)
        {
            const std::uint32_t a = scratch.mMoved[i];
            scratch.mOrder[i] = a;
            if (mEntries.mMeta[a].rank == 0)
                continue;
            auto candidate = [&](std::uint32_t b) noexcept
            {
                if (mEntries.mMeta[b].rank >= mEntries.mMeta[a].rank || scratch.mMembership[b] == scratch.mEpoch)
                    return;
                if (pairQualifies(a, b, true, context, geometryOnly))
                {
                    scratch.mMembership[b] = scratch.mEpoch;
                    scratch.mSelections[b] = 2;
                    scratch.mOrder[sources++] = b;
                }
            };
            scan(mEntries.mBoxes[a], mEntries.mMeta[a].filter, context, candidate);
        }
        if (!context.active())
            return; // Incomplete source discovery cannot establish an ordered prefix.
        sortRanks(scratch.mOrder.data(), sources);
        for (std::size_t i = 0; i < sources && context.active(); ++i)
        {
            const std::uint32_t a = scratch.mOrder[i];
            if ((sources == size() && i + 1 == sources) ||
                mEntries.mMeta[a].rank == std::numeric_limits<std::uint64_t>::max())
                break; // The global maximum rank has no higher partners.
            const bool dirty = scratch.mSelections[a] == 1;
            std::size_t count = 0;
            auto candidate = [&](std::uint32_t b) noexcept
            {
                if (mEntries.mMeta[b].rank <= mEntries.mMeta[a].rank ||
                    (!dirty && (scratch.mMembership[b] != scratch.mEpoch || scratch.mSelections[b] != 1)))
                    return;
                if (pairQualifies(a, b, true, context, predicate))
                    scratch.mRow[count++] = b;
            };
            // For a tiny dirty set, a clean row's partners are cheaper to find
            // directly in S; this also avoids n full scans of an overlapping pile.
            if (!dirty && movedCount <= 64)
                for (std::size_t j = 0; j < movedCount && context.active(); ++j)
                    candidate(scratch.mMoved[j]);
            else
                scan(mEntries.mBoxes[a], mEntries.mMeta[a].filter, context, candidate);
            if (!context.active())
                return; // Both discovery passes are budgeted; discard an unfinished row.
            sortRanks(scratch.mRow.data(), count);
            for (std::size_t j = 0; j < count && context.active(); ++j)
                reportPair(a, scratch.mRow[j], true, context, visitor);
        }
    }

    template<typename Visitor, typename Predicate>
    void itemsVsSubtree(std::uint32_t holder, std::uint32_t subtree, Context& context, Visitor& visitor,
        Predicate& predicate) const noexcept
    {
        if (!context.active() || mNodes[holder].ownCount == 0 || mNodes[subtree].count == 0 ||
            !detail::closedOverlap(mNodes[holder].itemBounds, mNodes[subtree].bounds))
            return;
        if (!detail::compatible(mNodes[holder].itemSummary, mNodes[subtree].summary))
        {
            context.filtered();
            return;
        }
        for (std::uint32_t a = mNodes[holder].head; a != none && context.active(); a = nextItem(mNodes[holder], a))
        {
            auto candidate = [&](std::uint32_t b) noexcept { testPair<true>(a, b, false, context, visitor, predicate); };
            walk(subtree, mEntries.mBoxes[a], mEntries.mMeta[a].filter, context, candidate);
        }
    }

    template<typename Visitor, typename Predicate>
    void crossPairs(std::uint32_t a, std::uint32_t b, Context& context, Visitor& visitor,
        Predicate& predicate) const noexcept
    {
        const Node& left = mNodes[a];
        const Node& right = mNodes[b];
        if (!context.active() || left.count == 0 || right.count == 0 || !context.node())
            return;
        if (!detail::closedOverlap(left.bounds, right.bounds))
            return;
        if (!detail::compatible(left.summary, right.summary))
        {
            context.filtered();
            return;
        }
        for (std::uint32_t i = left.head; i != none && context.active(); i = nextItem(left, i))
            for (std::uint32_t j = right.head; j != none && context.active(); j = nextItem(right, j))
                testPair<true>(i, j, false, context, visitor, predicate);
        if (right.firstChild != none)
            for (std::uint32_t q = 0; q < 4 && context.active(); ++q)
                itemsVsSubtree(a, right.firstChild + q, context, visitor, predicate);
        if (left.firstChild != none)
            for (std::uint32_t q = 0; q < 4 && context.active(); ++q)
                itemsVsSubtree(b, left.firstChild + q, context, visitor, predicate);
        if (left.firstChild != none && right.firstChild != none)
            for (std::uint32_t q = 0; q < 4 && context.active(); ++q)
                for (std::uint32_t r = 0; r < 4 && context.active(); ++r)
                    crossPairs(left.firstChild + q, right.firstChild + r, context, visitor, predicate);
    }

    template<typename Visitor, typename Predicate>
    void selfPairs(std::uint32_t n, Context& context, Visitor& visitor, Predicate& predicate) const noexcept
    {
        const Node& node = mNodes[n];
        if (!context.active() || node.count == 0 || !context.node())
            return;
        if (!detail::compatible(node.summary, node.summary))
        {
            context.filtered();
            return;
        }
        for (std::uint32_t a = node.head; a != none && context.active(); a = nextItem(node, a))
            for (std::uint32_t b = nextItem(node, a); b != none && context.active(); b = nextItem(node, b))
                testPair<true>(a, b, false, context, visitor, predicate);
        if (node.firstChild == none)
            return;
        for (std::uint32_t q = 0; q < 4 && context.active(); ++q)
            itemsVsSubtree(n, node.firstChild + q, context, visitor, predicate);
        for (std::uint32_t q = 0; q < 4 && context.active(); ++q)
            selfPairs(node.firstChild + q, context, visitor, predicate);
        for (std::uint32_t q = 0; q < 4 && context.active(); ++q)
            for (std::uint32_t r = q + 1; r < 4 && context.active(); ++r)
                crossPairs(node.firstChild + q, node.firstChild + r, context, visitor, predicate);
    }

    template<typename Other, typename Predicate>
    bool crossQualifies(const Other& other, std::uint32_t a, std::uint32_t b, Context& context,
        Predicate& predicate) const noexcept
    {
        if (!detail::compatible(mEntries.mMeta[a].filter, other.mEntries.mMeta[b].filter))
        {
            context.filtered();
            return false;
        }
        if (!context.candidate() || !detail::overlaps(mEntries.mBoxes[a], other.mEntries.mBoxes[b], mConfig.boundary))
            return false;
        if constexpr (!std::same_as<Predicate, detail::AcceptAll>)
        {
            context.predicate();
            const View left = view(a);
            const auto right = other.view(b);
            return std::invoke(predicate, left, right);
        }
        return true;
    }

    template<typename Other, typename Visitor>
    void crossReport(const Other& other, std::uint32_t a, std::uint32_t b, Context& context,
        Visitor& visitor) const noexcept
    {
        const View left = view(a);
        const auto right = other.view(b);
        context.report(std::invoke(visitor, left, right));
    }

    template<typename Other, typename Visitor, typename Predicate>
    QueryResult crossEnumeration(const Other& other, Workspace<T>& scratch, bool selections, bool ordered,
        WorkBudget budget, Visitor& visitor, Predicate& predicate) const noexcept
    {
        Context context(budget);
        std::size_t left_count = 0;
        if (ordered)
        {
            for (std::uint32_t a = 0; a < size(); ++a)
                if (!selections || scratch.mSelections[a] == 1)
                    scratch.mOrder[left_count++] = a;
            sortRanks(scratch.mOrder.data(), left_count);
        }
        else
            left_count = size();
        for (std::size_t i = 0; i < left_count && context.active(); ++i)
        {
            const auto a = ordered ? scratch.mOrder[i] : static_cast<std::uint32_t>(i);
            if (selections && scratch.mSelections[a] != 1)
                continue;
            std::size_t count = 0;
            auto candidate = [&](std::uint32_t b) noexcept
            {
                if (selections && scratch.mSelections[b] != 2)
                    return;
                if (!crossQualifies(other, a, b, context, predicate))
                    return;
                if (ordered)
                    scratch.mRow[count++] = b;
                else
                    crossReport(other, a, b, context, visitor);
            };
            other.scan(mEntries.mBoxes[a], mEntries.mMeta[a].filter, context, candidate);
            if (!context.active())
                break;
            if (ordered)
            {
                other.sortRanks(scratch.mRow.data(), count);
                for (std::size_t j = 0; j < count && context.active(); ++j)
                    crossReport(other, a, scratch.mRow[j], context, visitor);
            }
        }
        return context.result;
    }
};

} // namespace QuadTree
