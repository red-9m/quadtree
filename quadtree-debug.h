#pragma once

#include "types.h"
#include "quadtree.h"

constexpr auto debugDelim = "-----------------";

template<typename ItemT>
void debugPrintTree(QuadTree::Tree<ItemT>& tree);
