#pragma once

// Persistent-model workloads and engines (bench/MANIFEST.md, step 2). Entries
// are inserted once, then every step applies one workload update and enumerates
// either all pairs or the pairs involving the changed set.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "bench-engines.h"

namespace bench
{
enum class Motion
{
    None,
    Small,    // Fixed velocity in [-4, 4]^2 per step, reflected at the world edge
    Teleport, // Uniform new position in the world
    Noop,     // Update with the unchanged box
    Churn,    // Remove random live entries, insert as many new ones
};

struct Workload
{
    std::string_view name;
    Motion motion;
    double fraction; // Share of entries changed per step
    bool single;     // Exactly one changed entry
};

inline constexpr Workload kWorkloads[] = {
    {"static", Motion::None, 0, false},
    {"small-single", Motion::Small, 0, true},
    {"small-1%", Motion::Small, 0.01, false},
    {"small-10%", Motion::Small, 0.10, false},
    {"small-100%", Motion::Small, 1.0, false},
    {"teleport-1%", Motion::Teleport, 0.01, false},
    {"teleport-10%", Motion::Teleport, 0.10, false},
    {"noop-100%", Motion::Noop, 1.0, false},
    {"churn-1%", Motion::Churn, 0.01, false},
    {"churn-10%", Motion::Churn, 0.10, false},
};

inline constexpr std::string_view kPersistentScenes[] = {"uniform", "clustered", "split-line", "mixed-size"};

enum class PairMode
{
    Full,  // All overlapping pairs
    Moved, // Pairs with at least one endpoint in the changed set
};

// Moved-set enumeration equals full enumeration when everything changes.
inline bool hasMovedMode(const Workload& workload) noexcept
{
    return workload.fraction < 1.0;
}

inline std::size_t changedCount(const Workload& workload, std::size_t n) noexcept
{
    if (workload.single)
        return std::min<std::size_t>(1, n);
    if (workload.fraction <= 0 || n == 0)
        return 0;
    return std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(workload.fraction * static_cast<double>(n))));
}

template<typename T>
struct Update
{
    enum class Op : std::uint8_t
    {
        Move,
        Insert,
        Remove,
    };

    Op op;
    std::uint32_t id;
    Box<T> box;
};

// Deterministic update sequence of one workload, in integer geometry.
// restart() replays it from the initial scene.
template<typename T>
class MotionScript
{
public:
    MotionScript(std::span<const IBox> initial, IBox world, const Workload& workload, std::size_t steps,
        std::uint64_t seed):
        mInitial(initial.begin(), initial.end()), mWorld(world), mWorkload(workload), mSeed(seed),
        mCount(changedCount(workload, initial.size())),
        mCapacity(initial.size() + (workload.motion == Motion::Churn ? steps * mCount : 0))
    {
        restart();
    }

    std::size_t idCapacity() const noexcept { return mCapacity; }

    void restart()
    {
        mBoxes.assign(mInitial.begin(), mInitial.end());
        mBoxes.resize(mCapacity);
        mNextId = static_cast<std::uint32_t>(mInitial.size());
        mRng = Rng(mSeed ^ 0x73746570u);

        // Movers: first mCount ids of a seeded Fisher-Yates permutation.
        const std::size_t n = mInitial.size();
        std::vector<std::uint32_t> ids(n);
        for (std::size_t i = 0; i < n; ++i)
            ids[i] = static_cast<std::uint32_t>(i);
        Rng pick(mSeed ^ 0x6d6f766572u);
        for (std::size_t i = 0; i < mCount && i + 1 < n; ++i)
            std::swap(ids[i], ids[i + static_cast<std::size_t>(pick.uniform(0, std::int64_t(n - i) - 1))]);
        mMovers.assign(ids.begin(), ids.begin() + static_cast<std::ptrdiff_t>(std::min(mCount, n)));

        mVelocity.clear();
        for (std::size_t i = 0; i < mMovers.size(); ++i)
        {
            const std::int64_t vx = pick.uniform(-4, 4);
            const std::int64_t vy = pick.uniform(-4, 4);
            mVelocity.push_back(vx);
            mVelocity.push_back(vy);
        }

        mLive.resize(n);
        for (std::size_t i = 0; i < n; ++i)
            mLive[i] = static_cast<std::uint32_t>(i);
    }

