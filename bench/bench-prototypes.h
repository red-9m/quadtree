#pragma once

// Step 3 feasibility prototypes (bench/MANIFEST.md). Bench-only snapshot
// engines: adaptive quadtrees with different straddling strategies, and the
// QT-FR-24 ordered reference algorithm. Not the library implementation.
//
// Cells are half-open [lo, hi) per axis, closed at the root's far edges. A box
// overlaps a cell when minX < hi (<= on a closed edge) and maxX >= lo, per axis;
// a point lies in a cell when lo <= p < hi (<= on a closed edge). Split points
// are std::midpoint and are stored, so all tests compare stored coordinates
// exactly. The root is the data bounds (snapshot-derived extent).

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

#include "bench-engines.h"

namespace bench
{
struct PrototypeConfig
{
    static inline std::uint32_t leafCapacity = 8; // Split when more movable entries
    static inline std::uint32_t maxDepth = 24;
};

namespace proto
{
inline constexpr std::uint32_t kNone = ~std::uint32_t(0);

template<typename T>
struct Cell
{
    T loX, loY, hiX, hiY;
    bool closedX, closedY; // hi edge belongs to the cell (root far edges)
};

template<typename T>
bool overlapsCell(const Box<T>& box, const Cell<T>& cell) noexcept
{
    return (box.minX < cell.hiX || (cell.closedX && box.minX == cell.hiX)) && box.maxX >= cell.loX &&
        (box.minY < cell.hiY || (cell.closedY && box.minY == cell.hiY)) && box.maxY >= cell.loY;
}

template<typename T>
bool containsPoint(const Cell<T>& cell, T x, T y) noexcept
{
    return cell.loX <= x && (x < cell.hiX || (cell.closedX && x == cell.hiX)) && cell.loY <= y &&
        (y < cell.hiY || (cell.closedY && y == cell.hiY));
}

template<typename T>
bool boxesOverlapClosed(const Box<T>& a, const Box<T>& b) noexcept
{
    return a.minX <= b.maxX && b.minX <= a.maxX && a.minY <= b.maxY && b.minY <= a.maxY;
}

template<typename T>
Box<T> unite(const Box<T>& a, const Box<T>& b) noexcept
{
    return Box<T>{std::min(a.minX, b.minX), std::min(a.minY, b.minY), std::max(a.maxX, b.maxX),
        std::max(a.maxY, b.maxY)};
}

// Split of one node: per axis the split point, or no split when the midpoint
// makes no progress (zero or one-unit spans). Child k = xHalf + 2 yHalf.
template<typename T>
struct Split
{
    T cx, cy;
    bool splitX, splitY;
    double childW, childH; // Smaller child extent per axis (size test only)

    explicit Split(const Cell<T>& cell) noexcept
    {
        cx = std::midpoint(cell.loX, cell.hiX);
        cy = std::midpoint(cell.loY, cell.hiY);
        splitX = cell.loX < cx && cx < cell.hiX;
        splitY = cell.loY < cy && cy < cell.hiY;
        const double w = static_cast<double>(cell.hiX) - static_cast<double>(cell.loX);
        const double h = static_cast<double>(cell.hiY) - static_cast<double>(cell.loY);
        childW = splitX ? std::min(static_cast<double>(cx) - static_cast<double>(cell.loX),
                              static_cast<double>(cell.hiX) - static_cast<double>(cx))
                        : w;
        childH = splitY ? std::min(static_cast<double>(cy) - static_cast<double>(cell.loY),
                              static_cast<double>(cell.hiY) - static_cast<double>(cy))
                        : h;
    }

    bool canSplit() const noexcept { return splitX || splitY; }

