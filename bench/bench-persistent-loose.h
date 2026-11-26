#pragma once

// Step 3b prototype (bench/MANIFEST.md): persistent loose quadtree. Bench-only.
//
// Root = the fixed world box. An entry lives at the deepest node whose cell
// holds its center (half-open cells, closed at the root's far edges) and whose
// size fits its extent. Nodes keep tight bounds of their own entries and of
// their subtree; updates mark the node and its ancestors dirty, and
// finishUpdates() refits dirty subtrees bottom-up (O(changed paths)). Static
// loose bounds (cell enlarged by half its size) were measured first: sibling
// bounds always overlap and full enumeration needed ~5x the candidates of tight
// bounds (53 versus 10 per pair at 10^3). A leaf splits when more than
// leafCapacity of its entries fit a child; a node with children merges its
// subtree when the subtree holds at most leafCapacity / 2 entries (hysteresis).
// Child blocks of four nodes are recycled through a free list.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

#include "bench-persistent.h"
#include "bench-prototypes.h"

namespace bench
{
template<typename T>
class LoosePersistent
{
public:
    static constexpr std::string_view name = "qt-loose";
    static constexpr bool supportsMoved = true;

    void reset(const EngineSetup<T>& setup)
    {
        mBoundary = setup.boundary;
        mBoxes.assign(setup.idCapacity, Box<T>{});
        mClosed.assign(setup.idCapacity, 0);
        mNodeOf.assign(setup.idCapacity, kNone);
        mSlotOf.assign(setup.idCapacity, 0);
        mMembership.reset(setup.idCapacity);
        mFreeBlocks.clear();
        mNodes.resize(1);
        const Box<T>& world = setup.world;
        initNode(0, proto::Cell<T>{world.minX, world.minY, world.maxX, world.maxY, true, true}, kNone, 0,
            static_cast<double>(world.maxX) - static_cast<double>(world.minX),
            static_cast<double>(world.maxY) - static_cast<double>(world.minY));
    }

    void insert(std::uint32_t id, const Box<T>& box)
    {
        store(id, box);
        place(id);
    }

    void remove(std::uint32_t id)
    {
        const std::uint32_t node = mNodeOf[id];
        unlink(id);
        // Merge at the highest ancestor whose subtree became small.
        std::uint32_t merge = kNone;
        for (std::uint32_t n = node; n != kNone; n = mNodes[n].parent)
            if (mNodes[n].firstChild != kNone && mNodes[n].count <= PrototypeConfig::leafCapacity / 2)
                merge = n;
        if (merge != kNone)
            collapse(merge);
    }

    void move(std::uint32_t id, const Box<T>& box)
    {
        store(id, box);
        if (belongs(mNodeOf[id], box))
        {
            markDirty(mNodeOf[id]);
            return;
        }
        unlink(id);
        place(id);
    }

    void finishUpdates() { refit(0); }

    template<typename Sink>
    Counters pairs(Sink& sink, std::uint64_t limit)
    {
        Counters counters;
        self(0, sink, counters, limit);
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
            query(0, a, sink, counters);
        }
        return counters;
    }

private:
    static constexpr std::uint32_t kNone = proto::kNone;

    struct Node
    {
        proto::Cell<T> cell{};
        Box<T> bounds{};     // Subtree entries (valid when clean and count > 0)
        Box<T> itemBounds{}; // Own entries (valid when clean and items exist)
        bool dirty = true;
        T cx{}, cy{};
        double childW = 0, childH = 0; // Extent limits for descending into a child
        double fitW = 0, fitH = 0;     // Extent limits for this node (parent's child limits)
        bool canSplit = false;
        std::uint32_t parent = kNone;
        std::uint32_t firstChild = kNone;
        std::uint32_t depth = 0;
        std::uint32_t count = 0; // Entries in the subtree
        std::vector<std::uint32_t> items;
    };

    Boundary mBoundary = Boundary::Default;
    std::vector<Box<T>> mBoxes; // By id
    std::vector<std::uint8_t> mClosed;
    std::vector<std::uint32_t> mNodeOf;
    std::vector<std::uint32_t> mSlotOf;
    std::vector<Node> mNodes;
    std::vector<std::uint32_t> mFreeBlocks; // First node of a free block of four
    Membership mMembership;

