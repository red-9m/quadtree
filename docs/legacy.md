# Legacy `QuadTree::Tree`

This API lives in [`quadtree.h`](../quadtree.h) and is separate from
`QuadTree::Index`. It is not installed by the runtime CMake package. Do not
include the two API headers in one translation unit. For new code, see the
[runtime quick start](../README.md); for existing users, see the
[migration guide](../INDEX.md#migration).

`Tree` subdivides a fixed root into quadrants and keeps rectangles that span
quadrants in a containing node. It supports integral and floating-point
rectangles, including points and line segments.

```cpp
constexpr Tree(Rect<RectT> rect, ItemRectFunc getRect,
               std::size_t nodeItems = 16, std::size_t depth = 4)
```

- `rect` is the root rectangle. `nodeItems` reserves item storage per node.
- `getRect` returns a `const QuadTree::Rect<RectT>&` for an item. Convert a
  foreign rectangle by value into a stored `QuadTree::Rect`; do not cast.
- `depth` sets the subdivision depth. Depth 1 gives a root and four children.
- `add(item)` returns `false` without inserting if the rectangle is outside the
  root. Matching the root edges is allowed.
- `remove(item)` removes a previously added item. `query(rect, items)` and
  `queryAll(pairs)` append matching items or intersecting pairs to their vectors.

## Changing geometry

Keep an item's rectangle unchanged while it is in the tree. Remove it using its
old bounds before changing the rectangle, then reinsert and check the result.
Changing the rectangle first can send removal to the wrong node.

```cpp
tree.remove(&item);
item.rect = newRect;
if (!tree.add(&item)) {
    // The new bounds lie outside the root; the item is no longer indexed.
}
```

See [`quadTreeSimple()` in quadtree-test.cpp](../quadtree-test.cpp) for the legacy
example (`quadtree-test` CMake target, enabled by `QUADTREE_BUILD_DEMO`).

Original inspiration: [pvigier/Quadtree](https://github.com/pvigier/Quadtree).
