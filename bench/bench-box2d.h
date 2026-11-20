#pragma once

// Box2D >= 3.2 b2DynamicTree baselines (bench/MANIFEST.md, step 2). The tree
// stores float AABBs and reports touching boxes, so every candidate is refined
// with the exact QT-FR-04 test on the benchmark's own geometry; the refinement
// cost is included. Only exact float conversions are used (int32, float scenes).

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <box2d/base.h>
#include <box2d/collision.h>
#include <box2d/math_functions.h>
#include <box2d/types.h>

#include "bench-memory.h"
#include "bench-persistent.h"

namespace bench
{
// Routes Box2D allocations through the counting allocator.
inline void installBox2DAllocator()
{
    b2SetAllocator([](std::size_t size, int alignment) -> void*
        { return memory::allocateAligned(size, static_cast<std::size_t>(alignment)); },
        [](void* ptr, std::size_t) { memory::freeAligned(ptr); });
}

// Fat = false: the tree holds exact boxes and every update calls MoveProxy.
// Fat = true: the tree holds boxes enlarged by kMargin (one step of maximum
// motion); MoveProxy runs only when the exact box leaves its enlarged box,
// as in Box2D's own broad phase.
template<typename T, bool Fat>
class Box2DTree
{
public:
    static constexpr std::string_view name = Fat ? "b2tree-fat" : "b2tree";
    static constexpr bool supportsMoved = true;
    static constexpr float kMargin = 4.0f;

    Box2DTree() = default;
    Box2DTree(const Box2DTree&) = delete;
    Box2DTree& operator=(const Box2DTree&) = delete;

    ~Box2DTree()
    {
        if (mTree)
            b2DestroyDynamicTree(mTree);
    }

    void reset(const EngineSetup<T>& setup)
    {
        if (mTree)
            b2DestroyDynamicTree(mTree);
        mTree = b2CreateDynamicTree(static_cast<int>(setup.initialCount));
        mBoundary = setup.boundary;
        mBoxes.assign(setup.idCapacity, Box<T>{});
        mClosed.assign(setup.idCapacity, 0);
        mProxy.assign(setup.idCapacity, -1);
        mFat.assign(Fat ? setup.idCapacity : 0, b2AABB{});
        mLive.reset(setup.idCapacity);
        mMembership.reset(setup.idCapacity);
    }

    void insert(std::uint32_t id, const Box<T>& box)
    {
        store(id, box);
        b2AABB aabb = toAabb(box);
        if constexpr (Fat)
        {
            aabb = enlarge(aabb);
            mFat[id] = aabb;
        }
        mProxy[id] = b2CreateTreeProxy(mTree, aabb, B2_DEFAULT_CATEGORY_BITS, id);
        mLive.insert(id);
    }

    void remove(std::uint32_t id)
    {
        b2DestroyTreeProxy(mTree, mProxy[id]);
        mLive.remove(id);
    }

    void move(std::uint32_t id, const Box<T>& box)
    {
        store(id, box);
        const b2AABB aabb = toAabb(box);
        if constexpr (Fat)
        {
            if (b2AABB_Contains(mFat[id], aabb))
                return;
            mFat[id] = enlarge(aabb);
            b2DynamicTree_MoveProxy(mTree, mProxy[id], mFat[id]);
        }
        else
            b2DynamicTree_MoveProxy(mTree, mProxy[id], aabb);
    }

    void finishUpdates() {}

    template<typename Sink>
    Counters pairs(Sink& sink, std::uint64_t limit)
    {
        Counters counters;
        for (const std::uint32_t a : mLive.ids())
        {
            if (counters.candidates > limit)
            {
                counters.capped = true;
                return counters;
            }
            query<Sink, false>(a, sink, counters);
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
            query<Sink, true>(a, sink, counters);
        }
        return counters;
    }

private:
    template<typename Sink>
    struct Query
    {
        const Box2DTree* self;
        Sink* sink;
        Counters* counters;
        std::uint32_t a;
    };

    b2DynamicTree* mTree = nullptr;
    Boundary mBoundary = Boundary::Default;
    std::vector<Box<T>> mBoxes; // By id, exact
    std::vector<std::uint8_t> mClosed;
    std::vector<int> mProxy;
    std::vector<b2AABB> mFat;
    LiveSet mLive;
    Membership mMembership;

    static b2AABB toAabb(const Box<T>& box) noexcept
    {
        return b2AABB{{static_cast<float>(box.minX), static_cast<float>(box.minY)},
            {static_cast<float>(box.maxX), static_cast<float>(box.maxY)}};
    }

    static b2AABB enlarge(const b2AABB& aabb) noexcept
    {
        return b2AABB{{aabb.lowerBound.x - kMargin, aabb.lowerBound.y - kMargin},
            {aabb.upperBound.x + kMargin, aabb.upperBound.y + kMargin}};
    }

    void store(std::uint32_t id, const Box<T>& box)
    {
        mBoxes[id] = box;
        mClosed[id] = closedFlag(box, mBoundary);
    }

    // Full mode reports pairs with b > a; moved mode skips b in the changed
    // set with b < a. Both refine exactly.
    template<typename Sink, bool Moved>
    static bool visit(int, std::uint64_t userData, void* context)
    {
        Query<Sink>& query = *static_cast<Query<Sink>*>(context);
        const Box2DTree& self = *query.self;
        const auto b = static_cast<std::uint32_t>(userData);
        if constexpr (Moved)
        {
            if (b == query.a || (self.mMembership.contains(b) && b < query.a))
                return true;
        }
        else if (b <= query.a)
            return true;

        ++query.counters->candidates;
        if (overlaps(self.mBoxes[query.a], self.mBoxes[b], self.mClosed[query.a] != 0 || self.mClosed[b] != 0))
            (*query.sink)(query.a, b);
        return true;
    }

    template<typename Sink, bool Moved>
    void query(std::uint32_t a, Sink& sink, Counters& counters) const
    {
        Query<Sink> context{this, &sink, &counters, a};
        const b2TreeStats stats = b2DynamicTree_OverlapAABB(mTree, toAabb(mBoxes[a]), B2_DEFAULT_MASK_BITS,
            &visit<Sink, Moved>, &context);
        counters.nodeVisits += static_cast<std::uint64_t>(stats.nodeVisits) + static_cast<std::uint64_t>(stats.leafVisits);
    }
};
} // namespace bench
