#pragma once

struct RectLocal
{
    int x1, y1, w1, h1;
};

struct Item
{
    RectLocal rect;
    std::size_t id;
};
