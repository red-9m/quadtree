#include <chrono>
#include <iostream>
#include <vector>
#include <random>
#include <functional>

#include "test-types.h"
#include "quadtree.h"

#ifdef _DEBUG
#include "quadtree-debug.h"
#endif

template<typename RectT>
bool rectIntersects(const RectLocal<RectT>* rect1, const RectLocal<RectT>* rect2)
{
    return !(
        rect1->w1 == 0 || rect1->h1 == 0 || rect2->w1 == 0 || rect2->h1 == 0 ||
        rect1->x1 >= rect2->x1 + rect2->w1 || rect1->x1 + rect1->w1 <= rect2->x1 ||
        rect1->y1 >= rect2->y1 + rect2->h1 || rect1->y1 + rect1->h1 <= rect2->y1);
}

template<typename RectT>
std::vector<Item<RectT>> generateRandomNodes(std::size_t n)
{
    auto generator = std::default_random_engine();
    auto originDistribution = std::uniform_real_distribution(0.0f, 1.0f);
    auto sizeDistribution = std::uniform_real_distribution(0.1f, 0.04f);
    auto items = std::vector<Item<RectT>>(n);
    for (auto i = std::size_t(0); i < n; ++i)
    {
        items[i].rect.x1 = 100 * originDistribution(generator);
        items[i].rect.y1 = 100 * originDistribution(generator);
        items[i].rect.w1 = std::min(100.0f - items[i].rect.x1, 100.0f * sizeDistribution(generator));
        items[i].rect.h1 = std::min(100.0f - items[i].rect.y1, 100.0f * sizeDistribution(generator));
        items[i].id = i;
    }
    return items;
}

template<typename ItemT, typename RectT>
void quadTree(std::function<ItemT(Item<RectT>&)> addFunc, const std::string& runName)
{
    auto getRect = [](ItemT item) -> QuadTree::Rect<RectT>
    {
        return std::bit_cast<QuadTree::Rect<RectT>>(item->rect);
    };

#ifdef _DEBUG
    constexpr std::size_t node_items = 2;
    constexpr std::size_t depth = 2;
    constexpr std::size_t gen_nodes = 32;
#else
    constexpr std::size_t node_items = 80;
    constexpr std::size_t depth = 4;
    constexpr std::size_t gen_nodes = 10000;
#endif

    auto tree = QuadTree::Tree<ItemT, RectT>({0, 0, 100, 100}, getRect, node_items, depth);
    auto items = generateRandomNodes<RectT>(gen_nodes);
    std::vector<ItemT> query_res;
    query_res.reserve(gen_nodes / 20 + 10);

    auto start1 = std::chrono::steady_clock::now();
    for (auto& item : items)
        tree.add(addFunc(item));

#ifdef _DEBUG
    debugPrintTree<ItemT, RectT>(tree);
#endif

    auto start2 = std::chrono::steady_clock::now();
    QuadTree::Rect<RectT> query_rect{11,10,28,29};
    tree.query(query_rect, query_res);
    auto stop = std::chrono::steady_clock::now();

    auto duration1 = stop - start1;
    auto duration2 = stop - start2;

    std::cout << "Running    : " << runName << std::endl;
    std::cout << "full time  : " << std::chrono::duration_cast<std::chrono::microseconds>(duration1).count() << "µs" << std::endl;
    std::cout << "query only : " << std::chrono::duration_cast<std::chrono::microseconds>(duration2).count() << "µs" << std::endl;
    std::cout << "query items: " << query_res.size() << std::endl;

    query_res.clear();
    auto start3 = std::chrono::steady_clock::now();
    for (auto& item : items)
        if (rectIntersects(&item.rect, (RectLocal<RectT>*)&query_rect))
            query_res.push_back(addFunc(item));
    auto duration3 = std::chrono::steady_clock::now() - start3;
    std::cout << "local rectIntersects result: " << query_res.size() << std::endl;
    std::cout << "local rectIntersects time  : " << std::chrono::duration_cast<std::chrono::microseconds>(duration3).count() << "µs" << std::endl;
}

void quadTreeSimple(const std::string& runName)
{
    auto getRect = [](Item<int>* item) -> QuadTree::Rect<int>
    {
        return std::bit_cast<QuadTree::Rect<int>>(item->rect);
    };

    constexpr std::size_t node_items = 1;
    constexpr std::size_t depth = 3;

    auto tree = QuadTree::Tree<Item<int>*, int>({0, 0, 100, 100}, getRect, node_items, depth);
    std::vector<Item<int>> items{
        {{0, 1, 8, 13},1},
        {{2, 1, 2, 2},2},
        {{3, 3, 4, 0},3}
    };

    for (auto& item : items)
        tree.add(&item);

    std::vector<Item<int>*> query_res;
    query_res.reserve(items.size() / 10 + 5);

#ifdef _DEBUG
    debugPrintTree<Item<int>*, int>(tree);
#endif

    tree.remove(&items[1]);
    tree.remove(&items[2]);

#ifdef _DEBUG
    debugPrintTree<Item<int>*, int>(tree);
#endif

    tree.query({0,0,3,3}, query_res);

    std::cout << "Running    : " << runName << std::endl;
    std::cout << "query items: " << query_res.size() << std::endl;
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

    std::cout << std::endl;
    quadTree<Item<int>*, int>(addFuncPtr, "quadTree with pointers");

    std::cout << std::endl;
    quadTree<std::shared_ptr<Item<float>>, float>(addFuncSmartPtr, "quadTree with smart pointers");

    std::cout << std::endl;
    quadTreeSimple("quadTree simple");

    return 0;
}
