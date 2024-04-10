#pragma once

#include "test-types.h"
#include "quadtree.h"

constexpr auto debugDelim = "-----------------";

template<typename ItemT, typename RectT>
void debugPrintTree(QuadTree::Tree<ItemT, RectT>& tree);
