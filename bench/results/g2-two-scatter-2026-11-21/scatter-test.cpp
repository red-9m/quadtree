// Untimed root scatter probe, including all five tags and uneven tails.
#include <quadtree-index.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <vector>

namespace
{
using namespace QuadTree;
using T = std::int32_t;
using I = Index<int, T>;
using Key = detail::SpatialKey;
constexpr auto sentinel = std::numeric_limits<std::uint32_t>::max();

void check(bool ok)
{
    if (!ok) std::abort();
}

// Assign lower and upper halves different (sometimes disjoint) tag
// histograms. A parent-held box spans more than the child fit width.
void test(std::uint32_t length, unsigned pattern)
{
    constexpr std::uint32_t begin = 3;
    const auto end = begin + length;
    constexpr detail::Cell<T> root{{-128, -128, 128, 128}};
    auto made = I::create({.extent = Extent::Fixed, .root = root.box, .splitThreshold = 3,
        .mergeThreshold = 1, .maxDepth = 1});
    check(made.ok());
    std::vector<Box<T>> boxes(length);
    std::vector<unsigned> tagsForID(length);
    std::vector<std::uint32_t> order(end + 2, sentinel), scratch(order.size(), sentinel);
    std::vector<std::uint8_t> tags(order.size(), 0xff);
    std::iota(order.begin() + begin, order.begin() + end, 0u);
    std::reverse(order.begin() + begin, order.begin() + end);
    for (std::uint32_t j = 0; j < length; ++j)
    {
        const auto id = order[begin + j];
        const auto tag = pattern == 0 ? j % 5 : pattern == 1 ?
            (j < length / 2 ? j % 2 : 2 + j % 3) :
            (j + 1 == length ? 0u : 1u + j % 4);
        tagsForID[id] = tag;
        const auto q = tag == 0 ? 0u : tag - 1;
        const T x = (q & 1u) != 0 ? 16 : -24, y = (q & 2u) != 0 ? 32 : -16;
        boxes[id] = tag == 0 ? Box<T>{-120, -120, 120, 120} : Box<T>{x, y, x + 4, y + 4};
    }
    auto expected = order;
    std::stable_sort(expected.begin() + begin, expected.begin() + end,
        [&](std::uint32_t a, std::uint32_t b) { return tagsForID[a] < tagsForID[b]; });
    for (bool startInOrder : {false, true})
    {
        auto src = order;
        std::fill(scratch.begin(), scratch.end(), sentinel);
        std::fill(tags.begin(), tags.end(), 0xff);
        auto* final = startInOrder ? src.data() : scratch.data();
        std::array<Key, 2> keys{};
        std::size_t plans = 0;
        bool overflow = false;
        auto& index = made.value();
        check(index.planBulk(root, 0, begin, end, boxes.data(), src.data(), scratch.data(), final,
            keys.data(), tags.data(), keys.size(), plans, overflow));
        check(plans == 1 && !overflow && keys[0].level == 0);
        check(std::equal(expected.begin(), expected.end(), final));
        const auto edge = [&](std::size_t j)
        {
            return j == 0 ? static_cast<std::uint32_t>(keys[0].hi >> 32) :
                j == 1 ? static_cast<std::uint32_t>(keys[0].hi) :
                j == 2 ? static_cast<std::uint32_t>(keys[0].lo >> 32) : static_cast<std::uint32_t>(keys[0].lo);
        };
        for (std::uint32_t q = 0; q < 4; ++q)
            check(edge(q) == begin + static_cast<std::uint32_t>(std::count_if(tagsForID.begin(), tagsForID.end(),
                [&](unsigned tag) { return tag <= q; })));
        check(tags.front() == 0xff && tags.back() == 0xff);
    }
}
} // namespace

int main()
{
    for (const auto n: {31u, 32u, 33u, 63u, 64u, 65u})
        for (unsigned pattern = 0; pattern != 3; ++pattern)
            test(n, pattern);
    std::puts("36 exact scatter cases: five tags, disjoint half histograms, own-entry tails, both output parities.");
}
