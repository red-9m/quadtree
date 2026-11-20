#pragma once

// QuadTree next major version (work in progress).
//
// Runtime spatial Index and its geometry, resource and handle vocabulary.
// See INDEX.md for the API, reservation units and failure guarantees.
// Do not include together with the legacy quadtree.h (both define Rect).

#include <algorithm>
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
#include <optional>
#include <type_traits>
#include <utility>

namespace QuadTree
{

// ---------------------------------------------------------------------------
// Coordinates (QT-FR-01)
// ---------------------------------------------------------------------------

namespace detail
{
template<typename T>
concept CharacterType = std::same_as<T, char> || std::same_as<T, signed char> || std::same_as<T, unsigned char> ||
    std::same_as<T, wchar_t> || std::same_as<T, char8_t> || std::same_as<T, char16_t> || std::same_as<T, char32_t>;
} // namespace detail

// Signed 32- and 64-bit integers.
template<typename T>
concept IntegerCoordinate = std::same_as<T, std::remove_cv_t<T>> && std::is_integral_v<T> && std::is_signed_v<T> &&
    !detail::CharacterType<T> && (std::numeric_limits<T>::digits == 31 || std::numeric_limits<T>::digits == 63);

// Binary IEC 60559 float and double.
template<typename T>
concept FloatCoordinate = (std::same_as<T, float> || std::same_as<T, double>) &&
    std::numeric_limits<T>::is_iec559 && std::numeric_limits<T>::radix == 2;

template<typename T>
concept Coordinate = IntegerCoordinate<T> || FloatCoordinate<T>;

// ---------------------------------------------------------------------------
// Geometry (QT-FR-02/03/33)
// ---------------------------------------------------------------------------

// Origin/extent input form. Converted to Box with checked endpoint arithmetic.
template<Coordinate T>
struct Rect
{
    T x, y, w, h;
};

// Canonical closed edge form [minX, maxX] x [minY, maxY]; stored as is.
template<Coordinate T>
struct Box
{
    T minX, minY, maxX, maxY;

    friend constexpr bool operator==(const Box&, const Box&) noexcept = default;
};

namespace detail
{
template<FloatCoordinate T>
constexpr bool isFinite(T value) noexcept
{
    return value >= std::numeric_limits<T>::lowest() && value <= std::numeric_limits<T>::max();
}

template<FloatCoordinate T>
constexpr T normalizeZero(T value) noexcept
{
    return value == T(0) ? T(0) : value;
}

// Validates edge-form geometry and normalizes signed zero.
template<Coordinate T>
constexpr bool canonicalize(Box<T>& box) noexcept
{
    if constexpr (FloatCoordinate<T>)
    {
        if (!isFinite(box.minX) || !isFinite(box.minY) || !isFinite(box.maxX) || !isFinite(box.maxY))
            return false;

        box.minX = normalizeZero(box.minX);
        box.minY = normalizeZero(box.minY);
        box.maxX = normalizeZero(box.maxX);
        box.maxY = normalizeZero(box.maxY);
    }

    return box.minX <= box.maxX && box.minY <= box.maxY;
}

// Converts origin/extent geometry to edges. Integers reject unrepresentable far
// edges before adding; floats reject nonfinite input/endpoints and positive
// extents that round back to their origin.
template<Coordinate T>
constexpr bool toBox(const Rect<T>& rect, Box<T>& box) noexcept
{
    if constexpr (FloatCoordinate<T>)
    {
        if (!isFinite(rect.x) || !isFinite(rect.y) || !isFinite(rect.w) || !isFinite(rect.h))
            return false;
        if (rect.w < T(0) || rect.h < T(0))
            return false;

        const T max_x = rect.x + rect.w;
        const T max_y = rect.y + rect.h;
        if (!isFinite(max_x) || !isFinite(max_y))
            return false;
        if ((rect.w > T(0) && !(max_x > rect.x)) || (rect.h > T(0) && !(max_y > rect.y)))
            return false;

        box = Box<T>{normalizeZero(rect.x), normalizeZero(rect.y), normalizeZero(max_x), normalizeZero(max_y)};
        return true;
    }
    else
    {
        if (rect.w < 0 || rect.h < 0)
            return false;
        if (rect.x > std::numeric_limits<T>::max() - rect.w || rect.y > std::numeric_limits<T>::max() - rect.h)
            return false;

        box = Box<T>{rect.x, rect.y, rect.x + rect.w, rect.y + rect.h};
        return true;
    }
}
} // namespace detail

// ---------------------------------------------------------------------------
// Status and visitor control (QT-API-13, QT-FR-23)
// ---------------------------------------------------------------------------

// Failed operations leave indexed state and live handles unchanged unless the
// operation documents otherwise.
enum class [[nodiscard]] Status : std::uint8_t
{
    Ok,                // Completed
    Stopped,           // A visitor returned Stop
    InvalidGeometry,   // Negative extent, reversed edges, nonfinite or unrepresentable endpoint
    InvalidConfiguration,
    OutOfBounds,
    InsufficientWorkspace,
    InsufficientOutput,
    MissingRank,
    DuplicateHandle,
    IncompatiblePolicy,
    InvalidArgument,
    BudgetExhausted,
    DuplicateRank,     // Explicit rank already used by a live entry
    CapacityExhausted, // Hard-capacity mode needs more storage than reserved
    AllocationFailed,  // The resource returned nullptr
    IdentityExhausted, // No unused instance identity or slot generation left
    SizeLimitExceeded, // Entry count or byte size beyond the representable limit
};

enum class Visit : std::uint8_t
{
    Continue,
    Stop,
};

inline constexpr Visit Continue = Visit::Continue;
inline constexpr Visit Stop = Visit::Stop;

// ---------------------------------------------------------------------------
// Memory resource (QT-NFR-04)
// ---------------------------------------------------------------------------

// Caller-owned runtime resource, borrowed by indexes and workspaces. The library
// requests only bytes > 0 and power-of-two alignments.
class Resource
{
public:
    virtual ~Resource() = default;

    // Returns storage of at least bytes aligned to alignment, or nullptr.
    virtual void* allocate(std::size_t bytes, std::size_t alignment) noexcept = 0;