    Cell<T> child(const Cell<T>& cell, std::uint32_t k) const noexcept
    {
        Cell<T> result = cell;
        if (splitX)
        {
            if ((k & 1) == 0)
            {
                result.hiX = cx;
                result.closedX = false;
            }
            else
                result.loX = cx;
        }
        if (splitY)
        {
            if ((k & 2) == 0)
            {
                result.hiY = cy;
                result.closedY = false;
            }
            else
                result.loY = cy;
        }
        return result;
    }
};

template<typename T>
struct Node
{
    Cell<T> cell;
    std::uint32_t firstChild = kNone; // Four consecutive nodes
    std::uint32_t itemBegin = 0;      // Entry references in items
    std::uint32_t itemEnd = 0;
    bool empty = true;                // No references in the subtree
};

template<typename T>
Cell<T> rootCell(std::span<const Box<T>> boxes) noexcept
{
    Cell<T> cell{boxes[0].minX, boxes[0].minY, boxes[0].maxX, boxes[0].maxY, true, true};
    for (const Box<T>& box : boxes)
    {
        cell.loX = std::min(cell.loX, box.minX);
        cell.loY = std::min(cell.loY, box.minY);
        cell.hiX = std::max(cell.hiX, box.maxX);
        cell.hiY = std::max(cell.hiY, box.maxY);
    }
    return cell;
}

enum class Placement
{
    Contained, // Descend only into a child containing the box (straddlers stay)
    Refs,      // Descend into every reached child while the extent fits the child size
    Center,    // Descend into the child holding the center while the extent fits
};

// Entry record moved through the build: geometry travels with the id, so
// every pass reads memory sequentially.
template<typename T>
struct Record
{
    Box<T> box;
    std::uint32_t id;
    std::uint8_t closed;
};

// Shared top-down builder. Each node classifies its references once into a
// child mask (0 = stays here), then distributes them by counting sort. Node
// references are stored contiguously as ids, boxes and closed flags.
template<typename T, Placement P>
class QuadtreeBuilder
{
protected:
    std::vector<Box<T>> mBoxes; // By id, for per-entry queries
    std::vector<std::uint8_t> mClosed;
    std::vector<Node<T>> mNodes;
    std::vector<std::uint32_t> mItems;   // Reference ids, grouped per node
    std::vector<Box<T>> mItemBoxes;      // Reference boxes, parallel to mItems
    std::vector<std::uint8_t> mItemClosed;
    std::vector<Record<T>> mScratch;     // Build buckets, stack discipline
    std::vector<std::uint8_t> mMask;     // Child mask per scratch position

    void buildTree(std::span<const Box<T>> boxes, Boundary boundary)
    {
        const std::size_t n = boxes.size();
        mBoxes.assign(boxes.begin(), boxes.end());
        mClosed.resize(n);
        mScratch.resize(n);
        mMask.resize(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            mClosed[i] = closedFlag(boxes[i], boundary);
            mScratch[i] = Record<T>{boxes[i], static_cast<std::uint32_t>(i), mClosed[i]};
        }
        mNodes.clear();
        mItems.clear();
        mItemBoxes.clear();
        mItemClosed.clear();
        if (n == 0)
            return;
        mNodes.push_back(Node<T>{rootCell<T>(boxes)});
        buildNode(0, 0, 0, n);
    }

private:
    void keep(const Record<T>& record)
    {
        mItems.push_back(record.id);
        mItemBoxes.push_back(record.box);
        mItemClosed.push_back(record.closed);
    }

    std::uint8_t classify(const Split<T>& split, const Box<T>& box) const noexcept
    {
        if constexpr (P == Placement::Contained)
        {
            std::uint8_t x = 1;
            if (split.splitX)
                x = box.maxX < split.cx ? 1 : box.minX >= split.cx ? 2 : 0;
            std::uint8_t y = 1;
            if (split.splitY)
                y = box.maxY < split.cy ? 1 : box.minY >= split.cy ? 2 : 0;
            if (x == 0 || y == 0)
                return 0;
            return static_cast<std::uint8_t>(1u << ((x >> 1) + 2 * (y >> 1)));
        }
        else
        {
            if (static_cast<double>(box.maxX) - static_cast<double>(box.minX) > split.childW ||
                static_cast<double>(box.maxY) - static_cast<double>(box.minY) > split.childH)
                return 0;
            if constexpr (P == Placement::Center)
            {
                const unsigned x = split.splitX && std::midpoint(box.minX, box.maxX) >= split.cx ? 1 : 0;
                const unsigned y = split.splitY && std::midpoint(box.minY, box.maxY) >= split.cy ? 2 : 0;
                return static_cast<std::uint8_t>(1u << (x | y));
            }
            else
            {
                const unsigned x = !split.splitX ? 1u : (box.minX < split.cx ? 1u : 0u) | (box.maxX >= split.cx ? 2u : 0u);
                const bool low = !split.splitY || box.minY < split.cy;
                const bool high = split.splitY && box.maxY >= split.cy;
                return static_cast<std::uint8_t>((low ? x : 0u) | (high ? x << 2 : 0u));
            }
        }
    }

