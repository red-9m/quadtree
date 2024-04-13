#include <memory>
#include <iostream>
#include "quadtree-debug.h"

template<typename RectT>
const std::string debugPrintRect(const RectLocal<RectT>& rect)
{
    std::string result;
    result.append("[");
    result.append(std::to_string(rect.x1));
    result.append(",");
    result.append(std::to_string(rect.y1));
    result.append(",");
    result.append(std::to_string(rect.w1));
    result.append(",");
    result.append(std::to_string(rect.h1));
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
            offset.insert(0, level * 4, ' ');
            if (type == 0)
                std::cout << offset << "Node" << quadrant << " (" << hasChildren << ") " << debugPrintRect(std::bit_cast<RectLocal<RectT>>(rect)) << " items: ";
            else
                std::cout << debugPrintRect((*item)->rect) << " ";
        }
    };

    tree.enumerateNodes(printEnum);
}

// A hack to keep implementation in .cpp file
template const std::string debugPrintRect<int>(const RectLocal<int>& rect);
template const std::string debugPrintRect<float>(const RectLocal<float>& rect);
template void debugPrintTree<Item<int>*, int>(QuadTree::Tree<Item<int>*, int>& tree);
template void debugPrintTree<std::shared_ptr<Item<float>>, float>(QuadTree::Tree<std::shared_ptr<Item<float>>, float>& tree);
