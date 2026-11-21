// Build against the staged control with -fno-access-control (probe only).
#include "quadtree-index.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <set>

template<typename T>
void size(const char* coordinate)
{
    using Node = typename QuadTree::Index<unsigned, T>::Node;
    std::printf("%s sizeof=%zu alignof=%zu\n", coordinate, sizeof(Node), alignof(Node));
}

int main()
{
    size<std::int32_t>("int32");
    size<std::int64_t>("int64");
    size<float>("float");
    size<double>("double");
    using Node = QuadTree::Index<unsigned, std::int32_t>::Node;
    constexpr std::size_t line = 64;
    static_assert(sizeof(Node::bounds) == 16);
    constexpr auto bounds = offsetof(Node, bounds);
    constexpr auto child = offsetof(Node, firstChild);
    constexpr auto count = offsetof(Node, count);
    std::printf("sizeof=%zu alignof=%zu bounds=%zu..%zu firstChild=%zu..%zu count=%zu..%zu\n",
        sizeof(Node), alignof(Node), bounds, bounds + sizeof(Node::bounds) - 1,
        child, child + sizeof(Node::firstChild) - 1, count, count + sizeof(Node::count) - 1);
    std::printf("stride=%zu, node-start residue step=%zu, hardware line=%zu\n",
        sizeof(Node), std::gcd(sizeof(Node), line), line);
    constexpr std::array edges{bounds, bounds + 4, bounds + 8, bounds + 12};
    std::puts("start_mod64,count_line,bounds_first_line,bounds_last_line,firstChild_line,three_field_lines,four_sibling_count_bounds_lines,packed_three_field_lines,packed_four_sibling_count_bounds_lines");
    for (std::size_t start = 0; start < line; start += alignof(Node))
    {
        std::set<std::size_t> node, siblings, packedNode, packedSiblings;
        node.insert((start + count) / line);
        node.insert((start + child) / line);
        // Hypothetical contiguous bounds/count/firstChild at offsets 0/16/20.
        packedNode.insert((start + 16) / line);
        packedNode.insert((start + 20) / line);
        for (const auto e: edges)
            node.insert((start + e) / line);
        for (std::size_t e = 0; e < sizeof(Node::bounds); e += 4)
            packedNode.insert((start + e) / line);
        for (std::size_t i = 0; i < 4; ++i)
        {
            const auto base = start + i * sizeof(Node);
            siblings.insert((base + count) / line);
            packedSiblings.insert((base + 16) / line);
            for (const auto e: edges)
            {
                siblings.insert((base + e) / line);
                packedSiblings.insert((base + e - bounds) / line);
            }
        }
        std::printf("%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu\n", start, (start + count) / line,
            (start + bounds) / line, (start + bounds + sizeof(Node::bounds) - 1) / line,
            (start + child) / line, node.size(), siblings.size(), packedNode.size(), packedSiblings.size());
    }
}
