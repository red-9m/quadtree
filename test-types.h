#pragma once

#include <cstddef>

#include "quadtree.h"

// Foreign rectangle type used by the demo's reference query.
template<typename RectT>
struct RectLocal
{
    RectT x1, y1, w1, h1;
};

template<typename RectT>
constexpr RectLocal<RectT> toLocal(QuadTree::Rect<RectT> rect) noexcept
{
    return {rect.x, rect.y, rect.w, rect.h};
}

template<typename RectT>
struct Item
{
    QuadTree::Rect<RectT> rect{};
    std::size_t id{};
};