    void buildNode(std::uint32_t node, std::uint32_t depth, std::size_t begin, std::size_t end)
    {
        const Cell<T> cell = mNodes[node].cell;
        const Split<T> split(cell);
        mNodes[node].empty = begin == end;
        mNodes[node].itemBegin = static_cast<std::uint32_t>(mItems.size());

        std::size_t moving = 0;
        if (split.canSplit() && depth < PrototypeConfig::maxDepth && end - begin > PrototypeConfig::leafCapacity)
            for (std::size_t i = begin; i < end; ++i)
            {
                mMask[i] = classify(split, mScratch[i].box);
                moving += mMask[i] != 0 ? 1u : 0u;
            }
        if (moving <= PrototypeConfig::leafCapacity)
        {
            for (std::size_t i = begin; i < end; ++i)
                keep(mScratch[i]);
            mNodes[node].itemEnd = static_cast<std::uint32_t>(mItems.size());
            return;
        }

        std::size_t count[4] = {};
        for (std::size_t i = begin; i < end; ++i)
        {
            const unsigned mask = mMask[i];
            if (mask == 0)
                keep(mScratch[i]);
            for (unsigned k = 0; k < 4; ++k)
                count[k] += (mask >> k) & 1u;
        }
        mNodes[node].itemEnd = static_cast<std::uint32_t>(mItems.size());

        std::size_t bucket[5];
        bucket[0] = mScratch.size();
        for (unsigned k = 0; k < 4; ++k)
            bucket[k + 1] = bucket[k] + count[k];
        mScratch.resize(bucket[4]);
        mMask.resize(bucket[4]);
        std::size_t fill[4] = {bucket[0], bucket[1], bucket[2], bucket[3]};
        for (std::size_t i = begin; i < end; ++i)
        {
            const unsigned mask = mMask[i];
            for (unsigned k = 0; k < 4; ++k)
                if ((mask >> k) & 1u)
                    mScratch[fill[k]++] = mScratch[i];
        }

        const auto first = static_cast<std::uint32_t>(mNodes.size());
        mNodes[node].firstChild = first;
        for (std::uint32_t k = 0; k < 4; ++k)
            mNodes.push_back(Node<T>{split.child(cell, k)});
        for (std::uint32_t k = 0; k < 4; ++k)
            buildNode(first + k, depth + 1, bucket[k], bucket[k + 1]);
        mScratch.resize(bucket[0]);
        mMask.resize(bucket[0]);
    }
};
} // namespace proto

// Region quadtree with entries referenced from cells.
// Classic (Refs = false): an entry descends only into a child that contains
//   it, so entries crossing a split line stay at that node (the current
//   library rule).
// Refs: an entry descends into every child it reaches while its extent fits
//   the child size, giving at most 4 references per entry independent of its
//   area. A pair is evaluated only at the deeper of the two reference cells
//   that contains (max minX, max minY): exactly once.
// Leaves split when more than leafCapacity entries could move down.
template<typename T, bool Refs>
class RegionQuadtree: protected proto::QuadtreeBuilder<T, Refs ? proto::Placement::Refs : proto::Placement::Contained>
{
public:
    static constexpr std::string_view name = Refs ? "qt-refs" : "qt-classic";

    void build(std::span<const Box<T>> boxes, Boundary boundary)
    {
        this->buildTree(boxes, boundary);
        mMaxRefs = 0;
        if constexpr (Refs)
        {
            mRefCount.assign(boxes.size(), 0);
            for (const std::uint32_t id : this->mItems)
                mMaxRefs = std::max(mMaxRefs, ++mRefCount[id]);
        }
        else
            mMaxRefs = boxes.empty() ? 0 : 1;
    }