    // Fills the next step's updates and changed set (moved or inserted ids).
    void next(std::vector<Update<T>>& updates, std::vector<std::uint32_t>& changed)
    {
        updates.clear();
        changed.clear();
        switch (mWorkload.motion)
        {
        case Motion::None:
            break;
        case Motion::Small:
            for (std::size_t i = 0; i < mMovers.size(); ++i)
            {
                IBox& box = mBoxes[mMovers[i]];
                const std::int64_t x = advance(box.minX, box.maxX, mWorld.minX, mWorld.maxX, mVelocity[2 * i]);
                const std::int64_t y = advance(box.minY, box.maxY, mWorld.minY, mWorld.maxY, mVelocity[2 * i + 1]);
                box = IBox{x, y, x + (box.maxX - box.minX), y + (box.maxY - box.minY)};
                move(mMovers[i], updates, changed);
            }
            break;
        case Motion::Teleport:
            for (const std::uint32_t id : mMovers)
            {
                IBox& box = mBoxes[id];
                box = place(box.maxX - box.minX, box.maxY - box.minY);
                move(id, updates, changed);
            }
            break;
        case Motion::Noop:
            for (const std::uint32_t id : mMovers)
                move(id, updates, changed);
            break;
        case Motion::Churn:
            for (std::size_t i = 0; i < mCount && !mLive.empty(); ++i)
            {
                const auto index = static_cast<std::size_t>(mRng.uniform(0, std::int64_t(mLive.size()) - 1));
                updates.push_back(Update<T>{Update<T>::Op::Remove, mLive[index], {}});
                mLive[index] = mLive.back();
                mLive.pop_back();
            }
            for (std::size_t i = 0; i < mCount; ++i)
            {
                const IBox& source = mInitial[static_cast<std::size_t>(mRng.uniform(0, std::int64_t(mInitial.size()) - 1))];
                const std::uint32_t id = mNextId++;
                mBoxes[id] = place(source.maxX - source.minX, source.maxY - source.minY);
                mLive.push_back(id);
                updates.push_back(Update<T>{Update<T>::Op::Insert, id, toBox<T>(mBoxes[id])});
                changed.push_back(id);
            }
            break;
        }
    }

private:
    std::vector<IBox> mInitial;
    IBox mWorld;
    Workload mWorkload;
    std::uint64_t mSeed;
    std::size_t mCount;
    std::size_t mCapacity;
    std::vector<IBox> mBoxes; // By id
    std::vector<std::uint32_t> mMovers;
    std::vector<std::int64_t> mVelocity; // vx, vy per mover
    std::vector<std::uint32_t> mLive;
    std::uint32_t mNextId = 0;
    Rng mRng{0};

    // Moves [lo, hi] by velocity; at the world edge it stops there and reverses.
    static std::int64_t advance(std::int64_t lo, std::int64_t hi, std::int64_t worldLo, std::int64_t worldHi,
        std::int64_t& velocity) noexcept
    {
        std::int64_t start = lo + velocity;
        if (start < worldLo || start + (hi - lo) > worldHi)
        {
            start = std::clamp(start, worldLo, worldHi - (hi - lo));
            velocity = -velocity;
        }
        return start;
    }

    IBox place(std::int64_t w, std::int64_t h)
    {
        const std::int64_t x = mRng.uniform(mWorld.minX, mWorld.maxX - w);
        const std::int64_t y = mRng.uniform(mWorld.minY, mWorld.maxY - h);
        return IBox{x, y, x + w, y + h};
    }

    void move(std::uint32_t id, std::vector<Update<T>>& updates, std::vector<std::uint32_t>& changed)
    {
        updates.push_back(Update<T>{Update<T>::Op::Move, id, toBox<T>(mBoxes[id])});
        changed.push_back(id);
    }
};

template<typename T>
struct EngineSetup
{
    std::size_t idCapacity = 0;
    std::size_t initialCount = 0;
    Box<T> world{};
    double cellSize = 1; // Grid engines
    Boundary boundary = Boundary::Default;
};

// Changed-set membership by epoch stamps: O(|S|) per step, no O(n) clear.
class Membership
{
public:
    void reset(std::size_t capacity)
    {
        mStamp.assign(capacity, 0);
        mEpoch = 0;
    }

    void begin(std::span<const std::uint32_t> ids)
    {
        if (++mEpoch == 0)
        {
            std::fill(mStamp.begin(), mStamp.end(), 0);
            mEpoch = 1;
        }
        for (const std::uint32_t id : ids)
            mStamp[id] = mEpoch;
    }

