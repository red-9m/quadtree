#pragma once

// Baseline broad-phase engines for the snapshot benchmark. All share the
// QT-FR-04 overlap rule, report each overlapping unordered pair exactly once,
// and keep their buffers across builds (retained capacity).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "bench-scenes.h"

namespace bench
{
template<typename T>
using Box = QuadTree::Box<T>;

// Work counters of one pairs() call.
struct Counters
{
    std::uint64_t candidates = 0; // Entry-vs-entry overlap evaluations, repeats included
    std::uint64_t references = 0; // Stored entry references (grid cells)
    std::uint64_t nodeVisits = 0; // Tree nodes (internal and leaf) or grid cells visited
    std::uint64_t cellTests = 0;  // Entry-vs-cell and bounds-vs-bounds tests
    std::uint32_t maxReferences = 0; // Largest number of references of one entry
    std::uint64_t orderViolations = 0; // Ordered engines: pairs out of rank order
    bool capped = false;          // Stopped after exceeding the candidate limit
};

// Closed comparisons when touch-inclusive or either box is degenerate.
template<typename T>
bool closedFlag(const Box<T>& box, Boundary boundary) noexcept
{
    return boundary == Boundary::Touch || box.minX == box.maxX || box.minY == box.maxY;
}

template<typename T>
bool overlaps(const Box<T>& a, const Box<T>& b, bool closed) noexcept
{
    if (closed)
        return a.minX <= b.maxX && b.minX <= a.maxX && a.minY <= b.maxY && b.minY <= a.maxY;
    return a.minX < b.maxX && b.minX < a.maxX && a.minY < b.maxY && b.minY < a.maxY;
}

// All n(n-1)/2 pairs.
template<typename T>
class BruteForce
{
public:
    static constexpr std::string_view name = "brute";

    void build(std::span<const Box<T>> boxes, Boundary boundary)
    {
        mBoxes.assign(boxes.begin(), boxes.end());
        mClosed.resize(boxes.size());
        for (std::size_t i = 0; i < boxes.size(); ++i)
            mClosed[i] = closedFlag(boxes[i], boundary);
    }

    template<typename Sink>
    Counters pairs(Sink& sink, std::uint64_t limit) const
    {
        Counters counters;
        const auto n = static_cast<std::uint32_t>(mBoxes.size());
        for (std::uint32_t i = 0; i < n; ++i)
        {
            if (counters.candidates > limit)
            {
                counters.capped = true;
                return counters;
            }
            const Box<T> a = mBoxes[i];
            const bool closed = mClosed[i] != 0;
            counters.candidates += n - i - 1;
            for (std::uint32_t j = i + 1; j < n; ++j)
                if (overlaps(a, mBoxes[j], closed || mClosed[j] != 0))
                    sink(i, j);
        }
        return counters;
    }

private:
    std::vector<Box<T>> mBoxes;
    std::vector<std::uint8_t> mClosed;
};

// Sort by minX, sweep while the next minX does not exceed the current maxX.
template<typename T>
class SweepAndPrune
{
public:
    static constexpr std::string_view name = "sweep";

    void build(std::span<const Box<T>> boxes, Boundary boundary)
    {
        const std::size_t n = boxes.size();
        mKeys.resize(n);
        for (std::size_t i = 0; i < n; ++i)
            mKeys[i] = Key{boxes[i].minX, static_cast<std::uint32_t>(i)};
        std::sort(mKeys.begin(), mKeys.end(), [](const Key& a, const Key& b)
            { return a.minX < b.minX || (a.minX == b.minX && a.id < b.id); });

        mSorted.resize(n);
        mIds.resize(n);
        mClosed.resize(n);
        for (std::size_t k = 0; k < n; ++k)
        {
            mSorted[k] = boxes[mKeys[k].id];
            mIds[k] = mKeys[k].id;
            mClosed[k] = closedFlag(mSorted[k], boundary);
        }
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
            const bool closed = mClosed[i] != 0;
            for (std::size_t j = i + 1; j < n && mSorted[j].minX <= a.maxX; ++j)
            {
                ++counters.candidates;
                if (overlaps(a, mSorted[j], closed || mClosed[j] != 0))
                    sink(mIds[i], mIds[j]);
            }
        }
        return counters;
    }

private:
    struct Key
    {
        T minX;
        std::uint32_t id;
    };

    std::vector<Key> mKeys;
    std::vector<Box<T>> mSorted;
    std::vector<std::uint32_t> mIds;
    std::vector<std::uint8_t> mClosed;
};

// Uniform grid over the data bounds. Cell side: twice the mean extent, raised
// so the grid has at most about 4n cells. Each entry is referenced by every
// cell it covers; a pair is evaluated only in the cell holding
// (max(minX), max(minY)) of the two boxes, so it is reported once.
template<typename T>
class UniformGrid
{
public:
    static constexpr std::string_view name = "grid";

