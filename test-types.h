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
    RectLocal<RectT> rect;
    std::size_t id;
};