    // Releases storage returned by allocate(). nullptr is a no-op.
    virtual void deallocate(void* ptr) noexcept = 0;

protected:
    Resource() = default;
    Resource(const Resource&) = default;
    Resource& operator=(const Resource&) = default;
};

namespace detail
{
// malloc-based resource with over-alignment. The original pointer is stored
// right below the returned block, so deallocation needs no size or alignment.
class HeapResource final: public Resource
{
public:
    void* allocate(std::size_t bytes, std::size_t alignment) noexcept override
    {
        constexpr std::size_t header = sizeof(void*);
        if (bytes == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0)
            return nullptr;
        if (alignment < alignof(void*))
            alignment = alignof(void*);

        const std::size_t padding = header + alignment - 1;
        if (bytes > std::numeric_limits<std::size_t>::max() - padding)
            return nullptr;

        void* raw = std::malloc(bytes + padding);
        if (!raw)
            return nullptr;

        void* aligned = static_cast<unsigned char*>(raw) + header;
        std::size_t space = bytes + alignment - 1;
        aligned = std::align(alignment, bytes, aligned, space);
        if (!aligned)
        {
            std::free(raw);
            return nullptr;
        }

        std::memcpy(static_cast<unsigned char*>(aligned) - header, &raw, header);
        return aligned;
    }

    void deallocate(void* ptr) noexcept override
    {
        if (!ptr)
            return;

        void* raw = nullptr;
        std::memcpy(&raw, static_cast<unsigned char*>(ptr) - sizeof(void*), sizeof(void*));
        std::free(raw);
    }
};
} // namespace detail

// Library-provided nonthrowing heap resource with process lifetime (never destroyed).
inline Resource* defaultResource() noexcept
{
    alignas(detail::HeapResource) static unsigned char storage[sizeof(detail::HeapResource)];
    static Resource* const resource = ::new (static_cast<void*>(storage)) detail::HeapResource();
    return resource;
}

// ---------------------------------------------------------------------------
// Filter (QT-FR-27, QT-API-11)
// ---------------------------------------------------------------------------

struct Filter
{
    std::uint64_t category = ~std::uint64_t(0);
    std::uint64_t mask = ~std::uint64_t(0);

    friend constexpr bool operator==(const Filter&, const Filter&) noexcept = default;
};

// ---------------------------------------------------------------------------
// Diagnostics policy (QT-FR-07, QT-NFR-09)
// ---------------------------------------------------------------------------

// Handle-taking operations require valid handles. Checked types validate them
// and terminate on misuse; unchecked types compile the validation out. The
// selection never depends on _DEBUG or NDEBUG.
struct Unchecked
{
    static constexpr bool checkHandles = false;
};

struct Checked
{
    static constexpr bool checkHandles = true;
};

// Optional policy members, used by tests:
//   static std::uint64_t acquireIdentity() noexcept;  // unique nonzero id, 0 when exhausted
//   static constexpr std::uint32_t generationLimit;   // last usable generation of a slot
//   static constexpr std::uint32_t slotLimit;         // maximum number of slots
//   static constexpr std::uint32_t retiredLimit;      // retired slots that block new slots (> 0)
//   static constexpr std::uint32_t rankProbeLimit;    // rank hash displacement bound (> 0)
//   static constexpr std::uint32_t rankEpochLimit;    // rank hash epochs before a full reset (> 1)
// Index work probes (test-only; omitted entirely when the policy has no hook):
//   static void onSplitPreparation(std::size_t entries) noexcept;
//   static void onItemRefit(std::size_t entries) noexcept;
template<typename P>
concept DiagnosticsPolicy = requires { { P::checkHandles } -> std::convertible_to<bool>; };

// ---------------------------------------------------------------------------
// Handles (QT-FR-07)
// ---------------------------------------------------------------------------
//
// Handle identity:
// - Size: 16 bytes = 64-bit owner identity + 32-bit slot + 32-bit generation.
// - Owner identity comes from one module-wide atomic counter (an inline
//   variable shared by every translation unit of the executable or shared
//   library). Storage draws a fresh identity lazily, on the first insertion
//   after creation, clear(), or being moved from. Identities are never reused,
//   so handles of destroyed, cleared or moved-from state never match again,
//   also at a reused address. Counter exhaustion fails that insertion with
//   Status::IdentityExhausted.
// - A slot's generation increments on removal. A slot removed at the generation
//   limit is retired (kept, never reused) until the next identity reset.
// - New slots start at a generation of at most half the limit, so a slot retires
//   after more than 2^31 uses. Once retiredLimit (default 65536) slots are
//   retired, or only retired slots remain at the slot limit, insertions that need
//   a new slot fail with Status::IdentityExhausted. The slot table therefore
//   never exceeds the live high-water mark plus retiredLimit, independent of
//   operation history; exhaustion takes at least retiredLimit * 2^31 removals.
// - shrink() truncates the slot table only past the highest live, retired, or
//   above-half-limit slot; recreated slots start above every truncated
//   generation and still at most half the limit.
// - Value-initialized handles are never valid. EntryStore uses its full
//   specialization as the handle tag, so different coordinate, payload,
//   policy or caller tag types give mutually unconvertible handle types.

namespace detail
{
struct HandleAccess;
} // namespace detail

template<typename Tag>
class Handle
{
public:
    constexpr Handle() noexcept = default;

    friend constexpr bool operator==(const Handle&, const Handle&) noexcept = default;

private:
    friend struct detail::HandleAccess;

    constexpr Handle(std::uint64_t owner, std::uint32_t slot, std::uint32_t generation) noexcept:
        mOwner(owner), mSlot(slot), mGeneration(generation)
    {
    }

    std::uint64_t mOwner = 0;
    std::uint32_t mSlot = 0;
    std::uint32_t mGeneration = 0;
};

// Const entry view, valid for the callback's duration only (section 6).
template<Coordinate T, typename Payload, typename Tag>
struct EntryView
{
    Handle<Tag> handle;
    const Box<T>& rect;
    const Payload& payload;
    Filter filter;
    std::optional<std::uint64_t> rank;
};

template<typename Tag>
struct [[nodiscard]] InsertResult
{
    Status status;
    Handle<Tag> handle; // Valid only when status == Status::Ok

