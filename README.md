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
* Supported integral and floating point rectangle coordinates
* Static tree nodes for maximum performance when adding an item
* The QuadTree can contain any class of your item. You must provide a callback to get the item's rectangle
* Two classes to deal with: QuadTree::Tree and QuadTree::Rect. Any compatible class can be used instead of QuadTree::Rect in your item to represent a rectangle (ex. SDL_Rect)

# Constructor
```c++
Tree(const Rect<RectT>& rect, ItemRectFunc getRect, std::size_t nodeItems = 16, std::size_t depth = 4);
```

* rect - Represents your initial rectangle: screen/space
* getRect - function to get your item's rectangle
* nodeItems - Number of items that must be reserved for each node
* depth - Depth of QuadTree (1 - means 5 nodes: root node and 4 children)

# Example
See quadtree-test.cpp:

```c++
// Most simple implementation in quadtree-test.cpp
void quadTreeSimple();
```

# Inspiration code
https://github.com/pvigier/Quadtree.git