    template<typename Sink>
    Counters pairs(Sink& sink, std::uint64_t limit)
    {
        Counters counters;
        counters.references = this->mItems.size();
        counters.maxReferences = mMaxRefs;
        mActive.clear();
        if (!this->mNodes.empty())
            visit(0, 0, 0, sink, counters, limit);
        return counters;
    }

    // Entries overlapping entry a's box, excluding a; each reported once.
    template<typename Callback>
    Counters query(std::uint32_t a, Callback&& callback) const
    {
        Counters counters;
        if (!this->mNodes.empty())
            queryNode(0, a, callback, counters);
        return counters;
    }

private:
    std::vector<std::uint32_t> mActive; // Ancestor reference indices per DFS level
    std::vector<std::uint32_t> mRefCount;
    std::uint32_t mMaxRefs = 0;

    bool reportHere(const Box<T>& a, const Box<T>& b, const proto::Cell<T>& cell) const noexcept
    {
        if constexpr (Refs)
            return proto::containsPoint(cell, std::max(a.minX, b.minX), std::max(a.minY, b.minY));
        else
            return true;
    }

    // References i and j (indices into the item arrays).
    template<typename Sink>
    void test(std::uint32_t i, std::uint32_t j, Sink& sink, Counters& counters) const
    {
        ++counters.candidates;
        if (overlaps(this->mItemBoxes[i], this->mItemBoxes[j], this->mItemClosed[i] != 0 || this->mItemClosed[j] != 0))
            sink(this->mItems[i], this->mItems[j]);
    }

    template<typename Sink>
    void visit(std::uint32_t node, std::size_t activeBegin, std::size_t activeEnd, Sink& sink, Counters& counters,
        std::uint64_t limit)
    {
        if (counters.candidates > limit)
        {
            counters.capped = true;
            return;
        }
        ++counters.nodeVisits;
        const proto::Node<T> current = this->mNodes[node];
        const std::vector<Box<T>>& boxes = this->mItemBoxes;
        for (std::uint32_t i = current.itemBegin; i < current.itemEnd; ++i)
        {
            for (std::uint32_t j = i + 1; j < current.itemEnd; ++j)
                if (reportHere(boxes[i], boxes[j], current.cell))
                    test(i, j, sink, counters);
            for (std::size_t k = activeBegin; k < activeEnd; ++k)
                if (reportHere(boxes[mActive[k]], boxes[i], current.cell))
                    test(mActive[k], i, sink, counters);
        }
        if (current.firstChild == proto::kNone)
            return;

        for (std::uint32_t k = 0; k < 4; ++k)
        {
            const std::uint32_t child = current.firstChild + k;
            if (this->mNodes[child].empty)
                continue;
            const proto::Cell<T> cell = this->mNodes[child].cell;
            const std::size_t begin = mActive.size();
            for (std::size_t i = activeBegin; i < activeEnd; ++i)
            {
                ++counters.cellTests;
                if (proto::overlapsCell(boxes[mActive[i]], cell))
                    mActive.push_back(mActive[i]);
            }
            for (std::uint32_t i = current.itemBegin; i < current.itemEnd; ++i)
            {
                ++counters.cellTests;
                if (proto::overlapsCell(boxes[i], cell))
                    mActive.push_back(i);
            }
            visit(child, begin, mActive.size(), sink, counters, limit);
            mActive.resize(begin);
        }
    }

    template<typename Callback>
    void queryNode(std::uint32_t node, std::uint32_t a, Callback& callback, Counters& counters) const
    {
        ++counters.nodeVisits;
        const proto::Node<T>& current = this->mNodes[node];
        const Box<T>& box = this->mBoxes[a];
        for (std::uint32_t i = current.itemBegin; i < current.itemEnd; ++i)
        {
            const std::uint32_t b = this->mItems[i];
            if (b == a || !reportHere(box, this->mItemBoxes[i], current.cell))
                continue;
            ++counters.candidates;
            if (overlaps(box, this->mItemBoxes[i], this->mClosed[a] != 0 || this->mItemClosed[i] != 0))
                callback(b);
        }
        if (current.firstChild == proto::kNone)
            return;
        for (std::uint32_t k = 0; k < 4; ++k)
        {
            const proto::Node<T>& child = this->mNodes[current.firstChild + k];
            ++counters.cellTests;
            if (!child.empty && proto::overlapsCell(box, child.cell))
                queryNode(current.firstChild + k, a, callback, counters);
        }
    }
};

// Loose quadtree: one reference per entry, at the deepest node whose cell holds
// the entry's center and whose child size still fits its extent (so the entry
// lies within the cell enlarged by half its size). Nodes keep the union of
// their subtree's boxes; pairs come from a dual traversal of node pairs with
// overlapping bounds.
template<typename T>
class LooseQuadtree: protected proto::QuadtreeBuilder<T, proto::Placement::Center>
{
public:
    static constexpr std::string_view name = "qt-loose";