    constexpr bool ok() const noexcept { return status == Status::Ok; }
};

// Storage reservation units. reserve() fails with Status::IdentityExhausted
// when retired slots leave too little slot space for reservation.entries;
// slots retired after reserve() consume reserved slot capacity.
struct Reservation
{
    std::size_t entries = 0;
    std::size_t rankedEntries = 0;
    std::size_t nodes = 0;
    std::size_t references = 0; // Index reserves at least entries references (one per entry)
    std::size_t descriptors = 0; // Incremental split/growth scratch; build uses Workspace instead
};

// Payloads are owned values; relocation and destruction must not throw (QT-FR-08).
template<typename P>
concept StorablePayload = std::is_object_v<P> && !std::is_array_v<P> && std::same_as<P, std::remove_cv_t<P>> &&
    std::is_nothrow_destructible_v<P> && std::is_nothrow_move_constructible_v<P>;

template<StorablePayload Payload, Coordinate T, DiagnosticsPolicy Policy = Unchecked, bool Instrumented = true>
class Index;

namespace detail
{
inline constexpr std::uint32_t kNone = std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint32_t kRetired = kNone - 1;
inline constexpr std::uint32_t kMaxSlots = kNone - 2; // Slot and dense indices stay below kRetired

// Shared by all translation units of the module (inline variable).
inline std::atomic<std::uint64_t> moduleIdentityCounter{0};

// Returns a unique nonzero identity, or 0 when the counter is exhausted.
#if __has_cpp_attribute(gnu::always_inline)
[[gnu::always_inline]]
#endif
inline std::uint64_t acquireModuleIdentityInline() noexcept
{
    std::uint64_t current = moduleIdentityCounter.load(std::memory_order_relaxed);
    do
    {
        if (current == std::numeric_limits<std::uint64_t>::max())
            return 0;
    } while (!moduleIdentityCounter.compare_exchange_weak(current, current + 1, std::memory_order_relaxed));

    return current + 1;
}

inline std::uint64_t acquireModuleIdentity() noexcept
{
    return acquireModuleIdentityInline();
}

[[noreturn]] inline void invalidHandle(const char* operation) noexcept
{
    std::fputs("QuadTree: invalid handle passed to ", stderr);
    std::fputs(operation, stderr);
    std::fputs("()\n", stderr);
    std::fflush(stderr);
    std::abort();
}

// splitmix64 finalizer: fixed, deterministic, bijective.
constexpr std::uint64_t mix64(std::uint64_t value) noexcept
{
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9u;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebu;
    value ^= value >> 31;
    return value;
}

struct HandleAccess
{
    template<typename Tag>
    static constexpr Handle<Tag> make(std::uint64_t owner, std::uint32_t slot, std::uint32_t generation) noexcept
    {
        return Handle<Tag>(owner, slot, generation);
    }

    template<typename Tag>
    static constexpr std::uint64_t owner(const Handle<Tag>& handle) noexcept { return handle.mOwner; }

    template<typename Tag>
    static constexpr std::uint32_t slot(const Handle<Tag>& handle) noexcept { return handle.mSlot; }

    template<typename Tag>
    static constexpr std::uint32_t generation(const Handle<Tag>& handle) noexcept { return handle.mGeneration; }

    template<typename Tag>
    static constexpr std::size_t hash(const Handle<Tag>& handle) noexcept
    {
        const std::uint64_t local = (std::uint64_t(handle.mSlot) << 32) | handle.mGeneration;
        return static_cast<std::size_t>(mix64(handle.mOwner ^ mix64(local)));
    }
};

template<DiagnosticsPolicy Policy>
struct PolicyTraits
{
    static constexpr bool checkHandles = static_cast<bool>(Policy::checkHandles);

    static constexpr std::uint32_t generationLimit = []
    {
        if constexpr (requires { Policy::generationLimit; })
            return std::uint32_t{Policy::generationLimit};
        else
            return std::numeric_limits<std::uint32_t>::max();
    }();

    static constexpr std::uint32_t slotLimit = []
    {
        if constexpr (requires { Policy::slotLimit; })
            return std::uint32_t{Policy::slotLimit};
        else
            return kMaxSlots;
    }();

    static constexpr std::uint32_t retiredLimit = []
    {
        if constexpr (requires { Policy::retiredLimit; })
            return std::uint32_t{Policy::retiredLimit};
        else
            return std::uint32_t{65536};
    }();

    // Highest generation a new slot may start at.
    static constexpr std::uint32_t freshLimit = generationLimit / 2;

    static constexpr std::uint32_t rankProbeLimit = []
    {
        if constexpr (requires { Policy::rankProbeLimit; })
            return std::uint32_t{Policy::rankProbeLimit};
        else
            return std::uint32_t{128};
    }();

    static constexpr std::uint32_t rankEpochLimit = []
    {
        if constexpr (requires { Policy::rankEpochLimit; })
            return std::uint32_t{Policy::rankEpochLimit};
        else
            return std::numeric_limits<std::uint32_t>::max();
    }();

    static_assert(slotLimit > 0 && slotLimit <= kMaxSlots, "QuadTree: policy slotLimit out of range");
    static_assert(retiredLimit > 0, "QuadTree: policy retiredLimit must be positive");
    static_assert(rankProbeLimit > 0, "QuadTree: policy rankProbeLimit must be positive");
    static_assert(rankEpochLimit > 1, "QuadTree: policy rankEpochLimit must exceed 1");

    static std::uint64_t acquireIdentity() noexcept
    {
        if constexpr (requires { Policy::acquireIdentity(); })
        {
            static_assert(noexcept(Policy::acquireIdentity()) &&
                    std::same_as<decltype(Policy::acquireIdentity()), std::uint64_t>,
                "QuadTree: Policy::acquireIdentity() must be noexcept and return std::uint64_t");
            return Policy::acquireIdentity();
        }
        else
            return acquireModuleIdentity();
    }
};

// Entry storage: owns payloads and stored geometry, issues handles, and keeps
// filter and rank metadata. Live entries are dense (swap-remove), so iteration
// is O(n) in a deterministic order. Handles address stable slots.
//
// All operations are noexcept. Failed operations leave entries and handles
// unchanged (strong guarantee). Capacity grows automatically unless
// hardCapacity is set; then only reserve() grows it.
//
// Bytes per entry of capacity: sizeof(Box<T>) + sizeof(Payload) + sizeof(Meta)
// + sizeof(Slot); the rank set adds sizeof(RankCell) per cell at load factor
// 1/4..1/2. retainedBytes() reports the total (32 + 8 and 16 on LP64).
//
// Rank set (duplicate-rank detection): open addressing with linear probing on
// a fixed mix64 hash, every rank within rankProbeLimit (128) buckets of its
// home. Hash lookup/insertion probes cost O(rankProbeLimit); backward-shift
// erase costs O(n) worst-case and O(rankProbeLimit) amortized: shifts reduce
// total displacement, which each hash insertion increases by at most
// rankProbeLimit. Ranks are client data and mix64 is public, so ranks can be
// chosen to share one home bucket. An insertion beyond the bound switches
// the set to a splay tree
// built in the same cells from live metadata (O(n log n) once; amortized
// O(log n) per operation for any sequence, adversarial ones included). The
// tree stays until the next rehash. clear() is O(1): occupied buckets carry
// the current epoch, and only an epoch wrap (every rankEpochLimit clears)
// resets all buckets. A rehash initializes the new cells (O(new capacity),
// paid with the allocation) and reinserts ranks from live metadata.
template<Coordinate T, StorablePayload Payload, DiagnosticsPolicy Policy, typename Tag>
class EntryStore
{
    template<StorablePayload, Coordinate, DiagnosticsPolicy, bool>
    friend class ::QuadTree::Index;