    void build(std::span<const Box<T>> boxes, Boundary boundary)
    {
        const std::size_t n = boxes.size();
        mBoxes.assign(boxes.begin(), boxes.end());
        mClosed.resize(n);
        mRange.resize(4 * n);
        mColumns = 0;
        mRows = 0;
        mStart.clear();
        mRefs.clear();
        if (n == 0)
            return;

        double lo_x = static_cast<double>(boxes[0].minX);
        double lo_y = static_cast<double>(boxes[0].minY);
        double hi_x = static_cast<double>(boxes[0].maxX);
        double hi_y = static_cast<double>(boxes[0].maxY);
        double extent = 0;
        for (const Box<T>& box : boxes)
        {
            lo_x = std::min(lo_x, static_cast<double>(box.minX));
            lo_y = std::min(lo_y, static_cast<double>(box.minY));
            hi_x = std::max(hi_x, static_cast<double>(box.maxX));
            hi_y = std::max(hi_y, static_cast<double>(box.maxY));
            extent += (static_cast<double>(box.maxX) - static_cast<double>(box.minX)) +
                (static_cast<double>(box.maxY) - static_cast<double>(box.minY));
        }

        const double span_x = hi_x - lo_x;
        const double span_y = hi_y - lo_y;
        const double cells = 4.0 * static_cast<double>(n);
        double cell = std::max({extent / static_cast<double>(n), std::sqrt(span_x * span_y / cells), span_x / cells,
            span_y / cells});
        if (!(cell > 0))
            cell = 1;

        mOriginX = lo_x;
        mOriginY = lo_y;
        mInverse = 1.0 / cell;
        mColumns = static_cast<std::uint32_t>(span_x * mInverse) + 1;
        mRows = static_cast<std::uint32_t>(span_y * mInverse) + 1;

        mStart.assign(std::size_t(mColumns) * mRows + 1, 0);
        for (std::size_t i = 0; i < n; ++i)
        {
            mClosed[i] = closedFlag(boxes[i], boundary);
            const std::uint32_t x0 = column(boxes[i].minX);
            const std::uint32_t y0 = row(boxes[i].minY);
            const std::uint32_t x1 = column(boxes[i].maxX);
            const std::uint32_t y1 = row(boxes[i].maxY);
            mRange[4 * i] = x0;
            mRange[4 * i + 1] = y0;
            mRange[4 * i + 2] = x1;
            mRange[4 * i + 3] = y1;
            for (std::uint32_t y = y0; y <= y1; ++y)
                for (std::uint32_t x = x0; x <= x1; ++x)
                    ++mStart[std::size_t(y) * mColumns + x + 1];
        }
        for (std::size_t c = 1; c < mStart.size(); ++c)
            mStart[c] += mStart[c - 1];

        mFill.assign(mStart.begin(), mStart.end() - 1);
        mRefs.resize(mStart.back());
        for (std::size_t i = 0; i < n; ++i)
            for (std::uint32_t y = mRange[4 * i + 1]; y <= mRange[4 * i + 3]; ++y)
                for (std::uint32_t x = mRange[4 * i]; x <= mRange[4 * i + 2]; ++x)
                    mRefs[mFill[std::size_t(y) * mColumns + x]++] = static_cast<std::uint32_t>(i);
    }

    template<typename Sink>
    Counters pairs(Sink& sink, std::uint64_t limit) const
    {
        Counters counters;
        counters.references = mRefs.size();
        const std::size_t cells = mStart.empty() ? 0 : mStart.size() - 1;
        for (std::size_t c = 0; c < cells; ++c)
        {
            if (counters.candidates > limit)
            {
                counters.capped = true;
                return counters;
            }
            const std::size_t begin = mStart[c];
            const std::size_t end = mStart[c + 1];
            for (std::size_t p = begin; p < end; ++p)
            {
                const std::uint32_t a = mRefs[p];
                for (std::size_t q = p + 1; q < end; ++q)
                {
                    const std::uint32_t b = mRefs[q];
                    const std::uint32_t x = std::max(mRange[4 * a], mRange[4 * b]);
                    const std::uint32_t y = std::max(mRange[4 * a + 1], mRange[4 * b + 1]);
                    if (std::size_t(y) * mColumns + x != c)
                        continue;
                    ++counters.candidates;
                    if (overlaps(mBoxes[a], mBoxes[b], mClosed[a] != 0 || mClosed[b] != 0))
                        sink(a, b);
                }
            }
        }
        return counters;
    }

    // Entries whose boxes overlap entry a's box, excluding a, each once (the
    // cell of max minX, max minY reports it).
    template<typename Callback>
    Counters overlapping(std::uint32_t a, Callback& callback) const
    {
        Counters counters;
        for (std::uint32_t y = mRange[4 * a + 1]; y <= mRange[4 * a + 3]; ++y)
            for (std::uint32_t x = mRange[4 * a]; x <= mRange[4 * a + 2]; ++x)
            {
                const std::size_t c = std::size_t(y) * mColumns + x;
                ++counters.nodeVisits;
                for (std::size_t p = mStart[c]; p < mStart[c + 1]; ++p)
                {
                    const std::uint32_t b = mRefs[p];
                    const std::uint32_t rx = std::max(mRange[4 * a], mRange[4 * b]);
                    const std::uint32_t ry = std::max(mRange[4 * a + 1], mRange[4 * b + 1]);
                    if (b == a || std::size_t(ry) * mColumns + rx != c)
                        continue;
                    ++counters.candidates;
                    if (overlaps(mBoxes[a], mBoxes[b], mClosed[a] != 0 || mClosed[b] != 0))
                        callback(b);
                }
            }
        return counters;
    }

private:
    std::vector<Box<T>> mBoxes;
    std::vector<std::uint8_t> mClosed;
    std::vector<std::uint32_t> mRange; // x0, y0, x1, y1 per entry
    std::vector<std::size_t> mStart;   // Cell offsets into mRefs, cells + 1
    std::vector<std::size_t> mFill;
    std::vector<std::uint32_t> mRefs;
    double mOriginX = 0;
    double mOriginY = 0;
    double mInverse = 1;
    std::uint32_t mColumns = 0;
    std::uint32_t mRows = 0;

    // Monotonic, so cell ranges and the reporting cell agree.
    std::uint32_t column(T x) const noexcept
    {
        return std::min(mColumns - 1, static_cast<std::uint32_t>((static_cast<double>(x) - mOriginX) * mInverse));
    }

    std::uint32_t row(T y) const noexcept
    {
        return std::min(mRows - 1, static_cast<std::uint32_t>((static_cast<double>(y) - mOriginY) * mInverse));
    }
};
} // namespace bench