    bool contains(std::uint32_t id) const noexcept { return mStamp[id] == mEpoch; }

private:
    std::vector<std::uint32_t> mStamp;
    std::uint32_t mEpoch = 0;
};

// Live id list with O(1) removal.
class LiveSet
{
public:
    void reset(std::size_t capacity)
    {
        mIds.clear();
        mSlot.assign(capacity, 0);
    }

    void insert(std::uint32_t id)
    {
        mSlot[id] = static_cast<std::uint32_t>(mIds.size());
        mIds.push_back(id);
    }

    void remove(std::uint32_t id)
    {
        const std::uint32_t slot = mSlot[id];
        mIds[slot] = mIds.back();
        mSlot[mIds[slot]] = slot;
        mIds.pop_back();
    }

    std::span<const std::uint32_t> ids() const noexcept { return mIds; }
    std::uint32_t slot(std::uint32_t id) const noexcept { return mSlot[id]; }

private:
    std::vector<std::uint32_t> mIds;
    std::vector<std::uint32_t> mSlot;
};

// Reference: all pairs of live entries; changed set against all live entries.
template<typename T>
class BrutePersistent
{
public:
    static constexpr std::string_view name = "brute";
    static constexpr bool supportsMoved = true;

    void reset(const EngineSetup<T>& setup)
    {
        mBoundary = setup.boundary;
        mBoxes.assign(setup.idCapacity, Box<T>{});
        mClosed.assign(setup.idCapacity, 0);
        mLive.reset(setup.idCapacity);
        mMembership.reset(setup.idCapacity);
    }

    void insert(std::uint32_t id, const Box<T>& box)
    {
        move(id, box);
        mLive.insert(id);
    }

    void remove(std::uint32_t id) { mLive.remove(id); }

    void move(std::uint32_t id, const Box<T>& box)
    {
        mBoxes[id] = box;
        mClosed[id] = closedFlag(box, mBoundary);
    }

    void finishUpdates() {}

    template<typename Sink>
    Counters pairs(Sink& sink, std::uint64_t limit) const
    {
        Counters counters;
        const auto ids = mLive.ids();
        for (std::size_t i = 0; i < ids.size(); ++i)
        {
            if (counters.candidates > limit)
            {
                counters.capped = true;
                return counters;
            }
            const std::uint32_t a = ids[i];
            counters.candidates += ids.size() - i - 1;
            for (std::size_t j = i + 1; j < ids.size(); ++j)
                if (overlaps(mBoxes[a], mBoxes[ids[j]], mClosed[a] != 0 || mClosed[ids[j]] != 0))
                    sink(a, ids[j]);
        }
        return counters;
    }

    template<typename Sink>
    Counters pairsInvolving(std::span<const std::uint32_t> changed, Sink& sink, std::uint64_t limit)
    {
        Counters counters;
        mMembership.begin(changed);
        for (const std::uint32_t a : changed)
        {
            if (counters.candidates > limit)
            {
                counters.capped = true;
                return counters;
            }
            for (const std::uint32_t b : mLive.ids())
            {
                if (b == a || (mMembership.contains(b) && b < a))
                    continue;
                ++counters.candidates;
                if (overlaps(mBoxes[a], mBoxes[b], mClosed[a] != 0 || mClosed[b] != 0))
                    sink(a, b);
            }
        }
        return counters;
    }

private:
    Boundary mBoundary = Boundary::Default;
    std::vector<Box<T>> mBoxes; // By id
    std::vector<std::uint8_t> mClosed;
    LiveSet mLive;
    Membership mMembership;
};

// Sort-and-sweep keeping its order between steps. finishUpdates() drops the
// changed entries from the order, sorts them and merges them back: O(n + k log k).
// Moved-set queries scan the sorted order from minX - (largest width).
template<typename T>
class SweepPersistent
{
public:
    static constexpr std::string_view name = "sweep";
    static constexpr bool supportsMoved = true;

    void reset(const EngineSetup<T>& setup)
    {
        mBoundary = setup.boundary;
        mBoxes.assign(setup.idCapacity, Box<T>{});
        mClosed.assign(setup.idCapacity, 0);
        mAlive.assign(setup.idCapacity, 0);
        mChangedStamp.assign(setup.idCapacity, 0);
        mEpoch = 1;
        mOrder.clear();
        mChanged.clear();
        mSorted.clear();
        mSortedIds.clear();
        mMembership.reset(setup.idCapacity);
        mMaxWidth = T(0);
    }

