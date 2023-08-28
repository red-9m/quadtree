#include <memory>
#include <iostream>
#include "quadtree-debug.h"

const std::string debugPrintRect(const RectLocal* const rect)
{
    std::string result;
    result.append("[");
    result.append(std::to_string(rect->x1));
    result.append(",");
    result.append(std::to_string(rect->y1));
    result.append(",");
    result.append(std::to_string(rect->w1));
    result.append(",");
    result.append(std::to_string(rect->h1));
    result.append("]");

    return result;
}

template<typename ItemT>
void debugPrintTree(QuadTree::Tree<ItemT>& tree)
{
    auto printEnum = [](int type, int level, int quadrant, const QuadTree::Rect& rect, bool hasChildren, const ItemT* item)
    {
        if (type == 2)
        {
            std::cout << std::endl;
        } else
        {
            std::string offset;
            offset.insert(0, level * 4, ' ');
            if (type == 0)
                std::cout << offset << "Node" << quadrant << " (" << hasChildren << ") " << debugPrintRect((RectLocal*)&rect) << " items: ";
            else
                std::cout << debugPrintRect(&(*item)->rect) << " ";
        }
    };

    std::cout << debugDelim << std::endl;
    tree.enumerateNodes(printEnum);
}

// A hack to keep implementation in .cpp file
template void debugPrintTree<Item*>(QuadTree::Tree<Item*>& tree);
template void debugPrintTree<std::shared_ptr<Item>>(QuadTree::Tree<std::shared_ptr<Item>>& tree);
