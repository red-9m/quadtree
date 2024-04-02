# QuadTree C++ implementation
This C++ header library implements QuadTree for rectangular items. The QuadTree can also process with points and lines as a degenerate case of rectangle.

# API
* add() - adds an item (containing a rectangle) to the QuadTree
* remove() - removes on of the previously added items from the QuadTree
* query() - returns a list of items that intersect with given rectangle

# Features
* Implementation takes single header file only: quadtree.h
* Tries to be as fast as possible
* Should fit nice for game object intersection task
* Rectangle coordinates are integers
* Static tree nodes for maximum performance when adding an item
* The QuadTree can contain any class of your item. You must provide a callback to get the item's rectangle
* Two classes to deal with: QuadTree::Tree and QuadTree::Rect. Any compatible class can be used instead of QuadTree::Rect in your item to represent a rectangle (ex. SDL_Rect)

# Constructor
```c++
Tree(const Rect& rect, ItemRectFunc getRect, std::size_t nodeItems = 16, std::size_t depth = 4)
```

* rect - Represents your screen/space
* getRect - function to get you item's rectangle
* nodeItems - Number of items that must be reserved for each node
* depth - Depth of QuadTree (1 - means 5 nodes: root node and 4 children)

# Example
From quadtree-test.cpp:

```c++
struct RectLocal { int x1, y1, w1, h1; };

struct Item
{
    RectLocal rect;
    std::size_t id;
};

...

auto getRect = [](Item* item) -> QuadTree::Rect
{
    return std::bit_cast<QuadTree::Rect>(item->rect);
};

constexpr std::size_t node_items = 1;
constexpr std::size_t depth = 3;

auto tree = QuadTree::Tree<Item*>({0, 0, 100, 100}, getRect, node_items, depth);
std::vector<Item> items{
    {{0, 1, 8, 13},1},
    {{2, 1, 2, 2},2},
    {{3, 3, 4, 0},3}
};

for (auto& item : items)
    tree.add(&item);

tree.query({0,0,3,3}, query_res);
```

# Inspiration code
https://github.com/pvigier/Quadtree.git