    void insert(std::uint32_t id, const Box<T>& box)
    {
        mAlive[id] = 1;
        move(id, box);
    }

    void remove(std::uint32_t id)
    {
        mAlive[id] = 0;
        mark(id);
    }

    void move(std::uint32_t id, const Box<T>& box)
    {
        mBoxes[id] = box;
        mClosed[id] = closedFlag(box, mBoundary);
        mark(id);
    }

    void finishUpdates()
    {
        if (mChanged.empty())
            return;

        // Unchanged entries keep their relative order.
        mMerged.clear();
        for (const std::uint32_t id : mOrder)
            if (mChangedStamp[id] != mEpoch)
                mMerged.push_back(id);
        mOrder.swap(mMerged);

        mMoved.clear();
        for (const std::uint32_t id : mChanged)
            if (mAlive[id] != 0)
                mMoved.push_back(id);
        const auto less = [this](std::uint32_t a, std::uint32_t b)
        { return mBoxes[a].minX < mBoxes[b].minX || (mBoxes[a].minX == mBoxes[b].minX && a < b); };
        std::sort(mMoved.begin(), mMoved.end(), less);
        mMerged.resize(mOrder.size() + mMoved.size());
        std::merge(mOrder.begin(), mOrder.end(), mMoved.begin(), mMoved.end(), mMerged.begin(), less);
        mOrder.swap(mMerged);

        mSorted.resize(mOrder.size());
        mSortedIds.resize(mOrder.size());
        mMaxWidth = T(0);
        for (std::size_t k = 0; k < mOrder.size(); ++k)
        {
            mSorted[k] = mBoxes[mOrder[k]];
            mSortedIds[k] = mOrder[k];
            mMaxWidth = std::max(mMaxWidth, static_cast<T>(mSorted[k].maxX - mSorted[k].minX));
        }

        mChanged.clear();
        ++mEpoch;
    }

    template<typename Sink>
    Counters pairs(Sink& sink, std::uint64_t limit) const
    {
        Counters counters;
        const std::size_t n = mSorted.size();
        for (std::size_t i = 0; i < n; ++i)
        {
            if (counters.candidates > limit)
            {
                counters.capped = true;
                return counters;
            }
            const Box<T> a = mSorted[i];
            const bool closed = mClosed[mSortedIds[i]] != 0;
            for (std::size_t j = i + 1; j < n && mSorted[j].minX <= a.maxX; ++j)
            {
                ++counters.candidates;
                if (overlaps(a, mSorted[j], closed || mClosed[mSortedIds[j]] != 0))
                    sink(mSortedIds[i], mSortedIds[j]);
            }
        }
        return counters;
    }

    template<typename Sink>
    Counters pairsInvolving(std::span<const std::uint32_t> changed, Sink& sink, std::uint64_t limit)
    {
        Counters counters;
        mMembership.begin(changed);
        for (const std::uint32_t a : changed)
        {
            if (counters.candidates > limit)
            {
                counters.capped = true;
                return counters;
            }
            const Box<T> box = mBoxes[a];
            const bool closed = mClosed[a] != 0;
            // Every x-overlapping entry has minX >= box.minX - mMaxWidth.
            const T from = box.minX - mMaxWidth;
            auto j = static_cast<std::size_t>(std::lower_bound(mSorted.begin(), mSorted.end(), from,
                [](const Box<T>& entry, T value) { return entry.minX < value; }) - mSorted.begin());
            for (; j < mSorted.size() && mSorted[j].minX <= box.maxX; ++j)
            {
                const std::uint32_t b = mSortedIds[j];
                if (b == a || (mMembership.contains(b) && b < a))
                    continue;
                ++counters.candidates;
                if (overlaps(box, mSorted[j], closed || mClosed[b] != 0))
                    sink(a, b);
            }
        }
        return counters;
    }

private:
    Boundary mBoundary = Boundary::Default;
    std::vector<Box<T>> mBoxes; // By id
    std::vector<std::uint8_t> mClosed;
    std::vector<std::uint8_t> mAlive;
    std::vector<std::uint32_t> mChangedStamp;
    std::uint32_t mEpoch = 1;
    std::vector<std::uint32_t> mChanged;
    std::vector<std::uint32_t> mOrder;
    std::vector<std::uint32_t> mMoved;
    std::vector<std::uint32_t> mMerged;
    std::vector<Box<T>> mSorted;
    std::vector<std::uint32_t> mSortedIds;
    Membership mMembership;
    T mMaxWidth = T(0);

