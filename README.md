# QuadTree C++ implementation
This C++ header library implements QuadTree for rectangular items. The QuadTree can also process with points and lines as a degenerate case of rectangle.

# API
* add()    - Adds an item containing a rectangle (like a Bounding Box for game item) to the QuadTree
* remove() - Removes one of the previously added items from the QuadTree
* query()  - Returns a list of items that intersect with given rectangle

# Features
* Implementation takes single header file only: quadtree.h
* Tries to provide maximum performance
* Should fit nice for game object intersection task
* Supported integral and floating point rectangle coordinates
* The QuadTree can work with any class of your item. You just need to provide a callback to get the item's rectangle
* Two classes to deal with: QuadTree::Tree and QuadTree::Rect
* Any compatible class can be used instead of QuadTree::Rect in your item to represent a rectangle (ex. SDL_Rect)

# Constructor
```c++
constexpr Tree(Rect<RectT> rect, ItemRectFunc getRect, std::size_t nodeItems = 16, std::size_t depth = 4)
```

* rect      - Represents your initial rectangle (screen/space)
* getRect   - Callback function to get your item's rectangle as QuadTree::Rect
* nodeItems - Number of items that must be reserved for each node
* depth     - Depth of QuadTree (1 - means 5 nodes: root node and 4 children)

# Example
See details in quadtree-test.cpp

```c++
void quadTreeSimple()
{
    // Prepare your Item type and getRect() function to extract Rect from Item
    // ....

    // QuadTree Constructor: Use integral(int) rectangle coordinates
    auto tree = QuadTree::Tree<ItemT, int>({0, 0, 100, 100}, getRect, node_items, depth);

    // Prepare items
    std::vector<Item<int>> items{
        {{0, 1, 8, 13}, 1},
        {{2, 1, 2, 2}, 2},
        {{3, 3, 4, 0}, 3}
    };

    // Add items
    for (auto& item : items)
        tree.add(&item);

    // Make a Query to find intersections
    std::vector<ItemT> query_res;
    tree.query({0, 0, 3, 3}, query_res);
}
```

# Inspiration code
https://github.com/pvigier/Quadtree.git