    void build(std::span<const Box<T>> boxes, Boundary boundary)
    {
        this->buildTree(boxes, boundary);
        computeBounds();
    }

    template<typename Sink>
    Counters pairs(Sink& sink, std::uint64_t limit)
    {
        Counters counters;
        counters.references = this->mItems.size();
        counters.maxReferences = this->mItems.empty() ? 0 : 1;
        if (!this->mNodes.empty())
            self(0, sink, counters, limit);
        return counters;
    }

protected:
    std::vector<Box<T>> mBounds;     // Union of subtree boxes
    std::vector<Box<T>> mItemBounds; // Union of own boxes

    // Children follow their parent in mNodes: one reverse pass is bottom-up.
    void computeBounds()
    {
        const std::size_t count = this->mNodes.size();
        mBounds.resize(count);
        mItemBounds.resize(count);
        for (std::size_t n = count; n-- > 0;)
        {
            proto::Node<T>& node = this->mNodes[n];
            bool any = false;
            for (std::uint32_t i = node.itemBegin; i < node.itemEnd; ++i)
            {
                const Box<T>& box = this->mItemBoxes[i];
                mItemBounds[n] = any ? proto::unite(mItemBounds[n], box) : box;
                any = true;
            }
            mBounds[n] = mItemBounds[n];
            if (node.firstChild != proto::kNone)
                for (std::uint32_t k = 0; k < 4; ++k)
                {
                    const std::uint32_t child = node.firstChild + k;
                    if (this->mNodes[child].empty)
                        continue;
                    mBounds[n] = any ? proto::unite(mBounds[n], mBounds[child]) : mBounds[child];
                    any = true;
                }
            node.empty = !any;
        }
    }

private:
    // References i and j (indices into the item arrays).
    template<typename Sink>
    void test(std::uint32_t i, std::uint32_t j, Sink& sink, Counters& counters) const
    {
        ++counters.candidates;
        if (overlaps(this->mItemBoxes[i], this->mItemBoxes[j], this->mItemClosed[i] != 0 || this->mItemClosed[j] != 0))
            sink(this->mItems[i], this->mItems[j]);
    }

    bool hasItems(const proto::Node<T>& node) const noexcept { return node.itemBegin != node.itemEnd; }

    // Reference a (item index) against the subtree of node m.
    template<typename Sink>
    void itemVsSubtree(std::uint32_t a, std::uint32_t m, Sink& sink, Counters& counters) const
    {
        ++counters.nodeVisits;
        const proto::Node<T>& node = this->mNodes[m];
        for (std::uint32_t j = node.itemBegin; j < node.itemEnd; ++j)
            test(a, j, sink, counters);
        if (node.firstChild == proto::kNone)
            return;
        for (std::uint32_t k = 0; k < 4; ++k)
        {
            const std::uint32_t child = node.firstChild + k;
            ++counters.cellTests;
            if (!this->mNodes[child].empty && proto::boxesOverlapClosed(this->mItemBoxes[a], mBounds[child]))
                itemVsSubtree(a, child, sink, counters);
        }
    }

    // Own entries of node h against the subtree of node m.
    template<typename Sink>
    void itemsVsSubtree(std::uint32_t h, std::uint32_t m, Sink& sink, Counters& counters) const
    {
        const proto::Node<T>& holder = this->mNodes[h];
        ++counters.cellTests;
        if (!hasItems(holder) || this->mNodes[m].empty || !proto::boxesOverlapClosed(mItemBounds[h], mBounds[m]))
            return;
        for (std::uint32_t i = holder.itemBegin; i < holder.itemEnd; ++i)
        {
            ++counters.cellTests;
            if (proto::boxesOverlapClosed(this->mItemBoxes[i], mBounds[m]))
                itemVsSubtree(i, m, sink, counters);
        }
    }