    using Traits = PolicyTraits<Policy>;
    using HandleTag = EntryStore;

public:
    using HandleType = Handle<HandleTag>;
    using View = EntryView<T, Payload, HandleTag>;
    using InsertResultType = InsertResult<HandleTag>;

    explicit EntryStore(Resource* resource = nullptr, bool hardCapacity = false) noexcept:
        mResource(resource ? resource : defaultResource()), mHardCapacity(hardCapacity)
    {
    }

    EntryStore(const EntryStore&) = delete;
    EntryStore& operator=(const EntryStore&) = delete;

    // O(1); the source keeps its resource and becomes empty and reusable.
    EntryStore(EntryStore&& other) noexcept: mResource(other.mResource), mHardCapacity(other.mHardCapacity)
    {
        adopt(other);
    }

    // Releases this store's entries and storage through its old resource, then
    // adopts the source state and resource. Former handles of this store become
    // invalid; source handles follow the state.
    EntryStore& operator=(EntryStore&& other) noexcept
    {
        if (this != &other)
        {
            std::destroy_n(mPayloads, mSize);
            releaseStorage();
            mResource = other.mResource;
            mHardCapacity = other.mHardCapacity;
            adopt(other);
        }
        return *this;
    }

    ~EntryStore()
    {
        std::destroy_n(mPayloads, mSize);
        releaseStorage();
    }

    Resource* resource() const noexcept { return mResource; }
    std::size_t size() const noexcept { return mSize; }
    bool empty() const noexcept { return mSize == 0; }
    std::size_t capacity() const noexcept { return mCapacity; }

    std::size_t retainedBytes() const noexcept
    {
        return std::size_t(mCapacity) * (sizeof(Box<T>) + sizeof(Payload) + sizeof(Meta)) +
            std::size_t(mSlotCapacity) * sizeof(Slot) + mRankCapacity * sizeof(RankCell);
    }

    // Rank set work counters (tests and benchmarks).
    struct RankStats
    {
        std::uint64_t probes = 0;      // Hash probes and splay steps
        std::uint64_t cellResets = 0;  // Cells reset to empty buckets
        std::uint64_t conversions = 0; // Switches from hash to splay tree
        bool tree = false;             // Splay-tree mode active
    };

    RankStats rankStats() const noexcept
    {
        RankStats stats = mRankStats;
        stats.tree = mRankTree;
        return stats;
    }

    // Exact in every build: false for stale, foreign and value-initialized handles.
    bool valid(HandleType handle) const noexcept
    {
        const std::uint64_t owner = HandleAccess::owner(handle);
        const std::uint32_t slot = HandleAccess::slot(handle);
        if (owner == 0 || owner != mOwner || slot >= mSlotsUsed)
            return false;

        // Free slots carry a generation never issued; retired slots fail the index test.
        const Slot& entry_slot = mSlots[slot];
        return entry_slot.generation == HandleAccess::generation(handle) && entry_slot.index < mSize;
    }

    InsertResultType insert(const Box<T>& box, const Payload& payload, Filter filter = {},
        std::optional<std::uint64_t> rank = {}) noexcept requires std::is_nothrow_copy_constructible_v<Payload>
    {
        return insertBox(box, payload, filter, rank);
    }

    InsertResultType insert(const Box<T>& box, Payload&& payload, Filter filter = {},
        std::optional<std::uint64_t> rank = {}) noexcept
    {
        return insertBox(box, std::move(payload), filter, rank);
    }

    InsertResultType insert(const Rect<T>& rect, const Payload& payload, Filter filter = {},
        std::optional<std::uint64_t> rank = {}) noexcept requires std::is_nothrow_copy_constructible_v<Payload>
    {
        Box<T> box{};
        if (!toBox(rect, box))
            return {Status::InvalidGeometry, {}};
        return insertBox(box, payload, filter, rank);
    }

    InsertResultType insert(const Rect<T>& rect, Payload&& payload, Filter filter = {},
        std::optional<std::uint64_t> rank = {}) noexcept
    {
        Box<T> box{};
        if (!toBox(rect, box))
            return {Status::InvalidGeometry, {}};
        return insertBox(box, std::move(payload), filter, rank);
    }

    // Replaces stored geometry; handle, payload, filter and rank are preserved.
    Status update(HandleType handle, Box<T> box) noexcept
    {
        requireValid(handle, "update");
        if (!canonicalize(box))
            return Status::InvalidGeometry;

        mBoxes[denseOf(handle)] = box;
        return Status::Ok;
    }

    Status update(HandleType handle, const Rect<T>& rect) noexcept
    {
        requireValid(handle, "update");
        Box<T> box{};
        if (!toBox(rect, box))
            return Status::InvalidGeometry;

        mBoxes[denseOf(handle)] = box;
        return Status::Ok;
    }

    // Never allocates. Invalidates only this handle.
    Status remove(HandleType handle) noexcept
    {
        requireValid(handle, "remove");
        const std::uint32_t slot = HandleAccess::slot(handle);
        const std::uint32_t dense = mSlots[slot].index;
        if (mMeta[dense].hasRank)
            eraseRank(mMeta[dense].rank);

        std::destroy_at(mPayloads + dense);
        const std::uint32_t last = mSize - 1;
        if (dense != last)
        {
            std::construct_at(mPayloads + dense, std::move(mPayloads[last]));
            std::destroy_at(mPayloads + last);
            mBoxes[dense] = mBoxes[last];
            mMeta[dense] = mMeta[last];
            mSlots[mMeta[dense].slot].index = dense;
        }

        mSize = last;
        releaseSlot(slot);
        return Status::Ok;
    }

    Status setFilter(HandleType handle, Filter filter) noexcept
    {
        requireValid(handle, "setFilter");
        mMeta[denseOf(handle)].filter = filter;
        return Status::Ok;
    }

