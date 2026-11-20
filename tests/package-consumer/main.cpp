#include <quadtree-index.h>

int main()
{
    const auto index = QuadTree::Index<int, int>::create();
    return index.ok() && index.value().empty() ? 0 : 1;
}