    void mark(std::uint32_t id)
    {
        if (mChangedStamp[id] != mEpoch)
        {
            mChangedStamp[id] = mEpoch;
            mChanged.push_back(id);
        }
    }
};

// Uniform grid over the fixed world, updated in place. An update that keeps
// the covered cell range only stores the new box.
template<typename T>
class GridPersistent
{
public:
    static constexpr std::string_view name = "grid";
    static constexpr bool supportsMoved = true;

    void reset(const EngineSetup<T>& setup)
    {
        mBoundary = setup.boundary;
        mOriginX = static_cast<double>(setup.world.minX);
        mOriginY = static_cast<double>(setup.world.minY);
        mInverse = 1.0 / setup.cellSize;
        mColumns = static_cast<std::uint32_t>((static_cast<double>(setup.world.maxX) - mOriginX) * mInverse) + 1;
        mRows = static_cast<std::uint32_t>((static_cast<double>(setup.world.maxY) - mOriginY) * mInverse) + 1;
        mCells.assign(std::size_t(mColumns) * mRows, {});
        mBoxes.assign(setup.idCapacity, Box<T>{});
        mClosed.assign(setup.idCapacity, 0);
        mRange.assign(4 * setup.idCapacity, 0);
        mMembership.reset(setup.idCapacity);
    }

    void insert(std::uint32_t id, const Box<T>& box)
    {
        store(id, box);
        add(id);
    }

    void remove(std::uint32_t id) { erase(id); }

    void move(std::uint32_t id, const Box<T>& box)
    {
        const std::uint32_t x0 = column(box.minX);
        const std::uint32_t y0 = row(box.minY);
        const std::uint32_t x1 = column(box.maxX);
        const std::uint32_t y1 = row(box.maxY);
        if (x0 == mRange[4 * id] && y0 == mRange[4 * id + 1] && x1 == mRange[4 * id + 2] && y1 == mRange[4 * id + 3])
        {
            mBoxes[id] = box;
            mClosed[id] = closedFlag(box, mBoundary);
            return;
        }
        erase(id);
        store(id, box);
        add(id);
    }

    void finishUpdates() {}

    template<typename Sink>
    Counters pairs(Sink& sink, std::uint64_t limit) const
    {
        Counters counters;
        for (std::size_t c = 0; c < mCells.size(); ++c)
        {
            if (counters.candidates > limit)
            {
                counters.capped = true;
                return counters;
            }
            const std::vector<std::uint32_t>& cell = mCells[c];
            counters.references += cell.size();
            for (std::size_t p = 0; p < cell.size(); ++p)
            {
                const std::uint32_t a = cell[p];
                for (std::size_t q = p + 1; q < cell.size(); ++q)
                {
                    const std::uint32_t b = cell[q];
                    if (reportingCell(a, b) != c)
                        continue;
                    ++counters.candidates;
                    if (overlaps(mBoxes[a], mBoxes[b], mClosed[a] != 0 || mClosed[b] != 0))
                        sink(a, b);
                }
            }
        }
        return counters;
    }

    template<typename Sink>
    Counters pairsInvolving(std::span<const std::uint32_t> changed, Sink& sink, std::uint64_t limit)
    {
        Counters counters;
        mMembership.begin(changed);
        for (const std::uint32_t a : changed)
        {
            if (counters.candidates > limit)
            {
                counters.capped = true;
                return counters;
            }
            for (std::uint32_t y = mRange[4 * a + 1]; y <= mRange[4 * a + 3]; ++y)
                for (std::uint32_t x = mRange[4 * a]; x <= mRange[4 * a + 2]; ++x)
                {
                    const std::size_t c = std::size_t(y) * mColumns + x;
                    for (const std::uint32_t b : mCells[c])
                    {
                        if (b == a || (mMembership.contains(b) && b < a) || reportingCell(a, b) != c)
                            continue;
                        ++counters.candidates;
                        if (overlaps(mBoxes[a], mBoxes[b], mClosed[a] != 0 || mClosed[b] != 0))
                            sink(a, b);
                    }
                }
        }
        return counters;
    }

private:
    Boundary mBoundary = Boundary::Default;
    double mOriginX = 0;
    double mOriginY = 0;
    double mInverse = 1;
    std::uint32_t mColumns = 0;
    std::uint32_t mRows = 0;
    std::vector<std::vector<std::uint32_t>> mCells;
    std::vector<Box<T>> mBoxes; // By id
    std::vector<std::uint8_t> mClosed;
    std::vector<std::uint32_t> mRange; // x0, y0, x1, y1 per id
    Membership mMembership;