    void store(std::uint32_t id, const Box<T>& box)
    {
        mBoxes[id] = box;
        mClosed[id] = closedFlag(box, mBoundary);
    }

    static double width(const Box<T>& box) noexcept
    {
        return static_cast<double>(box.maxX) - static_cast<double>(box.minX);
    }

    static double height(const Box<T>& box) noexcept
    {
        return static_cast<double>(box.maxY) - static_cast<double>(box.minY);
    }

    void initNode(std::uint32_t n, const proto::Cell<T>& cell, std::uint32_t parent, std::uint32_t depth, double fitW,
        double fitH)
    {
        Node& node = mNodes[n];
        const proto::Split<T> split(cell);
        node.cell = cell;
        node.cx = split.cx;
        node.cy = split.cy;
        node.childW = split.childW;
        node.childH = split.childH;
        node.fitW = fitW;
        node.fitH = fitH;
        node.canSplit = split.canSplit() && depth < PrototypeConfig::maxDepth;
        node.parent = parent;
        node.firstChild = kNone;
        node.depth = depth;
        node.count = 0;
        node.items.clear();
        node.dirty = true;
    }

    void markDirty(std::uint32_t n)
    {
        for (; n != kNone && !mNodes[n].dirty; n = mNodes[n].parent)
            mNodes[n].dirty = true;
    }

    // Invariant: a dirty node has dirty ancestors, so markDirty stops at the
    // first dirty node. Empty subtrees are cleaned completely to keep it.
    void clean(std::uint32_t n)
    {
        if (!mNodes[n].dirty)
            return;
        mNodes[n].dirty = false;
        if (mNodes[n].firstChild != kNone)
            for (std::uint32_t k = 0; k < 4; ++k)
                clean(mNodes[n].firstChild + k);
    }

    // Recomputes bounds of dirty nodes; clean subtrees are skipped.
    void refit(std::uint32_t n)
    {
        Node& node = mNodes[n];
        if (!node.dirty)
            return;
        node.dirty = false;
        bool any = false;
        for (const std::uint32_t id : node.items)
        {
            node.itemBounds = any ? proto::unite(node.itemBounds, mBoxes[id]) : mBoxes[id];
            any = true;
        }
        node.bounds = node.itemBounds;
        if (node.firstChild == kNone)
            return;
        for (std::uint32_t k = 0; k < 4; ++k)
        {
            const std::uint32_t child = node.firstChild + k;
            if (mNodes[child].count == 0)
            {
                clean(child);
                continue;
            }
            refit(child);
            mNodes[n].bounds = any ? proto::unite(mNodes[n].bounds, mNodes[child].bounds) : mNodes[child].bounds;
            any = true;
        }
    }

    bool fitsChild(const Node& node, const Box<T>& box) const noexcept
    {
        return node.firstChild != kNone && width(box) <= node.childW && height(box) <= node.childH;
    }

    std::uint32_t childByCenter(const Node& node, const Box<T>& box) const noexcept
    {
        const std::uint32_t x = std::midpoint(box.minX, box.maxX) >= node.cx ? 1 : 0;
        const std::uint32_t y = std::midpoint(box.minY, box.maxY) >= node.cy ? 2 : 0;
        return node.firstChild + (x | y);
    }

    bool belongs(std::uint32_t n, const Box<T>& box) const noexcept
    {
        const Node& node = mNodes[n];
        return proto::containsPoint(node.cell, std::midpoint(box.minX, box.maxX), std::midpoint(box.minY, box.maxY)) &&
            width(box) <= node.fitW && height(box) <= node.fitH && !fitsChild(node, box);
    }

    void link(std::uint32_t n, std::uint32_t id)
    {
        mNodeOf[id] = n;
        mSlotOf[id] = static_cast<std::uint32_t>(mNodes[n].items.size());
        mNodes[n].items.push_back(id);
        markDirty(n);
    }

