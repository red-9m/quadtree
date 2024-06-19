#include <chrono>
#include <iostream>
#include <vector>
#include <random>
#include <memory>
#include <functional>

#include "test-types.h"
#include "quadtree.h"

#ifdef _DEBUG
#include "quadtree-debug.h"
#endif

constexpr auto testDelim = "-------------------------------------------------------------";

template<typename RectT>
bool altRectIntersects(const RectLocal<RectT>* rect1, const RectLocal<RectT>* rect2)
{
    return !(
        rect1->w1 == 0 || rect1->h1 == 0 || rect2->w1 == 0 || rect2->h1 == 0 ||
        rect1->x1 >= rect2->x1 + rect2->w1 || rect1->x1 + rect1->w1 <= rect2->x1 ||
        rect1->y1 >= rect2->y1 + rect2->h1 || rect1->y1 + rect1->h1 <= rect2->y1);
}

template<typename RectT>
std::vector<Item<RectT>> generateRandomNodes(std::size_t n)
{
    auto generator = std::default_random_engine(12345u);
    auto originDistribution = std::uniform_real_distribution(0.1f, 1.0f);
    auto sizeDistribution = std::uniform_real_distribution(0.1f, 0.14f);
    auto items = std::vector<Item<RectT>>(n);
    for (auto i = std::size_t(0); i < n; ++i)
    {
        items[i].rect.localRect.x1 = static_cast<RectT>(100 * originDistribution(generator));
        items[i].rect.localRect.y1 =  static_cast<RectT>(100 * originDistribution(generator));
        items[i].rect.localRect.w1 =  static_cast<RectT>(std::min(100.0f - items[i].rect.localRect.x1, 100.0f * sizeDistribution(generator)));
        items[i].rect.localRect.h1 =  static_cast<RectT>(std::min(100.0f - items[i].rect.localRect.y1, 100.0f * sizeDistribution(generator)));
        items[i].id = i;
    }

    return items;
}

template<typename ItemT, typename RectT>
void quadTree(std::function<ItemT(Item<RectT>&)> addFunc, const std::string& runName)
{
    auto getRect = [](const ItemT& item) -> const QuadTree::Rect<RectT>&
    {
        return item->rect.quadRect;
    };

#ifdef _DEBUG
    constexpr std::size_t node_items = 4;
    constexpr std::size_t depth = 2;
    constexpr std::size_t gen_nodes = 32;
#else
    constexpr std::size_t node_items = 250;
    constexpr std::size_t depth = 3;
    constexpr std::size_t gen_nodes = 5000;
#endif

    auto tree = QuadTree::Tree<ItemT, RectT>({0, 0, 100, 100}, getRect, node_items, depth);
    auto items = generateRandomNodes<RectT>(gen_nodes);
    std::vector<ItemT> query_res;
    query_res.reserve(gen_nodes / 5);

    auto start = std::chrono::steady_clock::now();
    for (auto& item : items)
        tree.add(addFunc(item));
    auto duration1 = std::chrono::steady_clock::now() - start;

#ifdef _DEBUG
    std::cout << testDelim << std::endl;
    debugPrintTree<ItemT, RectT>(tree);
#endif

    QuadTree::Rect<RectT> query_rect{11, 10, 28, 29};
    start = std::chrono::steady_clock::now();
    tree.query(query_rect, query_res);
    auto duration2 = std::chrono::steady_clock::now() - start;

    start = std::chrono::steady_clock::now();
    std::vector<std::pair<ItemT, ItemT>> query_all_res;
    query_all_res.reserve(gen_nodes * 200);
    tree.queryAll(query_all_res);
    auto duration3 = std::chrono::steady_clock::now() - start;

    std::cout << testDelim << std::endl;
    std::cout << "Test                   : " << runName << std::endl;
    std::cout << "Total items            : " << gen_nodes << std::endl;
    std::cout << "QuadTree add time      : " << std::chrono::duration_cast<std::chrono::microseconds>(duration1).count() << "(µs)" << std::endl;
    std::cout << "QuadTree query time    : " << std::chrono::duration_cast<std::chrono::microseconds>(duration2).count() << "(µs)" << std::endl;
    std::cout << "QuadTree queryAll time : " << std::chrono::duration_cast<std::chrono::microseconds>(duration3).count() << "(µs)" << std::endl;
    std::cout << "QuadTree query items   : " << query_res.size() << std::endl;
    std::cout << "QuadTree queryAll items: " << query_all_res.size() << std::endl;

    query_res.clear();
    start = std::chrono::steady_clock::now();
    for (auto& item : items)
        if (altRectIntersects(&item.rect.localRect, (RectLocal<RectT>*)&query_rect))
            query_res.push_back(addFunc(item));
    auto duration4 = std::chrono::steady_clock::now() - start;
    std::cout << "Alt. query time        : " << std::chrono::duration_cast<std::chrono::microseconds>(duration4).count() << "(µs)" << std::endl;
    std::cout << "Alt. query items       : " << query_res.size() << std::endl;
}

void quadTreeSimple(const std::string& runName)
{
    typedef Item<int>* TreeItemT;
    auto getRect = [](const TreeItemT& item) -> const QuadTree::Rect<int>&
    {
        return item->rect.quadRect;
    };

    constexpr std::size_t node_items = 1;
    constexpr std::size_t depth = 1;

    auto tree = QuadTree::Tree<TreeItemT, int>({0, 0, 100, 100}, getRect, node_items, depth);
    std::vector<Item<int>> items{
        {{{10, 10, 3, 3}}, 1},
        {{{60, 10, 3, 3}}, 2},
        {{{10, 60, 3, 3}}, 3},
        {{{60, 60, 3, 3}}, 4},
        {{{11, 11, 50, 50}}, 5},
        {{{11, 11, 50, 3}}, 6},
        {{{11, 11, 3, 50}}, 7},
    };

    for (auto& item : items)
        tree.add(&item);

#ifdef _DEBUG
    std::cout << testDelim << std::endl;
    debugPrintTree<TreeItemT, int>(tree);
#endif

    std::string str_res;
    std::vector<std::pair<TreeItemT, TreeItemT>> query_all_res;
    query_all_res.reserve(10);
    tree.queryAll(query_all_res);
    for (const auto& pair : query_all_res)
    {
        str_res.append("|");
        str_res.append(std::to_string(pair.first->id));
        str_res.append("x");
        str_res.append(std::to_string(pair.second->id));
        str_res.append("|");
    }

    tree.remove(&items[1]);
    tree.remove(&items[2]);

#ifdef _DEBUG
    std::cout << testDelim << std::endl;
    debugPrintTree<TreeItemT, int>(tree);
#endif

    std::vector<TreeItemT> query_res;
    query_res.reserve(2);
    tree.query({11, 11, 3, 3}, query_res);

    std::cout << testDelim << std::endl;
    std::cout << "Test                   : " << runName << std::endl;
    std::cout << "QuadTree query items   : " << query_res.size() << std::endl;
    std::cout << "QuadTree queryAll items: " << query_all_res.size() << " : " << str_res << std::endl;
    std::cout << testDelim << std::endl;
}

int main()
{
    const auto addFuncPtr = [](Item<int>& item)
    {
        return &item;
    };

    const auto addFuncSmartPtr = [](Item<float>& item)
    {
        return std::make_shared<Item<float>>(std::move(item));
    };

    quadTree<Item<int>*, int>(addFuncPtr, "QuadTree with pointers (int)");
    quadTree<std::shared_ptr<Item<float>>, float>(addFuncSmartPtr, "QuadTree with smart pointers (float)");
    quadTreeSimple("QuadTree simple");

    return 0;
}