    // Lookup references stay valid until the next non-const operation.
    const Box<T>& rect(HandleType handle) const noexcept
    {
        requireValid(handle, "rect");
        return mBoxes[denseOf(handle)];
    }

    const Payload& payload(HandleType handle) const noexcept
    {
        requireValid(handle, "payload");
        return mPayloads[denseOf(handle)];
    }

    Payload& payload(HandleType handle) noexcept
    {
        requireValid(handle, "payload");
        return mPayloads[denseOf(handle)];
    }

    Filter filter(HandleType handle) const noexcept
    {
        requireValid(handle, "filter");
        return mMeta[denseOf(handle)].filter;
    }

    std::optional<std::uint64_t> rank(HandleType handle) const noexcept
    {
        requireValid(handle, "rank");
        return rankAt(denseOf(handle));
    }

    // Visits every live entry once in storage order, which is deterministic
    // for identical operation sequences. Returns Status::Stopped on Stop.
    template<typename Visitor>
        requires std::is_nothrow_invocable_r_v<Visit, Visitor&, const View&>
    Status forEachEntry(Visitor&& visitor) const noexcept
    {
        for (std::uint32_t i = 0; i < mSize; ++i)
        {
            const std::uint32_t slot = mMeta[i].slot;
            const View view{HandleAccess::make<HandleTag>(mOwner, slot, mSlots[slot].generation), mBoxes[i], mPayloads[i],
                mMeta[i].filter, rankAt(i)};
            if (visitor(view) == Visit::Stop)
                return Status::Stopped;
        }
        return Status::Ok;
    }

    // Destroys all entries and invalidates every handle; keeps capacity.
    // Costs O(size); the rank set is emptied in O(1).
    void clear() noexcept
    {
        std::destroy_n(mPayloads, mSize);
        resetSlots();
        mSize = 0;
        mSlotsUsed = 0;
        mOwner = 0;
    }

    // Grows capacity so that reservation.entries live entries, of which
    // reservation.rankedEntries have ranks, fit without allocation. Never
    // shrinks. Allowed in hard-capacity mode.
    Status reserve(const Reservation& reservation) noexcept
    {
        if (reservation.entries > Traits::slotLimit || reservation.rankedEntries > Traits::slotLimit)
            return Status::SizeLimitExceeded;

        // Live and free slots are reusable; more entries need new slots.
        const auto entries = static_cast<std::uint32_t>(reservation.entries);
        const std::uint64_t slots = std::uint64_t(entries) + mRetired;
        if (entries > mSlotsUsed - mRetired && (slots > Traits::slotLimit || mRetired >= Traits::retiredLimit))
            return Status::IdentityExhausted;

        const std::uint64_t rank_capacity = rankCapacityFor(reservation.rankedEntries);

        Pending pending;
        const Status status = allocate(pending, entries > mCapacity ? entries : 0,
            slots > mSlotCapacity ? slots : 0, rank_capacity > mRankCapacity ? rank_capacity : 0);
        if (status != Status::Ok)
            return status;

        commit(pending, mSlotsUsed);
        return Status::Ok;
    }

    // Releases unused capacity; live handles stay valid. Strong guarantee on
    // allocation failure.
    Status shrink() noexcept
    {
        if (mSize == 0)
        {
            clear();
            releaseStorage();
            return Status::Ok;
        }

        // Keep slots up to the highest live, retired, or above-freshLimit one,
        // so truncation never raises the start generation above freshLimit.
        std::uint32_t slot_count = 0;
        for (std::uint32_t i = 0; i < mSize; ++i)
            slot_count = std::max(slot_count, mMeta[i].slot + 1);
        for (std::uint32_t i = slot_count; i < mSlotsUsed; ++i)
            if (mSlots[i].index == kRetired || mSlots[i].generation > Traits::freshLimit)
                slot_count = i + 1;

        const std::uint64_t rank_capacity = rankCapacityFor(mRankCount);
        Pending pending;
        const Status status = allocate(pending, mSize < mCapacity ? mSize : 0,
            slot_count < mSlotCapacity ? slot_count : 0, rank_capacity < mRankCapacity ? rank_capacity : 0);
        if (status != Status::Ok)
            return status;

        if (slot_count < mSlotsUsed)
            truncateSlots(slot_count);
        commit(pending, mSlotsUsed);
        if (mRankCount == 0 && mRanks)
        {
            mResource->deallocate(mRanks);
            mRanks = nullptr;
            mRankCapacity = 0;
            mRankTree = false;
        }
        return Status::Ok;
    }

private:
    struct Slot
    {
        std::uint32_t generation;
        std::uint32_t index; // Dense index if live; next free slot, kNone or kRetired otherwise
    };

    struct Meta
    {
        Filter filter;
        std::uint64_t rank;
        std::uint32_t slot;
        bool hasRank;
    };

    struct RankBucket
    {
        std::uint64_t rank;
        std::uint32_t epoch; // Occupied when equal to mRankEpoch; 0 = never
        std::uint32_t unused;
    };

    struct RankNode
    {
        std::uint64_t rank;
        std::uint32_t left; // Next free node when on the free list
        std::uint32_t right;
    };

    // Hash bucket in hash mode, splay-tree node in tree mode; the active
    // member changes only by assignment of a whole member.
    union RankCell
    {
        RankBucket bucket;
        RankNode node;
    };

    // Buffers allocated before a commit; null members are not replaced.
    struct Pending
    {
        Box<T>* boxes = nullptr;
        Payload* payloads = nullptr;
        Meta* meta = nullptr;
        std::uint32_t capacity = 0;
        Slot* slots = nullptr;
        std::uint32_t slotCapacity = 0;
        RankCell* ranks = nullptr;
        std::size_t rankCapacity = 0;
    };

    Resource* mResource;
    Box<T>* mBoxes = nullptr;
    Payload* mPayloads = nullptr;
    Meta* mMeta = nullptr;
    Slot* mSlots = nullptr;
    RankCell* mRanks = nullptr;
    std::size_t mRankCapacity = 0; // Zero or a power of two; load factor <= 1/2
    std::size_t mRankCount = 0;
    std::uint32_t mRankEpoch = 1;  // Hash mode: epoch of occupied buckets
    bool mRankTree = false;        // Splay-tree mode
    std::uint32_t mRankRoot = kNone;
    std::uint32_t mRankFree = kNone;
    std::uint32_t mRankHighWater = 0; // Tree cells used since the switch or clear
    RankStats mRankStats;
    std::uint64_t mOwner = 0;      // Zero until the first insertion of an identity epoch
    std::uint32_t mSize = 0;
    std::uint32_t mCapacity = 0;
    std::uint32_t mSlotCapacity = 0;
    std::uint32_t mSlotsUsed = 0;
    std::uint32_t mFreeHead = kNone;
    std::uint32_t mRetired = 0;
    std::uint32_t mFreshGeneration = 0; // Generation of newly created slots
    bool mHardCapacity = false;