    // Removes id from its node and the subtree counts; no merge.
    void unlink(std::uint32_t id)
    {
        const std::uint32_t n = mNodeOf[id];
        std::vector<std::uint32_t>& items = mNodes[n].items;
        const std::uint32_t slot = mSlotOf[id];
        items[slot] = items.back();
        mSlotOf[items[slot]] = slot;
        items.pop_back();
        for (std::uint32_t a = n; a != kNone; a = mNodes[a].parent)
            --mNodes[a].count;
        markDirty(n);
    }

    // Descends from the root, links id, updates counts, splits if needed.
    void place(std::uint32_t id)
    {
        const Box<T>& box = mBoxes[id];
        std::uint32_t n = 0;
        ++mNodes[0].count;
        while (fitsChild(mNodes[n], box))
        {
            n = childByCenter(mNodes[n], box);
            ++mNodes[n].count;
        }
        link(n, id);
        maybeSplit(n);
    }

    std::uint32_t allocateBlock()
    {
        if (!mFreeBlocks.empty())
        {
            const std::uint32_t first = mFreeBlocks.back();
            mFreeBlocks.pop_back();
            return first;
        }
        const auto first = static_cast<std::uint32_t>(mNodes.size());
        mNodes.resize(mNodes.size() + 4);
        return first;
    }

