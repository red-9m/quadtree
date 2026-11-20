#pragma once

// Benchmark scene fixtures (see bench/MANIFEST.md). Generation uses only integer
// arithmetic on splitmix64 output, in a fixed draw order, so every platform and
// standard library produces identical fixtures; scene hashes verify this.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "quadtree-index.h"

namespace bench
{
// Scene geometry in integer world units.
struct IBox
{
    std::int64_t minX, minY, maxX, maxY;
};

constexpr std::uint64_t mix64(std::uint64_t value) noexcept
{
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9u;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebu;
    value ^= value >> 31;
    return value;
}

// splitmix64
class Rng
{
public:
    explicit Rng(std::uint64_t seed) noexcept: mState(seed) {}

    std::uint64_t next() noexcept
    {
        mState += 0x9e3779b97f4a7c15u;
        return mix64(mState);
    }

    // Uniform integer in [lo, hi]: lo + next() % (hi - lo + 1).
    std::int64_t uniform(std::int64_t lo, std::int64_t hi) noexcept
    {
        const auto span = static_cast<std::uint64_t>(hi - lo) + 1;
        return lo + static_cast<std::int64_t>(next() % span);
    }

private:
    std::uint64_t mState;
};

enum class Boundary
{
    Default, // Positive-area boxes need positive-area intersection (QT-FR-04)
    Touch,   // Closed comparisons for all boxes
};

struct SceneInfo
{
    std::string_view name;
    Boundary boundary;
    std::size_t maxEntries; // Larger sizes are skipped and reported as capped
    bool smallSizes;        // Also run the batched 0-64 entry sizes
    std::string_view description;
};

inline constexpr std::size_t kUncapped = ~std::size_t(0);

inline constexpr SceneInfo kScenes[] = {
    {"uniform", Boundary::Default, kUncapped, true, "small boxes, uniform"},
    {"clustered", Boundary::Default, kUncapped, true, "small boxes in clusters of ~64"},
    {"split-line", Boundary::Default, kUncapped, false, "half the boxes straddle dyadic root lines"},
    {"split-line-translated", Boundary::Default, kUncapped, false, "split-line content shifted off dyadic lines"},
    {"mixed-size", Boundary::Default, kUncapped, false, "98% small, 1.5% medium, 0.5% large"},
    {"long-thin", Boundary::Default, kUncapped, false, "thin rows and columns"},
    {"tiles", Boundary::Default, kUncapped, false, "edge-adjacent lattice, no overlaps"},
    {"tiles-touch", Boundary::Touch, kUncapped, false, "edge-adjacent lattice, touch-inclusive"},
    {"points", Boundary::Default, kUncapped, false, "points and segments on a lattice"},
    {"pile", Boundary::Default, 4096, false, "all boxes overlap"},
    {"disjoint", Boundary::Default, kUncapped, false, "separated lattice, zero pairs"},
    {"outliers", Boundary::Default, kUncapped, false, "uniform plus 1% far outliers"},
};

inline const SceneInfo* findScene(std::string_view name) noexcept
{
    for (const auto& scene : kScenes)
        if (scene.name == name)
            return &scene;
    return nullptr;
}

// Smallest r with r * r >= n.
constexpr std::int64_t ceilSqrt(std::uint64_t n) noexcept
{
    std::uint64_t lo = 0;
    std::uint64_t hi = 1u << 16;
    while (hi * hi < n)
        hi *= 2;
    while (lo < hi)
    {
        const std::uint64_t mid = lo + (hi - lo) / 2;
        if (mid * mid >= n)
            hi = mid;
        else
            lo = mid + 1;
    }
    return static_cast<std::int64_t>(lo);
}

// World side keeping about 1024 square units per entry.
constexpr std::int64_t side(std::size_t n) noexcept
{
    return 32 * ceilSqrt(std::max<std::uint64_t>(n, 1));
}

constexpr std::uint64_t fnv1a(std::string_view text) noexcept
{
    std::uint64_t hash = 0xcbf29ce484222325u;
    for (const char c : text)
        hash = (hash ^ static_cast<unsigned char>(c)) * 0x100000001b3u;
    return hash;
}

// Seed of batch member k of a scene at size n. Variants share their base
// scene's seed, so they differ only by translation or boundary policy.
inline std::uint64_t sceneSeed(std::uint64_t base, std::string_view scene, std::size_t n, std::uint64_t k) noexcept
{
    if (scene == "split-line-translated")
        scene = "split-line";
    else if (scene == "tiles-touch")
        scene = "tiles";
    return Rng(base ^ fnv1a(scene) ^ (std::uint64_t(n) << 20) ^ k).next();
}

// Order-dependent hash of the fixture geometry.
inline std::uint64_t sceneHash(std::span<const IBox> boxes) noexcept
{
    std::uint64_t hash = boxes.size();
    for (const IBox& box : boxes)
        for (const std::int64_t value : {box.minX, box.minY, box.maxX, box.maxY})
            hash = mix64(hash ^ static_cast<std::uint64_t>(value));
    return hash;
}

namespace detail
{
inline IBox make(std::int64_t x, std::int64_t y, std::int64_t w, std::int64_t h) noexcept
{
    return IBox{x, y, x + w, y + h};
}

// Sum of four uniform draws in [-64, 64], halved (truncation toward zero).
inline std::int64_t clusterOffset(Rng& rng) noexcept
{
    std::int64_t sum = rng.uniform(-64, 64);
    sum += rng.uniform(-64, 64);
    sum += rng.uniform(-64, 64);
    sum += rng.uniform(-64, 64);
    return sum / 2;
}

// Root side of the split-line scenes: smallest power of two >= max(64, 2 side(n)).
inline std::int64_t splitLineRoot(std::size_t n) noexcept
{
    std::int64_t w = 64;
    while (w < 2 * side(n))
        w *= 2;
    return w;
}

inline std::vector<IBox> splitLine(std::size_t n, std::uint64_t seed, bool translated)
{
    const std::int64_t w = splitLineRoot(n);
    const std::int64_t h = w / 2; // Content square [0, h]; root [0, w]

    int max_level = 1;
    while (max_level < 6 && h >> (max_level + 1) >= 32)
        ++max_level;

    std::vector<IBox> boxes;
    boxes.reserve(n);
    if (n > 0)
        boxes.push_back(IBox{0, 0, 0, 0}); // Anchors pin data bounds to the root
    if (n > 1)
        boxes.push_back(IBox{w, w, w, w});

    Rng rng(seed);
    for (std::size_t i = boxes.size(); i < n; ++i)
    {
        const std::int64_t bw = rng.uniform(8, 32);
        const std::int64_t bh = rng.uniform(8, 32);
        std::int64_t x = 0;
        std::int64_t y = 0;
        if (rng.uniform(0, 1) == 0)
        {
            x = rng.uniform(0, h - bw);
            y = rng.uniform(0, h - bh);
        }
        else
        {
            const std::int64_t level = rng.uniform(1, max_level);
            const std::int64_t k = 2 * rng.uniform(0, (std::int64_t(1) << (level - 1)) - 1) + 1;
            const std::int64_t line = k * (h >> level);
            if (rng.uniform(0, 1) == 0)
            {
                x = line - rng.uniform(1, bw - 1);
                y = rng.uniform(0, h - bh);
            }
            else
            {
                y = line - rng.uniform(1, bh - 1);
                x = rng.uniform(0, h - bw);
            }
        }
        boxes.push_back(make(x, y, bw, bh));
    }

    if (translated)
        for (std::size_t i = 2; i < boxes.size(); ++i)
        {
            boxes[i].minX += h / 2 + 5;
            boxes[i].maxX += h / 2 + 5;
            boxes[i].minY += h / 2 + 3;
            boxes[i].maxY += h / 2 + 3;
        }
    return boxes;
}
} // namespace detail

// Generates scene `name` with n entries. Draw order is part of the fixture.
inline std::vector<IBox> generate(std::string_view name, std::size_t n, std::uint64_t seed)
{
    using detail::make;
    std::vector<IBox> boxes;
    boxes.reserve(n);
    Rng rng(seed);
    const std::int64_t w = side(n);

    if (name == "uniform")
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            const std::int64_t bw = rng.uniform(8, 32);
            const std::int64_t bh = rng.uniform(8, 32);
            const std::int64_t x = rng.uniform(0, w - bw);
            const std::int64_t y = rng.uniform(0, w - bh);
            boxes.push_back(make(x, y, bw, bh));
        }
    }
    else if (name == "clustered")
    {
        const std::size_t clusters = std::max<std::size_t>(1, n / 64);
        std::vector<IBox> centers;
        for (std::size_t k = 0; k < clusters; ++k)
        {
            const std::int64_t cx = rng.uniform(0, w);
            const std::int64_t cy = rng.uniform(0, w);
            centers.push_back(IBox{cx, cy, cx, cy});
        }
        for (std::size_t i = 0; i < n; ++i)
        {
            const IBox& center = centers[static_cast<std::size_t>(rng.uniform(0, std::int64_t(clusters) - 1))];
            const std::int64_t bw = rng.uniform(8, 32);
            const std::int64_t bh = rng.uniform(8, 32);
            const std::int64_t dx = detail::clusterOffset(rng);
            const std::int64_t dy = detail::clusterOffset(rng);
            const std::int64_t x = std::clamp(center.minX + dx - bw / 2, std::int64_t(0), w - bw);
            const std::int64_t y = std::clamp(center.minY + dy - bh / 2, std::int64_t(0), w - bh);
            boxes.push_back(make(x, y, bw, bh));
        }
    }
    else if (name == "split-line" || name == "split-line-translated")
    {
        // Both variants share the split-line seed: identical content.
        return detail::splitLine(n, seed, name == "split-line-translated");
    }
    else if (name == "mixed-size")
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            const std::int64_t r = rng.uniform(0, 999);
            std::int64_t bw = 0;
            std::int64_t bh = 0;
            if (r < 5)
            {
                bw = rng.uniform(w / 8, w / 4);
                bh = rng.uniform(w / 8, w / 4);
            }
            else if (r < 20)
            {
                const std::int64_t hi = std::max<std::int64_t>(128, std::min<std::int64_t>(512, w));
                bw = rng.uniform(128, hi);
                bh = rng.uniform(128, hi);
            }
            else
            {
                bw = rng.uniform(8, 32);
                bh = rng.uniform(8, 32);
            }
            bw = std::min(bw, w);
            bh = std::min(bh, w);
            const std::int64_t x = rng.uniform(0, w - bw);
            const std::int64_t y = rng.uniform(0, w - bh);
            boxes.push_back(make(x, y, bw, bh));
        }
    }
    else if (name == "long-thin")
    {
        const std::int64_t world = 2 * w;
        for (std::size_t i = 0; i < n; ++i)
        {
            const std::int64_t thin = rng.uniform(2, 8);
            const std::int64_t length = rng.uniform(32, std::min<std::int64_t>(256, world));
            const bool horizontal = rng.uniform(0, 1) == 0;
            const std::int64_t bw = horizontal ? length : thin;
            const std::int64_t bh = horizontal ? thin : length;
            const std::int64_t x = rng.uniform(0, world - bw);
            const std::int64_t y = rng.uniform(0, world - bh);
            boxes.push_back(make(x, y, bw, bh));
        }
    }
    else if (name == "tiles" || name == "tiles-touch")
    {
        const auto columns = static_cast<std::size_t>(ceilSqrt(n));
        for (std::size_t i = 0; i < n; ++i)
            boxes.push_back(make(16 * std::int64_t(i % columns), 16 * std::int64_t(i / columns), 16, 16));
    }
    else if (name == "points")
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            const std::int64_t kind = rng.uniform(0, 3);
            const std::int64_t x = 8 * rng.uniform(0, w / 8);
            const std::int64_t y = 8 * rng.uniform(0, w / 8);
            if (kind < 2)
                boxes.push_back(IBox{x, y, x, y});
            else if (kind == 2)
                boxes.push_back(IBox{x, y, std::min(x + rng.uniform(8, 64), w), y});
            else
                boxes.push_back(IBox{x, y, x, std::min(y + rng.uniform(8, 64), w)});
        }
    }
    else if (name == "pile")
    {
        constexpr std::int64_t center = 512;
        for (std::size_t i = 0; i < n; ++i)
        {
            const std::int64_t left = rng.uniform(1, 400);
            const std::int64_t right = rng.uniform(1, 400);
            const std::int64_t bottom = rng.uniform(1, 400);
            const std::int64_t top = rng.uniform(1, 400);
            boxes.push_back(IBox{center - left, center - bottom, center + right, center + top});
        }
    }
    else if (name == "disjoint")
    {
        const auto columns = static_cast<std::size_t>(ceilSqrt(n));
        for (std::size_t i = 0; i < n; ++i)
        {
            const std::int64_t x = 32 * std::int64_t(i % columns) + rng.uniform(0, 15);
            const std::int64_t y = 32 * std::int64_t(i / columns) + rng.uniform(0, 15);
            const std::int64_t bw = rng.uniform(1, 16);
            const std::int64_t bh = rng.uniform(1, 16);
            boxes.push_back(make(x, y, bw, bh));
        }
    }
    else if (name == "outliers")
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            const std::int64_t bw = rng.uniform(8, 32);
            const std::int64_t bh = rng.uniform(8, 32);
            std::int64_t x = rng.uniform(0, w - bw);
            std::int64_t y = rng.uniform(0, w - bh);
            if (rng.uniform(0, 99) == 0)
            {
                const std::int64_t sx = rng.uniform(0, 1) == 0 ? -1 : 1;
                x = sx * rng.uniform(std::int64_t(1) << 28, std::int64_t(1) << 29);
                const std::int64_t sy = rng.uniform(0, 1) == 0 ? -1 : 1;
                y = sy * rng.uniform(std::int64_t(1) << 28, std::int64_t(1) << 29);
            }
            boxes.push_back(make(x, y, bw, bh));
        }
    }
    return boxes;
}

// Declared world of a scene; persistent motion, teleports and spawns stay inside.
inline IBox sceneWorld(std::string_view name, std::size_t n) noexcept
{
    if (name == "split-line" || name == "split-line-translated")
    {
        const std::int64_t w = detail::splitLineRoot(n);
        return IBox{0, 0, w, w};
    }
    if (name == "long-thin")
        return IBox{0, 0, 2 * side(n), 2 * side(n)};
    return IBox{0, 0, side(n), side(n)};
}

template<QuadTree::Coordinate T>
QuadTree::Box<T> toBox(const IBox& box) noexcept
{
    return QuadTree::Box<T>{static_cast<T>(box.minX), static_cast<T>(box.minY), static_cast<T>(box.maxX),
        static_cast<T>(box.maxY)};
}

// Integer to coordinate conversion: exact below 2^24 (float) and 2^53
// (double); larger values round to nearest under the QT-FR-02 environment.
template<QuadTree::Coordinate T>
std::vector<QuadTree::Box<T>> convert(std::span<const IBox> boxes)
{
    std::vector<QuadTree::Box<T>> result;
    result.reserve(boxes.size());
    for (const IBox& box : boxes)
        result.push_back(toBox<T>(box));
    return result;
}
} // namespace bench