    // All pairs between two disjoint subtrees.
    template<typename Sink>
    void cross(std::uint32_t a, std::uint32_t b, Sink& sink, Counters& counters) const
    {
        const proto::Node<T>& left = this->mNodes[a];
        const proto::Node<T>& right = this->mNodes[b];
        ++counters.cellTests;
        if (left.empty || right.empty || !proto::boxesOverlapClosed(mBounds[a], mBounds[b]))
            return;
        ++counters.nodeVisits;
        for (std::uint32_t i = left.itemBegin; i < left.itemEnd; ++i)
            for (std::uint32_t j = right.itemBegin; j < right.itemEnd; ++j)
                test(i, j, sink, counters);
        if (right.firstChild != proto::kNone)
            for (std::uint32_t k = 0; k < 4; ++k)
                itemsVsSubtree(a, right.firstChild + k, sink, counters);
        if (left.firstChild != proto::kNone)
            for (std::uint32_t k = 0; k < 4; ++k)
                itemsVsSubtree(b, left.firstChild + k, sink, counters);
        if (left.firstChild != proto::kNone && right.firstChild != proto::kNone)
            for (std::uint32_t k = 0; k < 4; ++k)
                for (std::uint32_t l = 0; l < 4; ++l)
                    cross(left.firstChild + k, right.firstChild + l, sink, counters);
    }

    template<typename Sink>
    void self(std::uint32_t n, Sink& sink, Counters& counters, std::uint64_t limit) const
    {
        const proto::Node<T>& node = this->mNodes[n];
        if (node.empty)
            return;
        if (counters.candidates > limit)
        {
            counters.capped = true;
            return;
        }
        ++counters.nodeVisits;
        for (std::uint32_t i = node.itemBegin; i < node.itemEnd; ++i)
            for (std::uint32_t j = i + 1; j < node.itemEnd; ++j)
                test(i, j, sink, counters);
        if (node.firstChild == proto::kNone)
            return;
        for (std::uint32_t k = 0; k < 4; ++k)
            itemsVsSubtree(n, node.firstChild + k, sink, counters);
        for (std::uint32_t k = 0; k < 4; ++k)
            self(node.firstChild + k, sink, counters, limit);
        for (std::uint32_t k = 0; k < 4; ++k)
            for (std::uint32_t l = k + 1; l < 4; ++l)
                cross(node.firstChild + k, node.firstChild + l, sink, counters);
    }
};

// Loose quadtree bulk load (snapshot): per entry a size level L (deepest level
// whose cell still fits its extent, at most kDepth) and the Morton code of its
// center truncated to level L; one LSD radix sort by (code, level) yields a
// pre-order layout: a node's own entries, then its four children's ranges.
// Nodes are cut from the sorted array by binary search; no per-level data
// movement. Same traversal as LooseQuadtree.
template<typename T>
class LooseSortedQuadtree: public LooseQuadtree<T>
{
public:
    static constexpr std::string_view name = "qt-loose-sort";

