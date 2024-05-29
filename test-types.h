#pragma once

#include <cstddef>

template<typename RectT>
struct RectLocal
{
    RectT x1, y1, w1, h1;
};

template<typename RectT>
struct Item
{
    Item() = default;
    Item(const Item<RectT>&) = default;
    Item(const RectLocal<RectT> aRect, std::size_t aId): rect(std::move(aRect)), id(aId) {}
    Item(Item<RectT>&&) = default;

    RectLocal<RectT> rect;
    std::size_t id;
};
