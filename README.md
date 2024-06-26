# QuadTree C++ 20 implementation
This is a high-performance modern C++ header library implementing a QuadTree for rectangular items.

A QuadTree distributes rectangles to quadrants by recursively subdividing a region into four equal parts (quadrants). Each rectangle is assigned to a quadrant based on its position relative to the center point. If a rectangle spans multiple quadrants, it is placed in the node that represents the entire region:
```
 Quadrant Layout:
┌───────┬───────┐
│       │       │
│   1   │   2   │
│       │       │
├───────┼───┬───┤
│       │4.1│4.2│
│   3   ├───┼───┤
│       │4.3│4.4│
└───────┴───┴───┘
```

# API
* add()      - Adds an item containing a rectangle (like a Bounding Box for game item) to the QuadTree
* remove()   - Removes one of the previously added items from the QuadTree
* query()    - Returns a list of items that intersect with given rectangle
* queryAll() - Returns a list of item pairs that contain all intersections between tree items

# Features
* Template based implementation takes single header file only
* Tries to provide maximum performance that should fit nice for game object intersection task
* Supported integral and floating point rectangle coordinates
* Can process points and lines as a degenerate case of rectangle
* The QuadTree can work with any class that represents your item. You just need to provide a callback function to get the item's rectangle
* Two classes to deal with: QuadTree::Tree and QuadTree::Rect. In the item you can use any compatible class instead of QuadTree::Rect (Ex: SDL_Rect)
* Supported compilers: Linux: Clang, GNU(GCC); Windows: MSCV(Microsoft Visual Studio)

# Constructor
```c++
constexpr Tree(Rect<RectT> rect, ItemRectFunc getRect, std::size_t nodeItems = 16, std::size_t depth = 4)
```

* rect      - Represents your initial rectangle (screen/space)
* getRect   - Callback function to get your item's rectangle as QuadTree::Rect
* nodeItems - Number of items that must be reserved for each node
* depth     - Depth of QuadTree. Ex: 1 - means a total of 5 nodes, root node and 4 children

# Example
See details in quadtree-test.cpp: function quadTreeSimple()

```c++
// Prepare your Item type and getRect() function to extract Rect from Item
typedef Item<int>* TreeItemT;
auto getRect = [](const TreeItemT& item) -> const QuadTree::Rect<int>&
{
    return item->rect.quadRect;
};

// QuadTree Constructor: Use integral(int) rectangle coordinates
constexpr std::size_t node_items = 1;
constexpr std::size_t depth = 1;
auto tree = QuadTree::Tree<TreeItemT, int>({0, 0, 100, 100}, getRect, node_items, depth);

// Prepare items
std::vector<Item<int>> items{
    {{{10, 10, 3, 3}}, 1},
    {{{60, 10, 3, 3}}, 2},
    {{{10, 60, 3, 3}}, 3},
    {{{60, 60, 3, 3}}, 4},
    {{{11, 11, 50, 50}}, 5},
    {{{11, 11, 50, 3}}, 6},
    {{{11, 11, 3, 50}}, 7},
};

// Add items
for (auto& item : items)
    tree.add(&item);

// Make a Query to find intersections
std::vector<TreeItemT> query_res;
tree.query({11, 11, 3, 3}, query_res);

```

# Inspiration code
https://github.com/pvigier/Quadtree.git