    void maybeSplit(std::uint32_t n)
    {
        {
            const Node& node = mNodes[n];
            if (node.firstChild != kNone || !node.canSplit || node.items.size() <= PrototypeConfig::leafCapacity)
                return;
            std::size_t movable = 0;
            for (const std::uint32_t id : node.items)
                movable += width(mBoxes[id]) <= node.childW && height(mBoxes[id]) <= node.childH ? 1u : 0u;
            if (movable <= PrototypeConfig::leafCapacity)
                return;
        }

        const std::uint32_t first = allocateBlock(); // May grow mNodes: no references held
        const proto::Split<T> split(mNodes[n].cell);
        for (std::uint32_t k = 0; k < 4; ++k)
            initNode(first + k, split.child(mNodes[n].cell, k), n, mNodes[n].depth + 1, mNodes[n].childW,
                mNodes[n].childH);
        mNodes[n].firstChild = first;
        mNodes[n].dirty = false; // Children start dirty; markDirty must reach the parent
        markDirty(n);

        // Movable entries go to the child holding their center; the rest stay.
        std::vector<std::uint32_t>& items = mNodes[n].items;
        std::size_t keep = 0;
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const std::uint32_t id = items[i];
            if (fitsChild(mNodes[n], mBoxes[id]))
            {
                const std::uint32_t child = childByCenter(mNodes[n], mBoxes[id]);
                ++mNodes[child].count;
                link(child, id);
            }
            else
            {
                items[keep] = id;
                mSlotOf[id] = static_cast<std::uint32_t>(keep);
                ++keep;
            }
        }
        items.resize(keep);
        for (std::uint32_t k = 0; k < 4; ++k)
            maybeSplit(first + k);
    }

    // Moves every entry of n's subtree into n and frees the child blocks.
    void collapse(std::uint32_t n)
    {
        const std::uint32_t first = mNodes[n].firstChild;
        mNodes[n].firstChild = kNone;
        for (std::uint32_t k = 0; k < 4; ++k)
            gather(first + k, n);
        mFreeBlocks.push_back(first);
        markDirty(n);
    }

    void gather(std::uint32_t child, std::uint32_t target)
    {
        for (const std::uint32_t id : mNodes[child].items)
            link(target, id);
        mNodes[child].items.clear();
        mNodes[child].count = 0;
        const std::uint32_t first = mNodes[child].firstChild;
        if (first != kNone)
        {
            mNodes[child].firstChild = kNone;
            for (std::uint32_t k = 0; k < 4; ++k)
                gather(first + k, target);
            mFreeBlocks.push_back(first);
        }
    }


    template<typename Sink>
    void test(std::uint32_t a, std::uint32_t b, Sink& sink, Counters& counters) const
    {
        ++counters.candidates;
        if (overlaps(mBoxes[a], mBoxes[b], mClosed[a] != 0 || mClosed[b] != 0))
            sink(a, b);
    }

    template<typename Sink>
    void itemVsSubtree(std::uint32_t a, std::uint32_t m, Sink& sink, Counters& counters) const
    {
        ++counters.nodeVisits;
        const Node& node = mNodes[m];
        for (const std::uint32_t b : node.items)
            test(a, b, sink, counters);
        if (node.firstChild == kNone)
            return;
        for (std::uint32_t k = 0; k < 4; ++k)
        {
            const std::uint32_t child = node.firstChild + k;
            ++counters.cellTests;
            if (mNodes[child].count != 0 && proto::boxesOverlapClosed(mBoxes[a], mNodes[child].bounds))
                itemVsSubtree(a, child, sink, counters);
        }
    }

    template<typename Sink>
    void itemsVsSubtree(std::uint32_t h, std::uint32_t m, Sink& sink, Counters& counters) const
    {
        ++counters.cellTests;
        if (mNodes[h].items.empty() || mNodes[m].count == 0 ||
            !proto::boxesOverlapClosed(mNodes[h].itemBounds, mNodes[m].bounds))
            return;
        for (const std::uint32_t a : mNodes[h].items)
        {
            ++counters.cellTests;
            if (proto::boxesOverlapClosed(mBoxes[a], mNodes[m].bounds))
                itemVsSubtree(a, m, sink, counters);
        }
    }

    template<typename Sink>
    void cross(std::uint32_t a, std::uint32_t b, Sink& sink, Counters& counters) const
    {
        const Node& left = mNodes[a];
        const Node& right = mNodes[b];
        ++counters.cellTests;
        if (left.count == 0 || right.count == 0 || !proto::boxesOverlapClosed(left.bounds, right.bounds))
            return;
        ++counters.nodeVisits;
        for (const std::uint32_t i : left.items)
            for (const std::uint32_t j : right.items)
                test(i, j, sink, counters);
        if (right.firstChild != kNone)
            for (std::uint32_t k = 0; k < 4; ++k)
                itemsVsSubtree(a, right.firstChild + k, sink, counters);
        if (left.firstChild != kNone)
            for (std::uint32_t k = 0; k < 4; ++k)
                itemsVsSubtree(b, left.firstChild + k, sink, counters);
        if (left.firstChild != kNone && right.firstChild != kNone)
            for (std::uint32_t k = 0; k < 4; ++k)
                for (std::uint32_t l = 0; l < 4; ++l)
                    cross(left.firstChild + k, right.firstChild + l, sink, counters);
    }

    template<typename Sink>
    void self(std::uint32_t n, Sink& sink, Counters& counters, std::uint64_t limit) const
    {
        const Node& node = mNodes[n];
        if (node.count == 0)
            return;
        if (counters.candidates > limit)
        {
            counters.capped = true;
            return;
        }
        ++counters.nodeVisits;
        for (std::size_t i = 0; i < node.items.size(); ++i)
            for (std::size_t j = i + 1; j < node.items.size(); ++j)
                test(node.items[i], node.items[j], sink, counters);
        if (node.firstChild == kNone)
            return;
        for (std::uint32_t k = 0; k < 4; ++k)
            itemsVsSubtree(n, node.firstChild + k, sink, counters);
        for (std::uint32_t k = 0; k < 4; ++k)
            self(node.firstChild + k, sink, counters, limit);
        for (std::uint32_t k = 0; k < 4; ++k)
            for (std::uint32_t l = k + 1; l < 4; ++l)
                cross(node.firstChild + k, node.firstChild + l, sink, counters);
    }

    // Changed entry a against the tree; skips a and changed entries below a.
    template<typename Sink>
    void query(std::uint32_t n, std::uint32_t a, Sink& sink, Counters& counters) const
    {
        ++counters.nodeVisits;
        const Node& node = mNodes[n];
        for (const std::uint32_t b : node.items)
        {
            if (b == a || (mMembership.contains(b) && b < a))
                continue;
            test(a, b, sink, counters);
        }
        if (node.firstChild == kNone)
            return;
        for (std::uint32_t k = 0; k < 4; ++k)
        {
            const std::uint32_t child = node.firstChild + k;
            ++counters.cellTests;
            if (mNodes[child].count != 0 && proto::boxesOverlapClosed(mBoxes[a], mNodes[child].bounds))
                query(child, a, sink, counters);
        }
    }
};
} // namespace bench
