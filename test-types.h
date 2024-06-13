#pragma once

#include <cstddef>

#include "quadtree.h"

// This structure must be castable to QuadTree::Rect by reinterpret_cast
// Or you need to use QuadTree::Rect directly in item
template<typename RectT>
struct RectLocal
{
    RectT x1, y1, w1, h1;
};

template<typename RectT>
union RectUnion {
    QuadTree::Rect<RectT> quadRect;
    RectLocal<RectT> localRect;
};

template<typename RectT>
struct Item
{
    Item() = default;
    Item(const Item<RectT>&) = default;
    Item(const RectUnion<RectT> aRect, std::size_t aId): rect(std::move(aRect)), id(aId) {}
    Item(Item<RectT>&&) = default;

    RectUnion<RectT> rect;
    std::size_t id;
};