    void build(std::span<const Box<T>> boxes, Boundary boundary)
    {
        const std::size_t n = boxes.size();
        this->mBoxes.assign(boxes.begin(), boxes.end());
        this->mClosed.resize(n);
        this->mNodes.clear();
        this->mItems.resize(n);
        this->mItemBoxes.resize(n);
        this->mItemClosed.resize(n);
        if (n == 0)
        {
            this->computeBounds();
            return;
        }

        const proto::Cell<T> root = proto::rootCell<T>(boxes);
        const double lo_x = static_cast<double>(root.loX);
        const double lo_y = static_cast<double>(root.loY);
        const double span_x = static_cast<double>(root.hiX) - lo_x;
        const double span_y = static_cast<double>(root.hiY) - lo_y;
        const std::uint32_t depth = std::min<std::uint32_t>(kDepth, PrototypeConfig::maxDepth);
        const double cells = static_cast<double>(std::uint32_t(1) << depth);
        const double scale_x = span_x > 0 ? cells / span_x : 0;
        const double scale_y = span_y > 0 ? cells / span_y : 0;

        mKeys.resize(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            const Box<T>& box = boxes[i];
            this->mClosed[i] = closedFlag(box, boundary);
            const double w = static_cast<double>(box.maxX) - static_cast<double>(box.minX);
            const double h = static_cast<double>(box.maxY) - static_cast<double>(box.minY);
            std::uint32_t level = depth;
            while (level > 0 && ((w > 0 && w * scale_x > cells / static_cast<double>(std::uint32_t(1) << level)) ||
                                    (h > 0 && h * scale_y > cells / static_cast<double>(std::uint32_t(1) << level))))
                --level;
            const double cx = (static_cast<double>(box.minX) + static_cast<double>(box.maxX)) / 2;
            const double cy = (static_cast<double>(box.minY) + static_cast<double>(box.maxY)) / 2;
            const auto qx = static_cast<std::uint32_t>(std::clamp((cx - lo_x) * scale_x, 0.0, cells - 1));
            const auto qy = static_cast<std::uint32_t>(std::clamp((cy - lo_y) * scale_y, 0.0, cells - 1));
            const std::uint64_t code = interleave(qx, qy) >> (2 * (depth - level)) << (2 * (depth - level));
            mKeys[i] = code << 32 | std::uint64_t(level) << 27 | i;
        }
        radixSort();

        for (std::size_t k = 0; k < n; ++k)
        {
            const auto id = static_cast<std::uint32_t>(mKeys[k] & kIndexMask);
            this->mItems[k] = id;
            this->mItemBoxes[k] = boxes[id];
            this->mItemClosed[k] = this->mClosed[id];
        }
        this->mNodes.push_back(proto::Node<T>{root});
        buildNode(0, 0, depth, 0, n);
        this->computeBounds();
    }

private:
    static constexpr std::uint32_t kDepth = 16;
    static constexpr std::uint64_t kIndexMask = (std::uint64_t(1) << 27) - 1;
    std::vector<std::uint64_t> mKeys;
    std::vector<std::uint64_t> mTemp;

    static std::uint64_t spread(std::uint32_t v) noexcept
    {
        std::uint64_t x = v & 0xffffu;
        x = (x | (x << 8)) & 0x00ff00ffu;
        x = (x | (x << 4)) & 0x0f0f0f0fu;
        x = (x | (x << 2)) & 0x33333333u;
        x = (x | (x << 1)) & 0x55555555u;
        return x;
    }

    static std::uint64_t interleave(std::uint32_t x, std::uint32_t y) noexcept { return spread(x) | spread(y) << 1; }

    std::uint64_t code(std::size_t k) const noexcept { return mKeys[k] >> 32; }
    std::uint32_t level(std::size_t k) const noexcept { return static_cast<std::uint32_t>(mKeys[k] >> 27 & 31); }

    // LSD radix sort on bits 27..63 (level and code), 8-bit digits.
    void radixSort()
    {
        mTemp.resize(mKeys.size());
        for (unsigned shift = 27; shift < 64; shift += 8)
        {
            std::size_t count[257] = {};
            for (const std::uint64_t key : mKeys)
                ++count[(key >> shift & 0xff) + 1];
            for (unsigned d = 0; d < 256; ++d)
                count[d + 1] += count[d];
            for (const std::uint64_t key : mKeys)
                mTemp[count[key >> shift & 0xff]++] = key;
            mKeys.swap(mTemp);
        }
    }

