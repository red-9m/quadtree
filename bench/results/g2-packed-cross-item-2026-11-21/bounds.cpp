// Untimed: compare packed and portable exact overlap without subtracting coordinates.
#include <quadtree-index.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace
{
using T = std::int32_t;
using QuadTree::Box;
using I = QuadTree::Index<unsigned, T, QuadTree::Checked>;

void check(const Box<T>& a, const Box<T>& b)
{
    for (auto mode : {QuadTree::Boundary::Default, QuadTree::Boundary::TouchInclusive})
    {
        const bool expected = QuadTree::detail::overlaps(a, b, mode);
        if (I::packedCrossItemOverlap(a, b, mode) != expected || I::packedCrossItemOverlap(b, a, mode) != expected)
            std::abort();
    }
}

std::uint32_t next(std::uint32_t& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

Box<T> randomBox(std::uint32_t& s)
{
    const T x = std::bit_cast<T>(next(s)), y = std::bit_cast<T>(next(s));
    const T u = std::bit_cast<T>(next(s)), v = std::bit_cast<T>(next(s));
    return {std::min(x, u), std::min(y, v), std::max(x, u), std::max(y, v)};
}
} // namespace

int main()
{
    const std::array<T, 7> coords{std::numeric_limits<T>::min(), std::numeric_limits<T>::min() + 1,
        -1, 0, 1, std::numeric_limits<T>::max() - 1, std::numeric_limits<T>::max()};
    std::vector<Box<T>> boxes;
    for (auto x : coords)
        for (auto y : coords)
            for (auto u : coords)
                for (auto v : coords)
                    if (x <= u && y <= v)
                        boxes.push_back({x, y, u, v});
    for (const auto& a : boxes)
        for (const auto& b : boxes)
            check(a, b);
    std::uint32_t state = 12345;
    for (unsigned i = 0; i < 100000; ++i)
    {
        const auto a = randomBox(state);
        const auto b = randomBox(state);
        check(a, b);
    }
    std::printf("%zu exhaustive signed-extreme box pairs plus 100000 generated pairs match scalar exact bounds in both orders and modes.\n",
        boxes.size() * boxes.size());
}