    void requireValid([[maybe_unused]] HandleType handle, [[maybe_unused]] const char* operation) const noexcept
    {
        if constexpr (Traits::checkHandles)
        {
            if (!valid(handle))
                invalidHandle(operation);
        }
    }

    std::uint32_t denseOf(HandleType handle) const noexcept { return mSlots[HandleAccess::slot(handle)].index; }

    std::optional<std::uint64_t> rankAt(std::uint32_t dense) const noexcept
    {
        if (!mMeta[dense].hasRank)
            return std::nullopt;
        return mMeta[dense].rank;
    }

    template<typename P>
    InsertResultType insertBox(Box<T> box, P&& payload, Filter filter, std::optional<std::uint64_t> rank) noexcept
    {
        if (!canonicalize(box))
            return {Status::InvalidGeometry, {}};
        if (rank && containsRank(*rank))
            return {Status::DuplicateRank, {}};
        if (mSize >= Traits::slotLimit)
            return {Status::SizeLimitExceeded, {}};

        const bool new_slot = mFreeHead == kNone;
        if (new_slot && (mSlotsUsed >= Traits::slotLimit || mRetired >= Traits::retiredLimit))
            return {Status::IdentityExhausted, {}}; // Remaining slot space is retired

        const bool grow_dense = mSize == mCapacity;
        const bool grow_slots = new_slot && mSlotsUsed == mSlotCapacity;
        const bool grow_ranks = rank && (mRankCount + 1) * 2 > mRankCapacity;
        if (mHardCapacity && (grow_dense || grow_slots || grow_ranks))
            return {Status::CapacityExhausted, {}};

        if (mOwner == 0)
        {
            const std::uint64_t owner = Traits::acquireIdentity();
            if (owner == 0)
                return {Status::IdentityExhausted, {}};
            mOwner = owner;
        }

        Pending pending;
        const Status status = allocate(pending, grow_dense ? grownCapacity(mCapacity, mSize + 1) : 0,
            grow_slots ? grownCapacity(mSlotCapacity, mSlotsUsed + 1) : 0,
            grow_ranks ? rankCapacityFor(mRankCount + 1) : 0);
        if (status != Status::Ok)
            return {status, {}};

        // Commit; nothing below fails. Construct the payload before relocating
        // old entries: the argument may refer to a stored payload.
        std::construct_at((pending.payloads ? pending.payloads : mPayloads) + mSize, std::forward<P>(payload));
        commit(pending, mSlotsUsed);

        const std::uint32_t slot = takeSlot();
        const std::uint32_t dense = mSize;
        std::construct_at(mBoxes + dense, box);
        std::construct_at(mMeta + dense, Meta{filter, rank.value_or(0), slot, rank.has_value()});
        mSlots[slot].index = dense;
        ++mSize;
        if (rank)
        {
            insertRank(*rank);
            ++mRankCount;
        }

        return {Status::Ok, HandleAccess::make<HandleTag>(mOwner, slot, mSlots[slot].generation)};
    }

    std::uint32_t takeSlot() noexcept
    {
        if (mFreeHead != kNone)
        {
            const std::uint32_t slot = mFreeHead;
            mFreeHead = mSlots[slot].index;
            return slot;
        }

        const std::uint32_t slot = mSlotsUsed++;
        std::construct_at(mSlots + slot, Slot{mFreshGeneration, kNone});
        return slot;
    }

    void releaseSlot(std::uint32_t slot) noexcept
    {
        Slot& entry_slot = mSlots[slot];
        if (entry_slot.generation >= Traits::generationLimit)
        {
            entry_slot.index = kRetired;
            ++mRetired;
            return;
        }

        ++entry_slot.generation;
        entry_slot.index = mFreeHead;
        mFreeHead = slot;
    }

    // Drops free slots at and above count, all at or below freshLimit.
    // Recreated slots start above every dropped generation, so stale handles to
    // them stay invalid.
    void truncateSlots(std::uint32_t count) noexcept
    {
        for (std::uint32_t i = count; i < mSlotsUsed; ++i)
            mFreshGeneration = std::max(mFreshGeneration, mSlots[i].generation);

        std::uint32_t* link = &mFreeHead;
        for (std::uint32_t slot = mFreeHead; slot != kNone; slot = mSlots[slot].index)
        {
            if (slot < count)
            {
                *link = slot;
                link = &mSlots[slot].index;
            }
        }
        *link = kNone;
        mSlotsUsed = count;
    }

    // --- Rank set: bounded linear probing, splay-tree fallback (see above) ---

    static std::uint64_t rankCapacityFor(std::uint64_t count) noexcept
    {
        if (count == 0)
            return 0;

        std::uint64_t capacity = 8;
        while (capacity < count * 2)
            capacity *= 2;
        return capacity;
    }

    std::size_t homeOf(std::uint64_t rank) const noexcept
    {
        return static_cast<std::size_t>(mix64(rank)) & (mRankCapacity - 1);
    }

    bool occupied(std::size_t i) const noexcept { return mRanks[i].bucket.epoch == mRankEpoch; }

    // Fresh cells: empty buckets, hash mode. O(capacity), paid with the allocation.
    void resetRankCells() noexcept
    {
        for (std::size_t i = 0; i < mRankCapacity; ++i)
            std::construct_at(mRanks + i, RankCell{RankBucket{0, 0, 0}});
        mRankStats.cellResets += mRankCapacity;
        mRankEpoch = 1;
        mRankTree = false;
    }

    void resetSlots() noexcept
    {
        if (mRankCapacity != 0)
            clearRanks();
        mFreeHead = kNone;
        mRetired = 0;
        mFreshGeneration = 0;
    }

    // Empties the set in O(1); a wrapped epoch resets every bucket.
    void clearRanks() noexcept
    {
        mRankCount = 0;
        if (mRankTree)
        {
            mRankRoot = kNone;
            mRankFree = kNone;
            mRankHighWater = 0;
            return;
        }
        if (++mRankEpoch >= Traits::rankEpochLimit)
        {
            for (std::size_t i = 0; i < mRankCapacity; ++i)
                mRanks[i].bucket = RankBucket{0, 0, 0};
            mRankStats.cellResets += mRankCapacity;
            mRankEpoch = 1;
        }
    }