    void buildNode(std::uint32_t node, std::uint32_t nodeLevel, std::uint32_t depth, std::size_t begin, std::size_t end)
    {
        this->mNodes[node].itemBegin = static_cast<std::uint32_t>(begin);
        this->mNodes[node].empty = begin == end;
        if (end - begin <= PrototypeConfig::leafCapacity || nodeLevel == depth)
        {
            this->mNodes[node].itemEnd = static_cast<std::uint32_t>(end);
            return;
        }
        // Own entries (level == nodeLevel) sort first in the node's range.
        std::size_t own = begin;
        while (own < end && level(own) == nodeLevel)
            ++own;
        this->mNodes[node].itemEnd = static_cast<std::uint32_t>(own);

        // Child k holds codes with digit k at this level.
        const unsigned shift = 2 * (depth - nodeLevel - 1);
        const std::uint64_t prefix = code(begin) >> (shift + 2) << (shift + 2);
        std::size_t bound[5];
        bound[0] = own;
        bound[4] = end;
        for (std::uint64_t k = 1; k < 4; ++k)
        {
            const std::uint64_t first = prefix | k << shift;
            std::size_t lo = bound[k - 1];
            std::size_t hi = end;
            while (lo < hi)
            {
                const std::size_t mid = lo + (hi - lo) / 2;
                if (code(mid) < first)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            bound[k] = lo;
        }

        const auto firstChild = static_cast<std::uint32_t>(this->mNodes.size());
        this->mNodes[node].firstChild = firstChild;
        for (std::uint32_t k = 0; k < 4; ++k)
            this->mNodes.push_back(proto::Node<T>{this->mNodes[node].cell});
        for (std::uint32_t k = 0; k < 4; ++k)
            buildNode(firstChild + k, nodeLevel + 1, depth, bound[k], bound[k + 1]);
    }
};

// Snapshot grid with a per-entry overlap query (reporting-cell rule against the
// query box), for the ordered reference algorithm.
template<typename T>
class QueryGrid: public UniformGrid<T>
{
public:
    template<typename Callback>
    Counters query(std::uint32_t a, Callback&& callback) const
    {
        return this->overlapping(a, callback);
    }
};

// QT-FR-24 reference: entries in rank order; per entry a partner query keeps
// higher ranks, the row is sorted by rank and emitted. O(n) workspace; output
// is lexicographic by (rank a, rank b). Ranks: mix64(id ^ 0x72616e6b), unique.
// Rank preparation (sort) counts as build time. Order violations are counted.
template<typename T, typename Base>
class OrderedEngine
{
public:
    static constexpr std::string_view name = Base::orderedName;

    void build(std::span<const Box<T>> boxes, Boundary boundary)
    {
        mBase.build(boxes, boundary);
        const std::size_t n = boxes.size();
        mRank.resize(n);
        mOrder.resize(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            mRank[i] = mix64(i ^ 0x72616e6bu);
            mOrder[i] = static_cast<std::uint32_t>(i);
        }
        std::sort(mOrder.begin(), mOrder.end(), [this](std::uint32_t a, std::uint32_t b) { return mRank[a] < mRank[b]; });
    }

    template<typename Sink>
    Counters pairs(Sink& sink, std::uint64_t limit)
    {
        Counters counters;
        std::uint64_t last_a = 0;
        std::uint64_t last_b = 0;
        bool first = true;
        for (const std::uint32_t a : mOrder)
        {
            if (counters.candidates > limit)
            {
                counters.capped = true;
                return counters;
            }
            mRow.clear();
            const std::uint64_t rank = mRank[a];
            const Counters query = mBase.query(a, [&](std::uint32_t b)
                {
                    if (mRank[b] > rank)
                        mRow.push_back(b);
                });
            counters.candidates += query.candidates;
            counters.nodeVisits += query.nodeVisits;
            counters.cellTests += query.cellTests;
            std::sort(mRow.begin(), mRow.end(), [this](std::uint32_t x, std::uint32_t y) { return mRank[x] < mRank[y]; });
            for (const std::uint32_t b : mRow)
            {
                if (!first && (rank < last_a || (rank == last_a && mRank[b] <= last_b)))
                    ++counters.orderViolations;
                first = false;
                last_a = rank;
                last_b = mRank[b];
                sink(a, b);
            }
        }
        return counters;
    }

private:
    Base mBase;
    std::vector<std::uint64_t> mRank;
    std::vector<std::uint32_t> mOrder;
    std::vector<std::uint32_t> mRow;
};

template<typename T>
struct OrderedRefs: RegionQuadtree<T, true>
{
    static constexpr std::string_view orderedName = "qt-refs-ordered";
};

template<typename T>
struct OrderedGrid: QueryGrid<T>
{
    static constexpr std::string_view orderedName = "grid-ordered";
};
} // namespace bench