    std::uint32_t column(T x) const noexcept
    {
        const double offset = std::max(0.0, (static_cast<double>(x) - mOriginX) * mInverse);
        return std::min(mColumns - 1, static_cast<std::uint32_t>(offset));
    }

    std::uint32_t row(T y) const noexcept
    {
        const double offset = std::max(0.0, (static_cast<double>(y) - mOriginY) * mInverse);
        return std::min(mRows - 1, static_cast<std::uint32_t>(offset));
    }

    std::size_t reportingCell(std::uint32_t a, std::uint32_t b) const noexcept
    {
        const std::uint32_t x = std::max(mRange[4 * a], mRange[4 * b]);
        const std::uint32_t y = std::max(mRange[4 * a + 1], mRange[4 * b + 1]);
        return std::size_t(y) * mColumns + x;
    }

    void store(std::uint32_t id, const Box<T>& box)
    {
        mBoxes[id] = box;
        mClosed[id] = closedFlag(box, mBoundary);
        mRange[4 * id] = column(box.minX);
        mRange[4 * id + 1] = row(box.minY);
        mRange[4 * id + 2] = column(box.maxX);
        mRange[4 * id + 3] = row(box.maxY);
    }

    void add(std::uint32_t id)
    {
        for (std::uint32_t y = mRange[4 * id + 1]; y <= mRange[4 * id + 3]; ++y)
            for (std::uint32_t x = mRange[4 * id]; x <= mRange[4 * id + 2]; ++x)
                mCells[std::size_t(y) * mColumns + x].push_back(id);
    }

    void erase(std::uint32_t id)
    {
        for (std::uint32_t y = mRange[4 * id + 1]; y <= mRange[4 * id + 3]; ++y)
            for (std::uint32_t x = mRange[4 * id]; x <= mRange[4 * id + 2]; ++x)
            {
                std::vector<std::uint32_t>& cell = mCells[std::size_t(y) * mColumns + x];
                const auto it = std::find(cell.begin(), cell.end(), id);
                *it = cell.back();
                cell.pop_back();
            }
    }
};

// Snapshot grid rebuilt from all live boxes after every step's updates.
template<typename T>
class GridRebuild
{
public:
    static constexpr std::string_view name = "grid-rebuild";
    static constexpr bool supportsMoved = false;

    void reset(const EngineSetup<T>& setup)
    {
        mBoundary = setup.boundary;
        mDense.clear();
        mIds.clear();
        mSlot.assign(setup.idCapacity, 0);
    }

    void insert(std::uint32_t id, const Box<T>& box)
    {
        mSlot[id] = static_cast<std::uint32_t>(mDense.size());
        mDense.push_back(box);
        mIds.push_back(id);
    }

    void remove(std::uint32_t id)
    {
        const std::uint32_t slot = mSlot[id];
        mDense[slot] = mDense.back();
        mIds[slot] = mIds.back();
        mSlot[mIds[slot]] = slot;
        mDense.pop_back();
        mIds.pop_back();
    }

    void move(std::uint32_t id, const Box<T>& box) { mDense[mSlot[id]] = box; }

    // The rebuild belongs to the update phase.
    void finishUpdates() { mGrid.build(mDense, mBoundary); }

    template<typename Sink>
    Counters pairs(Sink& sink, std::uint64_t limit)
    {
        IdSink<Sink> mapped{mIds, sink};
        return mGrid.pairs(mapped, limit);
    }

    template<typename Sink>
    Counters pairsInvolving(std::span<const std::uint32_t>, Sink&, std::uint64_t)
    {
        return {};
    }

private:
    template<typename Sink>
    struct IdSink
    {
        const std::vector<std::uint32_t>& ids;
        Sink& sink;

        void operator()(std::uint32_t a, std::uint32_t b) { sink(ids[a], ids[b]); }
    };

    Boundary mBoundary = Boundary::Default;
    std::vector<Box<T>> mDense;
    std::vector<std::uint32_t> mIds;
    std::vector<std::uint32_t> mSlot;
    UniformGrid<T> mGrid;
};
} // namespace bench