    bool containsRank(std::uint64_t rank) noexcept
    {
        if (mRankCount == 0)
            return false;
        if (mRankTree)
        {
            mRankRoot = splay(mRankRoot, rank);
            return mRanks[mRankRoot].node.rank == rank;
        }
        // Every stored rank lies within rankProbeLimit buckets of its home.
        const std::size_t mask = mRankCapacity - 1;
        std::size_t i = homeOf(rank);
        for (std::uint32_t distance = 0; distance <= Traits::rankProbeLimit && occupied(i); ++distance)
        {
            ++mRankStats.probes;
            if (mRanks[i].bucket.rank == rank)
                return true;
            i = (i + 1) & mask;
        }
        return false;
    }

    // Hash insertion; false (nothing changed) if it would exceed the bound.
    bool hashInsert(std::uint64_t rank) noexcept
    {
        const std::size_t mask = mRankCapacity - 1;
        std::size_t i = homeOf(rank);
        for (std::uint32_t distance = 0; distance <= Traits::rankProbeLimit; ++distance)
        {
            ++mRankStats.probes;
            if (!occupied(i))
            {
                mRanks[i].bucket = RankBucket{rank, mRankEpoch, 0};
                return true;
            }
            i = (i + 1) & mask;
        }
        return false;
    }

    // Adds a rank whose metadata is already live and which is not present.
    void insertRank(std::uint64_t rank) noexcept
    {
        if (mRankTree)
            treeInsert(rank);
        else if (!hashInsert(rank))
            buildRankTree();
    }

    void eraseRank(std::uint64_t rank) noexcept
    {
        --mRankCount;
        if (mRankTree)
        {
            treeErase(rank);
            return;
        }
        const std::size_t mask = mRankCapacity - 1;
        std::size_t hole = homeOf(rank);
        while (mRanks[hole].bucket.rank != rank || !occupied(hole))
        {
            ++mRankStats.probes;
            hole = (hole + 1) & mask;
        }
        // Backward shift; entries more than rankProbeLimit past the moving
        // hole have their home after it, so the scan stops there. Each shift
        // moves the hole and can extend the scan through the whole cluster.
        std::uint32_t distance = 1;
        for (std::size_t next = (hole + 1) & mask; occupied(next) && distance <= Traits::rankProbeLimit;
             next = (next + 1) & mask, ++distance)
        {
            ++mRankStats.probes;
            const std::size_t home = homeOf(mRanks[next].bucket.rank);
            const bool stays = hole <= next ? (hole < home && home <= next) : (hole < home || home <= next);
            if (!stays)
            {
                mRanks[hole].bucket = mRanks[next].bucket;
                hole = next;
                distance = 0;
            }
        }
        mRanks[hole].bucket = RankBucket{0, 0, 0};
    }

    // Reinserts live ranks after a rehash into fresh cells.
    void rebuildRanks() noexcept
    {
        for (std::uint32_t i = 0; i < mSize; ++i)
            if (mMeta[i].hasRank && !hashInsert(mMeta[i].rank))
            {
                buildRankTree();
                return;
            }
    }

    // Splay tree of all live ranks in the current cells (from metadata).
    void buildRankTree() noexcept
    {
        ++mRankStats.conversions;
        mRankTree = true;
        mRankRoot = kNone;
        mRankFree = kNone;
        mRankHighWater = 0;
        for (std::uint32_t i = 0; i < mSize; ++i)
            if (mMeta[i].hasRank)
                treeInsert(mMeta[i].rank);
    }

    RankNode& node(std::uint32_t i) noexcept { return mRanks[i].node; }

    // Top-down splay: brings rank, or its neighbour, to the root.
    std::uint32_t splay(std::uint32_t t, std::uint64_t rank) noexcept
    {
        std::uint32_t left_root = kNone;
        std::uint32_t right_root = kNone;
        std::uint32_t* left_hook = &left_root;   // Right child slot of the left tree's maximum
        std::uint32_t* right_hook = &right_root; // Left child slot of the right tree's minimum
        for (;;)
        {
            ++mRankStats.probes;
            if (rank < node(t).rank)
            {
                std::uint32_t child = node(t).left;
                if (child == kNone)
                    break;
                if (rank < node(child).rank)
                {
                    node(t).left = node(child).right;
                    node(child).right = t;
                    t = child;
                    if (node(t).left == kNone)
                        break;
                }
                *right_hook = t;
                right_hook = &node(t).left;
                t = node(t).left;
            }
            else if (rank > node(t).rank)
            {
                std::uint32_t child = node(t).right;
                if (child == kNone)
                    break;
                if (rank > node(child).rank)
                {
                    node(t).right = node(child).left;
                    node(child).left = t;
                    t = child;
                    if (node(t).right == kNone)
                        break;
                }
                *left_hook = t;
                left_hook = &node(t).right;
                t = node(t).right;
            }
            else
                break;
        }
        *left_hook = node(t).left;
        *right_hook = node(t).right;
        node(t).left = left_root;
        node(t).right = right_root;
        return t;
    }

    // Inserts a rank that is not present. Nodes never exceed the cells: the
    // set holds at most capacity / 2 ranks.
    void treeInsert(std::uint64_t rank) noexcept
    {
        std::uint32_t x = mRankFree;
        if (x != kNone)
            mRankFree = node(x).left;
        else
            x = mRankHighWater++;
        mRanks[x].node = RankNode{rank, kNone, kNone};
        if (mRankRoot == kNone)
        {
            mRankRoot = x;
            return;
        }
        const std::uint32_t root = splay(mRankRoot, rank);
        if (rank < node(root).rank)
        {
            node(x).left = node(root).left;
            node(x).right = root;
            node(root).left = kNone;
        }
        else
        {
            node(x).right = node(root).right;
            node(x).left = root;
            node(root).right = kNone;
        }
        mRankRoot = x;
    }

    // Removes a present rank.
    void treeErase(std::uint64_t rank) noexcept
    {
        const std::uint32_t root = splay(mRankRoot, rank);
        if (node(root).left == kNone)
            mRankRoot = node(root).right;
        else
        {
            // The left subtree's maximum becomes the root; its right child is empty.
            mRankRoot = splay(node(root).left, rank);
            node(mRankRoot).right = node(root).right;
        }
        node(root).left = mRankFree;
        mRankFree = root;
    }

