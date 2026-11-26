#pragma once

#include <cstddef>
#include <iostream>
#include <string>
#include "quadtree.h"

template<typename RectT>
const std::string debugPrintRect(const QuadTree::Rect<RectT>& rect)
{
    std::string result;
    result.append("[");
    result.append(std::to_string(rect.x));
    result.append(",");
    result.append(std::to_string(rect.y));
    result.append(",");
    result.append(std::to_string(rect.w));
    result.append(",");
    result.append(std::to_string(rect.h));
    result.append("]");

    return result;
}

template<typename ItemT, typename RectT>
void debugPrintTree(QuadTree::Tree<ItemT, RectT>& tree)
{
    auto printEnum = [](int type, int level, int quadrant, const QuadTree::Rect<RectT>& rect, bool hasChildren, const ItemT* item)
    {
        if (type == 2)
        {
            std::cout << std::endl;
        } else
        {
            std::string offset;
            offset.insert(0, static_cast<std::size_t>(level) * 4, ' ');
            if (type == 0)
                std::cout << offset << "Node" << quadrant << " (" << hasChildren << ") " << debugPrintRect(rect) << " items: ";
            else
                std::cout << debugPrintRect((*item)->rect) << " ";
        }
    };

    tree.enumerateNodes(printEnum);
}