    // --- Storage ---

    static std::uint32_t grownCapacity(std::uint32_t current, std::uint32_t needed) noexcept
    {
        std::uint64_t capacity = current < 8 ? 8 : std::uint64_t(current) * 2;
        capacity = std::max<std::uint64_t>(capacity, needed);
        return static_cast<std::uint32_t>(std::min<std::uint64_t>(capacity, Traits::slotLimit));
    }

    template<typename U>
    static constexpr bool fits(std::uint64_t count) noexcept
    {
        return count <= std::numeric_limits<std::size_t>::max() / sizeof(U);
    }

    template<typename U>
    U* allocateArray(std::size_t count) noexcept
    {
        return static_cast<U*>(mResource->allocate(count * sizeof(U), alignof(U)));
    }

    void release(Pending& pending) noexcept
    {
        mResource->deallocate(pending.boxes);
        mResource->deallocate(pending.payloads);
        mResource->deallocate(pending.meta);
        mResource->deallocate(pending.slots);
        mResource->deallocate(pending.ranks);
        pending = Pending{};
    }

    // Allocates new buffers for nonzero capacities; all or nothing.
    Status allocate(Pending& pending, std::uint32_t capacity, std::uint64_t slotCapacity,
        std::uint64_t rankCapacity) noexcept
    {
        if (!fits<Box<T>>(capacity) || !fits<Payload>(capacity) || !fits<Meta>(capacity) ||
            !fits<Slot>(slotCapacity) || !fits<RankCell>(rankCapacity))
            return Status::SizeLimitExceeded;

        bool ok = true;
        if (capacity != 0)
        {
            pending.boxes = allocateArray<Box<T>>(capacity);
            pending.payloads = allocateArray<Payload>(capacity);
            pending.meta = allocateArray<Meta>(capacity);
            pending.capacity = capacity;
            ok = pending.boxes && pending.payloads && pending.meta;
        }
        if (ok && slotCapacity != 0)
        {
            pending.slots = allocateArray<Slot>(static_cast<std::size_t>(slotCapacity));
            pending.slotCapacity = static_cast<std::uint32_t>(slotCapacity);
            ok = pending.slots != nullptr;
        }
        if (ok && rankCapacity != 0)
        {
            pending.ranks = allocateArray<RankCell>(static_cast<std::size_t>(rankCapacity));
            pending.rankCapacity = static_cast<std::size_t>(rankCapacity);
            ok = pending.ranks != nullptr;
        }

        if (!ok)
        {
            release(pending);
            return Status::AllocationFailed;
        }
        return Status::Ok;
    }

    // Moves live state into the pending buffers and frees the replaced ones.
    void commit(Pending& pending, std::uint32_t slotCount) noexcept
    {
        if (pending.payloads)
        {
            for (std::uint32_t i = 0; i < mSize; ++i)
            {
                std::construct_at(pending.boxes + i, mBoxes[i]);
                std::construct_at(pending.payloads + i, std::move(mPayloads[i]));
                std::destroy_at(mPayloads + i);
                std::construct_at(pending.meta + i, mMeta[i]);
            }
            mResource->deallocate(mBoxes);
            mResource->deallocate(mPayloads);
            mResource->deallocate(mMeta);
            mBoxes = pending.boxes;
            mPayloads = pending.payloads;
            mMeta = pending.meta;
            mCapacity = pending.capacity;
        }

        if (pending.slots)
        {
            for (std::uint32_t i = 0; i < slotCount; ++i)
                std::construct_at(pending.slots + i, mSlots[i]);
            mResource->deallocate(mSlots);
            mSlots = pending.slots;
            mSlotCapacity = pending.slotCapacity;
        }

        if (pending.ranks)
        {
            // Rehash from live metadata (after the dense commit above).
            mResource->deallocate(mRanks);
            mRanks = pending.ranks;
            mRankCapacity = pending.rankCapacity;
            resetRankCells();
            rebuildRanks();
        }

        pending = Pending{};
    }

    // Frees all buffers; payloads must already be destroyed.
    void releaseStorage() noexcept
    {
        mResource->deallocate(mBoxes);
        mResource->deallocate(mPayloads);
        mResource->deallocate(mMeta);
        mResource->deallocate(mSlots);
        mResource->deallocate(mRanks);
        mBoxes = nullptr;
        mPayloads = nullptr;
        mMeta = nullptr;
        mSlots = nullptr;
        mRanks = nullptr;
        mCapacity = 0;
        mSlotCapacity = 0;
        mRankCapacity = 0;
        mRankTree = false;
    }

    // Takes the source state except its resource; the source becomes empty.
    void adopt(EntryStore& other) noexcept
    {
        mBoxes = std::exchange(other.mBoxes, nullptr);
        mPayloads = std::exchange(other.mPayloads, nullptr);
        mMeta = std::exchange(other.mMeta, nullptr);
        mSlots = std::exchange(other.mSlots, nullptr);
        mRanks = std::exchange(other.mRanks, nullptr);
        mRankCapacity = std::exchange(other.mRankCapacity, 0);
        mRankCount = std::exchange(other.mRankCount, 0);
        mRankEpoch = std::exchange(other.mRankEpoch, 1);
        mRankTree = std::exchange(other.mRankTree, false);
        mRankRoot = std::exchange(other.mRankRoot, kNone);
        mRankFree = std::exchange(other.mRankFree, kNone);
        mRankHighWater = std::exchange(other.mRankHighWater, 0);
        mRankStats = std::exchange(other.mRankStats, RankStats{});
        mOwner = std::exchange(other.mOwner, 0);
        mSize = std::exchange(other.mSize, 0);
        mCapacity = std::exchange(other.mCapacity, 0);
        mSlotCapacity = std::exchange(other.mSlotCapacity, 0);
        mSlotsUsed = std::exchange(other.mSlotsUsed, 0);
        mFreeHead = std::exchange(other.mFreeHead, kNone);
        mRetired = std::exchange(other.mRetired, 0);
        mFreshGeneration = std::exchange(other.mFreshGeneration, 0);
    }
};
} // namespace detail

} // namespace QuadTree

namespace std
{
template<typename Tag>
struct hash<QuadTree::Handle<Tag>>
{
    std::size_t operator()(const QuadTree::Handle<Tag>& handle) const noexcept
    {
        return QuadTree::detail::HandleAccess::hash(handle);
    }
};
} // namespace std

#include "detail/quadtree-index-impl.h"
